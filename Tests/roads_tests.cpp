// The roads (RatwRoads.h): towns and their stores, caravans that carry real goods, bandits who take them, contracts,
// and rumours. A strip of nine cells: the capital "east" at one end, "west" at the other, wild country between.
#include "RatwCheckpoint.h"
#include "RatwItems.h"
#include "RatwWire.h"
#include "RatwWorld.h"
#include "battle_play.h"

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

constexpr int Side = 16;
std::string id(int i) { return "c_" + std::to_string(i) + "_0"; }

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
      << "\"female\" \"average\" \"saddle\" 3 1 5 1 1 8 17 \"-\" 40 0 1 \"" << id(home) << "\" " << hx << " 4.5 \"" << id(at)
      << "\" " << wx << " 8.5 \"" << id(home) << "\" " << hx << " 5.5\n";
    return r.str();
}

// A strip of cells in a row, one letter each: 'E' the capital "east", 'M' a town "mid", 'W' the town "west", 'F'
// Frosthollow (a town with a trade, Data/Economy/specialties.json: its people and shops are the test's own), '.' wild
// country. Each town's first cell is where its people live, its second its market.
Fixture strip(const std::string& layout = "EE.....WW", const std::string& extra = "")
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
    const auto region = [](char c) {
        return c == 'E' ? "east" : c == 'M' ? "mid" : c == 'W' ? "west" : c == 'F' ? "frosthollow" : "wilds";   // (F: a town with a trade.)
    };
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
    // East's first cell is 0; mid's and west's are wherever their letters start.
    const auto first = [&](char c) { return int(layout.find(c)); };
    const std::map<char, std::pair<std::string, std::string>> merchants = {
        {'E', {"em", "Ember Oak"}}, {'M', {"mm", "Moss Hale"}}, {'W', {"wm", "Wren Reed"}}};
    for (const auto& [c, who] : merchants)
    {
        const int home = first(c);
        if (home < 0)
            continue;
        const std::string tag = c == 'E' ? "e" : c == 'M' ? "m" : "w";
        m << resident(who.first, who.second, "merchant", "keeping the stall", home, 2.5, home + 1, 8.5);
        for (int n = 1; n <= 5; ++n)
            m << resident(tag + std::to_string(n), std::string(c == 'E' ? "East" : c == 'M' ? "Mid" : "West") + " " + std::to_string(n), "civilian", "working",
                          home, 3.5 + n, home, 3.5 + n);
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

// (A business's goods are its till's since doc 46's Phase 2: tillOf, the account itself for anyone else.)
int stock(const World& w, const std::string& account, const std::string& item)
{
    const auto* a = w.society().account(w.society().tillOf(account));
    return a ? Society::stock(*a, item) : 0;
}

const Contract* find(const World& w, const std::string& kind)
{
    for (const auto& c : w.roads().contracts)
        if (c.kind == kind && (c.status == "open" || c.status == "taken"))
            return &c;
    return nullptr;
}

void run(World& w, double seconds)
{
    for (double t = 0; t < seconds; t += 1)
        w.tick(1);
}

void nextMorning(World& w)
{
    auto saved = w.save();
    saved.calendarDays = std::floor(saved.calendarDays) + 1.3;     // Early in the morning of the next day.
    expect(w.restore(saved).ok, "A day passes");
    w.tick(.6);
}

// A player who keeps beside a caravan's wagon, a step behind it, until it arrives where it was going.
bool walkWith(World& w, const std::string& player, const std::string& trip, int seconds)
{
    for (int t = 0; t < seconds; ++t)
    {
        if (const auto* wagon = w.entity("road:" + trip))
        {
            auto* p = w.entity(player);
            p->cellId = wagon->cellId;
            p->position = {std::max(.6, wagon->position.x - 1), wagon->position.y};
        }
        w.tick(1);
        for (const auto& e : w.takeEvents())
            if (e.kind == "caravan arrives" && e.actor == trip)
                return true;
    }
    return false;
}

void townsAndCaravans()
{
    auto f = strip();
    auto w = load(f);
    const int treasuryMeals = stock(w, "treasury", "meal");
    w.tick(.6);                                  // The first schedule update: towns are found, the day begins.
    expect(w.towns().size() == 2 && w.towns()[0].id == "east" && w.towns()[1].id == "west", "Two towns, the capital first");
    expect(w.towns()[0].store == "treasury" && w.towns()[1].store == "stores:west", "West has its own stores");
    expect(stock(w, "stores:west", "meal") > 0 && stock(w, "treasury", "meal") < treasuryMeals, "stocked from the treasury");
    expect(w.townOf(id(8)) && w.townOf(id(8))->id == "west" && !w.townOf(id(4)), "Wild country is no town");
    expect(w.roads().caravans.size() == 1 && w.roads().caravans[0].to == "west" && w.roads().caravans[0].route.size() == 8,
           "A caravan sets out for west along the road");
    const auto load = stock(w, w.roads().caravans[0].account, "meal");
    expect(load > 0, "carrying real goods");
    expect(w.society().conserved(), "Money is conserved");
    w.roads().camps.clear();                     // No bandits this time.
    w.takeEvents();
    w.believe("em", "e1", "owes half the market", "overheard", .9);   // News in the capital's market.
    run(w, 7 * 150 + 20);
    int delivered = 0;
    bool arrived = false;
    for (const auto& e : w.takeEvents())
    {
        if (e.kind == "economy" && e.detail == "caravan delivered" && e.item == "meal" && e.target == "stores:west")
            delivered += e.quantity;
        arrived |= e.kind == "caravan arrives" && e.target == "west";
    }
    expect(arrived && delivered == load, "It arrives, and its whole load goes into west's stores (" +
                                             std::to_string(delivered) + " of " + std::to_string(load) + ")");
    expect(w.beliefsOf("wm") && w.rumoursAbout("wm", "e1", "East 1").find("owes half the market") != std::string::npos,
           "and the carters told the west market what they heard: " + w.rumoursAbout("wm", "e1", "East 1"));
    run(w, 7 * 150 + 20);
    expect(w.roads().caravans.empty(), "Back home empty, the caravan is done");
    expect(w.society().conserved(), "Money is still conserved");
}

void banditsAndContracts()
{
    auto f = strip();
    auto w = load(f);
    w.addPlayer("player-ada", "Ada");
    w.tick(.6);
    expect(!w.roads().caravans.empty(), "A caravan on the road");
    // A strong, starving camp in the middle of the road.
    w.roads().camps = {{"camp_mid", id(4), 20, 100, -100, true}};
    const auto load = stock(w, w.roads().caravans[0].account, "meal");
    run(w, 4 * 150 + 20);
    expect(w.roads().caravans.empty() || w.roads().caravans[0].status == "raided", "The bandits rob it");
    const auto* bounty = find(w, "bounty");
    const auto* escort = find(w, "escort");
    expect(bounty && bounty->target == "camp_mid" && bounty->reward > 0 && bounty->town == "west",
           "West puts a price on the camp");
    expect(escort && escort->town == "west", "and asks for guards for the next caravan");
    expect(w.rumoursAbout("wm", "camp_mid", "The bandits").find("raids the road") != std::string::npos,
           "The west market hears of it: " + w.rumoursAbout("wm", "camp_mid", "The bandits"));
    expect(w.roads().camps[0].hunger < 100 - load * 3, "The loot feeds the bandits");
    expect(w.society().conserved(), "The reward was set aside, not made");

    // Ada, in west, takes the escort. The camp is gone by the next caravan (the watch), but she has to be there.
    auto* ada = w.entity("player-ada");
    ada->cellId = id(8);
    ada->position = {8.5, 8.5};
    const auto near = w.contractsNear("player-ada");
    expect(!near.empty(), "Work is to be had in west");
    const std::string escortId = escort->id;
    expect(w.takeContract("player-ada", escortId).ok, "Ada takes the escort");
    expect(!w.takeContract("player-ada", escortId).ok, "Once");
    w.roads().camps.clear();
    w.roads().caravans.clear();                 // (The robbed one, going home.)
    const auto purse = w.society().account("player-ada")->cash;
    // She goes to the capital's market to meet it.
    ada->cellId = id(1);
    ada->position = {8.5, 10.5};
    w.takeNotices();
    nextMorning(w);
    expect(!w.roads().caravans.empty() && w.roads().caravans.back().escorts == std::vector<std::string>{"player-ada"},
           "The next caravan counts her among its escorts");
    bool told = false;
    for (const auto& [who, words] : w.takeNotices())
        told |= who == "player-ada" && words.find("making ready") != std::string::npos;
    expect(told, "and she is told it is making ready");
    const auto* wagon = w.entity("road:" + w.roads().caravans.back().id);
    expect(wagon && wagon->transient && !wagon->offstage && wagon->cellId == id(1), "Its wagon stands in the market, in person");
    // She walks beside it all the way.
    const std::string trip = w.roads().caravans.back().id;
    const bool arrived = walkWith(w, "player-ada", trip, 900);
    expect(arrived, "It reaches west with her");
    expect(w.society().account("player-ada")->cash == purse + 12, "and she is paid for guarding it");
    expect(w.society().conserved(), "Money is conserved");
    // The wagon is never saved as itself: a restart brings it back from the caravan.
    for (const auto& npc : w.save().npcs)
        expect(npc.id.rfind("road:", 0) != 0, "Road folk are not saved as characters");
}

void anEscortWhoIsntThere()
{
    auto f = strip();
    auto w = load(f);
    w.addPlayer("player-ada", "Ada");
    w.tick(.6);
    w.roads().camps.clear();
    w.roads().caravans.clear();
    auto* ada = w.entity("player-ada");
    ada->cellId = id(8);
    ada->position = {8.5, 8.5};
    auto& job = w.postContract("escort", "treasury", "west", "west", 12, 7, "guarding the next caravan to west");
    const std::string jobId = job.id;
    expect(w.takeContract("player-ada", jobId).ok, "Ada takes an escort");
    const auto purse = w.society().account("player-ada")->cash;
    const auto treasury = w.society().account("treasury")->cash;
    nextMorning(w);
    const std::string trip = w.roads().caravans.back().id;
    run(w, 60);
    expect(w.entity("road:" + trip) && w.entity("road:" + trip)->cellId == id(1), "The caravan waits for her");
    // She never comes: it goes without her after two game hours, and gets there.
    bool arrived = false;
    for (int t = 0; t < 4000 && !arrived; ++t)
    {
        w.tick(1);
        for (const auto& e : w.takeEvents())
            arrived |= e.kind == "caravan arrives" && e.actor == trip;
    }
    expect(arrived, "It goes without her in the end");
    expect(w.society().account("player-ada")->cash == purse, "She is not paid for a road she didn't walk");
    bool returned = false;
    for (const auto& k : w.roads().contracts)
        returned |= k.id == jobId && k.status == "expired";
    expect(returned && treasury > 0, "The pay goes back");
    bool heard = false;
    for (const auto& [who, words] : w.takeNotices())
        heard |= who == "player-ada" && words.find("without you") != std::string::npos;
    expect(heard, "and she hears so");
}

void couriersAndSupplies()
{
    auto f = strip();
    auto w = load(f);
    w.addPlayer("player-ada", "Ada");
    w.tick(.6);
    // Ember in the east market pays for a letter to Wren in the west.
    const auto emberBefore = w.society().account("em")->cash;
    auto& letter = w.postContract("courier", "em", "west", "wm", 5, 10, "a letter from Ember to Wren");
    const std::string letterId = letter.id;
    expect(letter.reward == 5 && w.society().account("em")->cash == emberBefore - 5, "The reward is set aside from her purse");
    auto* ada = w.entity("player-ada");
    ada->cellId = id(1);
    ada->position = {8.5, 8.5};
    expect(w.takeContract("player-ada", letterId).ok, "Ada takes the letter in the east");
    const auto purse = w.society().account("player-ada")->cash;
    // She walks it west and stands before Wren.
    const auto* wren = w.entity("wm");
    ada->cellId = wren->cellId;
    ada->position = {wren->position.x + 1, wren->position.y};
    w.tick(.6);
    expect(w.society().account("player-ada")->cash == purse + 5, "Delivered in person: she is paid");
    expect(w.bonds().find("wm", "player-ada") && w.bonds().find("wm", "player-ada")->trust > 0, "and Wren trusts her a little");
    // A supply run: food for west, done by selling herbs to west's merchant.
    auto& run = w.postContract("supply", "treasury", "west", "west", 15, 7, "bringing food to west's market");
    const std::string supplyId = run.id;
    ada->cellId = id(8);
    expect(w.takeContract("player-ada", supplyId).ok, "Ada takes the supply run");
    w.society().shift("treasury", "player-ada", "herbs", 2, 0, "test: herbs to sell");
    ada->position = {w.entity("wm")->position.x, w.entity("wm")->position.y + 1};
    const auto sold = w.trade("player-ada", "wm", "herbs", 1, false);
    bool done = false;
    for (const auto& c : w.roads().contracts)
        done |= c.id == supplyId && c.status == "done";
    expect(sold.ok && done, "Selling to the west merchant finishes the supply run (" + sold.message + ")");
    expect(w.society().conserved(), "Money is conserved");
}

// Bandits of this camp standing in the world now.
std::vector<const Entity*> bandits(const World& w, const std::string& camp)
{
    std::vector<const Entity*> out;
    for (const auto& [eid, e] : w.entities())
        if (eid.rfind("road:" + camp + ":", 0) == 0 && !e.dead)
            out.push_back(&e);
    return out;
}

bool heard(World& w, const std::string& who, const std::string& words)
{
    bool found = false;
    for (const auto& [to, text] : w.takeNotices())
        found |= to == who && text.find(words) != std::string::npos;
    return found;
}

void banditsInPerson()
{
    auto f = strip();
    auto w = load(f);
    w.addPlayer("player-ada", "Ada");
    w.tick(.6);
    w.roads().caravans.clear();
    w.roads().camps = {{"camp_mid", id(4), 9, 60, -100, true}};
    run(w, 2);
    expect(bandits(w, "camp_mid").empty(), "Nobody near: the bandits are only numbers");
    auto* ada = w.entity("player-ada");
    ada->cellId = id(3);
    ada->position = {8.5, 8.5};
    run(w, 2);
    expect(bandits(w, "camp_mid").size() == 3, "Someone near: three of them in person (" +
                                                 std::to_string(bandits(w, "camp_mid").size()) + ")");
    expect(!w.banditDemand("player-ada"), "Not yet on their road, she is not stopped");
    // Into their cell.
    ada->cellId = id(4);
    ada->position = {w.roads().camps[0].x - 3, w.roads().camps[0].y};
    w.takeNotices();
    run(w, 2);
    const auto demand = w.banditDemand("player-ada");
    expect(demand > 0, "They stop her and ask for her purse");
    expect(heard(w, "player-ada", "Your purse"), "in words she is shown");
    const auto purse = w.society().account("player-ada")->cash;
    const auto* one = bandits(w, "camp_mid").front();
    const auto paid = w.payBandits("player-ada", one->id);
    expect(paid.ok && w.society().account("player-ada")->cash == purse - demand, "She pays: " + paid.message);
    expect(w.society().account("bandits:camp_mid") && w.society().account("bandits:camp_mid")->cash == demand,
           "and the camp has it");
    expect(!w.banditDemand("player-ada"), "They let her go");
    run(w, 30);
    expect(!w.banditDemand("player-ada"), "and leave her be for a while");
    expect(w.society().conserved(), "Money is conserved");
    // She goes away; they are numbers again.
    ada->cellId = id(8);
    run(w, 2);
    expect(bandits(w, "camp_mid").empty(), "Nobody near again: gone from the world");
    for (const auto& npc : w.save().npcs)
        expect(npc.id.rfind("road:", 0) != 0, "never saved as characters");
}

// Bandits creeping up on a traveller who hasn't seen them (doc 40): crouched, unseen, and the fight begins with her taken
// unawares; downwind of them, she smells them, so they step out and ask instead.
void banditsCreepUp()
{
    for (const bool downwind : {false, true})
    {
        auto f = strip();
        auto w = load(f);
        w.addPlayer("player-ada", "Ada");
        w.tick(.6);
        w.roads().caravans.clear();
        w.roads().camps = {{"camp_mid", id(4), 9, 60, -100, true}};
        if (auto* c = w.cell(id(4)))                            // Still air; or, the second time, blowing from them to her.
            c->wind = downwind ? Wind{std::acos(-1.0), .6, false} : Wind{0, 0, false};
        auto* ada = w.entity("player-ada");
        ada->cellId = id(3);
        ada->position = {8.5, 8.5};
        run(w, 2);
        ada->cellId = id(4);
        const auto& camp = w.roads().camps[0];
        ada->position = {camp.x - 9, camp.y};
        ada->facing = std::acos(-1.0);                          // Facing west: her back to the camp.
        w.takeNotices();
        bool asked = false;
        for (int i = 0; i < 400 && !w.inBattle("player-ada") && !asked; ++i)
        {
            ada->velocity = {};
            ada->facing = std::acos(-1.0);
            w.tick(.25);
            for (const auto& [to, text] : w.takeNotices())
                if (to == "player-ada")
                    asked = asked || text.find("Your purse") != std::string::npos;
        }
        if (downwind)
        {
            expect(asked, "downwind of them, she has smelt them: they step out and ask for her purse");
            continue;
        }
        expect(!asked, "her back to them: no one steps out to ask for her purse");
        expect(w.inBattle("player-ada"), "the bandits reach her and the fight begins");
        const auto* b = w.battleOf("player-ada");
        const auto* her = b ? b->fighter("player-ada") : nullptr;
        expect(her && her->meter < 10 && std::any_of(b->log.begin(), b->log.end(), [](const BattleLine& l) { return l.kind == "ambush"; }),
               "never having seen them: taken unawares, her bar empty");
    }
}

// Keep watch (doc 53, 4): Ada forages with her back to the camp while Bo keeps watch facing it; the creeping bandits
// must get past him too. He sees them, so they can't take her unawares.
void aWatcherCatchesTheCreep()
{
    for (const bool watching : {true, false})
    {
        auto f = strip();
        auto w = load(f);
        w.addPlayer("player-ada", "Ada");
        w.addPlayer("player-bo", "Bo");
        w.tick(.6);
        w.roads().caravans.clear();
        w.roads().camps = {{"camp_mid", id(4), 9, 60, -100, true}};
        auto* c = w.cell(id(4));
        c->wind = Wind{0, 0, false};
        auto* ada = w.entity("player-ada");
        auto* bo = w.entity("player-bo");
        ada->cellId = bo->cellId = id(3);
        ada->position = bo->position = {8.5, 8.5};
        run(w, 2);
        const auto& camp = w.roads().camps[0];
        ada->cellId = bo->cellId = id(4);
        ada->position = {camp.x - 9, camp.y};
        bo->position = {camp.x - 8.5, camp.y + 1.5};
        for (int dy = -2; dy <= 2; ++dy)                // (Grass to forage where she stands.)
            for (int dx = -2; dx <= 2; ++dx)
                if (auto* t = const_cast<Tile*>(c->tile(int(ada->position.x) + dx, int(ada->position.y) + dy)))
                    t->glyph = ',';
        expect(w.forage("player-ada").ok && w.lendAPaw("player-bo", "player-ada").ok, "Ada forages, Bo lends a paw");
        if (watching)
            expect(w.keepWatch("player-bo", true).ok && w.watchersOver("player-ada") == std::vector<std::string>{"player-bo"}, "and keeps watch over her");
        w.takeNotices();
        bool asked = false;
        for (int i = 0; i < 400 && !w.inBattle("player-ada") && !asked; ++i)
        {
            ada->velocity = bo->velocity = {};
            ada->facing = std::acos(-1.0);              // Her back to the camp;
            bo->facing = 0;                             // his eyes on it.
            if (i % 16 == 0)
                w.forage("player-ada");
            w.tick(.25);
            for (const auto& [to, text] : w.takeNotices())
                if (to == "player-ada")
                    asked = asked || text.find("Your purse") != std::string::npos;
        }
        // Seen creeping, they rise and rush (or, seen before, step out and ask): either way, not taken unawares.
        const auto* b = w.battleOf("player-ada");
        const bool ambushed = b && std::any_of(b->log.begin(), b->log.end(), [](const BattleLine& l) { return l.kind == "ambush"; });
        if (!watching)
        {
            expect(ambushed, "(not keeping watch, his eyes count for nothing: she is taken unawares)");
            continue;
        }
        expect((asked || b) && !ambushed, std::string("Bo saw them coming: she isn't taken unawares (asked ") + (asked ? "yes" : "no") + ", fight " +
                                              (b ? "yes" : "no") + ", ambushed " + (ambushed ? "yes" : "no") + ")");
    }
}

void aFight()
{
    auto f = strip();
    auto w = load(f);
    w.addPlayer("player-ada", "Ada");
    w.tick(.6);
    w.roads().caravans.clear();
    w.roads().camps = {{"camp_mid", id(4), 9, 60, -100, true}};
    const auto& bounty = w.postContract("bounty", "treasury", "west", "camp_mid", 30, 20, "the bandits at the ford");
    const std::string bountyId = bounty.id;
    auto* ada = w.entity("player-ada");
    ada->strength = 90;
    ada->dexterity = 90;
    ada->cellId = id(3);
    ada->position = {8.5, 8.5};
    run(w, 2);
    expect(!w.attack("player-ada", "npc_nobody").ok, "Nothing to fight that isn't there");
    // She walks up to the leader and goes for it: a fight in an arena (Docs/Design/33-combat.md), the whole band in it.
    const auto purse = w.society().account("player-ada")->cash;
    ada->cellId = id(4);
    const auto* leader = w.entity("road:camp_mid:0");
    expect(leader, "The camp's leader is on the road");
    ada->position = {std::max(.6, leader->position.x - 1), leader->position.y};
    const auto started = w.attack("player-ada", leader->id);
    expect(started.ok && w.inBattle("player-ada"), "A fight begins: " + started.message);
    expect(w.battleOf("player-ada")->fighters.size() >= 2 && !w.battleOf("player-ada")->camp.empty(), "against the band");
    bool cleared = false;
    for (int t = 0; t < 6000 && !cleared; ++t)
    {
        auto* me = w.entity("player-ada");
        me->stamina = std::max(me->stamina, 60.0);          // A strong fighter (and the test isn't about losing).
        me->hurt = 0;
        test::playTurn(w, "player-ada");
        w.tick(.25);
        cleared = !w.roads().camps[0].active;
    }
    expect(cleared, "She downs the leader, the rest run, and the camp is broken");
    bool paid = false;
    for (const auto& k : w.roads().contracts)
        paid |= k.id == bountyId && k.status == "done";
    expect(paid && w.society().account("player-ada")->cash >= purse + 30, "The bounty is hers");
    // (Word of it travels as a deed, from those who saw it: doc 56. The west market doesn't hear of it at once.)
    expect(w.rumoursAbout("wm", "player-ada", "Ada").find("drove the bandits off") == std::string::npos,
           "and the west market doesn't hear of it at once: " + w.rumoursAbout("wm", "player-ada", "Ada"));
    bool recorded = false;
    for (const auto& e : w.takeEvents())
        recorded |= e.kind == "camp cleared" && e.actor == "player-ada";
    expect(recorded, "the camp's breaking is recorded, for the game's deed");
    expect(w.society().conserved(), "Money is conserved");
}

void beatenAndRobbed()
{
    auto f = strip();
    auto w = load(f);
    w.addPlayer("player-ada", "Ada");
    w.tick(.6);
    w.roads().caravans.clear();
    w.roads().camps = {{"camp_mid", id(4), 9, 60, -100, true}};
    auto* ada = w.entity("player-ada");
    ada->cellId = id(3);
    ada->position = {8.5, 8.5};
    run(w, 2);
    ada->cellId = id(4);
    ada->position = {w.roads().camps[0].x - 3, w.roads().camps[0].y};
    ada->stamina = 30;
    w.society().shift("treasury", "player-ada", "", 0, 80, "test: a fuller purse");
    const auto purse = w.society().account("player-ada")->cash;
    // She neither pays nor leaves: they lose patience, and it is a fight; she lets every turn go by.
    bool beaten = false;
    for (int t = 0; t < 900 && !beaten; ++t)
    {
        w.entity("player-ada")->stamina = std::min(w.entity("player-ada")->stamina, 30.0);
        w.tick(1);
        for (const auto& e : w.takeEvents())
            beaten |= e.kind == "robbed" && e.target == "player-ada";
    }
    expect(beaten, "They beat her to the ground");
    const auto now = w.society().account("player-ada")->cash;
    expect(now < purse && now >= purse / 2 - 1, "and take half her purse (" + std::to_string(purse) + " -> " + std::to_string(now) + ")");
    expect(!w.entity("player-ada")->dead && w.entity("player-ada")->downedLeft > 0, "but she lives, Downed");
    expect(!w.banditDemand("player-ada"), "They leave her be");
    expect(w.society().conserved(), "Money is conserved");
}

void banditsCalled()
{
    // The Dungeon Master calls a bandit up near a traveller (doc 33): a camp of one, a few strides off, out at once.
    auto f = strip();
    auto w = load(f);
    w.addPlayer("player-ada", "Ada");
    w.tick(.6);
    w.roads().caravans.clear();
    auto* ada = w.entity("player-ada");
    ada->cellId = id(5);
    ada->position = {8.5, 8.5};
    run(w, 1);
    expect(!w.callBandits("nobody", 1).ok, "Only near someone");
    const auto called = w.callBandits("player-ada", 1);
    expect(called.ok, "One bandit called: " + called.message);
    run(w, 2);
    const auto near = bandits(w, called.targetId);
    expect(near.size() == 1, "One comes out");
    const double gap = std::hypot(near[0]->position.x - ada->position.x, near[0]->position.y - ada->position.y);
    expect(gap >= 2 && gap <= 12, "a few strides off: " + std::to_string(gap));
    ada->position = {near[0]->position.x - 1, near[0]->position.y};
    const auto fight = w.attack("player-ada", near[0]->id);
    expect(fight.ok && w.inBattle("player-ada"), "and can be fought: " + fight.message);
}

void residentsTakeWork()
{
    // A guard in the capital, and someone there out of work.
    auto f = strip("EE.....WW", resident("eg", "Holt Vane", "guard", "keeping watch", 0, 12.5, 1, 12.5) +
                                    resident("ex", "Bram Dell", "civilian", "-", 0, 13.5, 0, 13.5));
    auto w = load(f);
    w.tick(.6);
    expect(!w.society().jobOf("ex"), "Bram has no work");
    w.roads().camps.clear();
    w.roads().caravans.clear();
    // A letter nobody has taken for a couple of days.
    auto& letter = w.postContract("courier", "em", "west", "wm", 5, 10, "a letter from Ember to Wren");
    const std::string letterId = letter.id;
    auto& escort = w.postContract("escort", "treasury", "west", "west", 12, 7, "guarding the next caravan to west");
    const std::string escortId = escort.id;
    auto saved = w.save();
    saved.calendarDays += 2.05;
    expect(w.restore(saved).ok, "Two days pass");
    w.tick(.6);
    const Contract* carried = nullptr;
    const Contract* guarded = nullptr;
    for (const auto& k : w.roads().contracts)
    {
        if (k.id == letterId) carried = &k;
        if (k.id == escortId) guarded = &k;
    }
    expect(carried && carried->status == "taken" && carried->taker == "ex", "Bram takes the letter (" +
                                                                                 (carried ? carried->taker : "") + ")");
    expect(guarded && guarded->status == "taken" && guarded->taker == "eg", "and the town guard the escort");
    const auto bram = w.society().account("ex")->cash;
    const auto holt = w.society().account("eg")->cash;
    // They go, in person: the letter to Wren, the guard with the caravan.
    bool delivered = false, arrived = false;
    for (int t = 0; t < 6000 && !(delivered && arrived); ++t)
    {
        w.tick(1);
        for (const auto& e : w.takeEvents())
        {
            delivered |= e.kind == "contract done" && e.actor == "ex";
            arrived |= e.kind == "caravan arrives";
        }
    }
    expect(delivered && w.society().account("ex")->cash >= bram + 5, "Bram hands Wren the letter and is paid");
    expect(arrived, "The caravan arrives");
    expect(w.society().account("eg")->cash >= holt + 12, "and Holt, who walked with it, is paid");
    expect(w.society().conserved(), "Money is conserved");
}

void tradeAndPrices()
{
    auto f = strip("EE...MM....WW");
    auto w = load(f);
    w.addPlayer("player-ada", "Ada");
    w.tick(.6);
    expect(w.towns().size() == 3, "Three towns");
    w.roads().camps.clear();
    w.roads().caravans.clear();
    // Mid has plenty; west has nothing left.
    const int westMeals = stock(w, "stores:west", "meal");
    w.society().consume("stores:west", "meal", westMeals, "test: a bad winter");
    w.society().shift("treasury", "stores:west", "", 0, 200, "test: west has money");
    // Dearer in west than in the capital, within the hour.
    run(w, 700);
    auto* ada = w.entity("player-ada");
    w.society().shift("treasury", "player-ada", "", 0, 100, "test: purse");
    const auto east = w.society().quote("player-ada", "em", "meal", 1, true).unitPrice;
    const auto west = w.society().quote("player-ada", "wm", "meal", 1, true).unitPrice;
    expect(west > east, "Food costs more where the stores are empty (" + std::to_string(west) + " against " + std::to_string(east) + ")");
    (void)ada;
    // (Its own harvest: the capital's stores may be bought out by now, with everyone's starting money to spend.)
    expect(w.society().create("stores:mid", "meal", 40, "test: a good harvest"), "Mid's good harvest");
    const auto midCash = w.society().account("stores:mid")->cash;
    nextMorning(w);
    const Caravan* trade = nullptr;
    for (const auto& c : w.roads().caravans)
        if (c.from == "mid" && c.to == "west")
            trade = &c;
    std::string seen;
    for (const auto& c : w.roads().caravans)
        seen += c.from + ">" + c.to + " ";
    expect(trade && stock(w, trade->account, "meal") > 0, "Mid sends food to west (" + seen + "; mid " +
                                                              std::to_string(stock(w, "stores:mid", "meal")) + ", west " +
                                                              std::to_string(stock(w, "stores:west", "meal")) + ")");
    const std::string trip = trade->id;
    const int load = stock(w, trade->account, "meal");
    const int before = stock(w, "stores:west", "meal");
    // A storm over the road: the caravan waits it out where it stands (Phase 9), then goes on.
    for (const auto& [id, c] : w.cells())
        if (c.outdoors)
            w.setWeather(id, Weather::Storm);
    const auto legBefore = trade->leg;
    for (int t = 0; t < 300; ++t)
        w.tick(1);
    const Caravan* waiting = nullptr;
    for (const auto& c : w.roads().caravans)
        if (c.id == trip)
            waiting = &c;
    expect(waiting && waiting->leg == legBefore && w.entity("road:" + trip) &&
               w.entity("road:" + trip)->activity == "waiting out the weather",
           "A caravan waits out a storm");
    for (const auto& [id, c] : w.cells())
        if (c.outdoors)
            w.setWeather(id, Weather::Clear);
    bool arrived = false;
    std::int64_t paid = 0;
    for (int t = 0; t < 4000 && !arrived; ++t)
    {
        w.tick(1);
        for (const auto& e : w.takeEvents())
        {
            arrived |= e.kind == "caravan arrives" && e.actor == trip;
            if (e.kind == "economy" && e.detail == "goods from mid" && e.actor == "stores:west" && e.target == "stores:mid")
                paid += e.coins;
        }
    }
    expect(arrived && stock(w, "stores:west", "meal") >= before + load - 2, "It arrives with west's food");
    // (Mid's own purse pays its wages now, doc 42, so its cash alone doesn't show it.)
    expect(paid > 0, "and west pays mid for it");
    (void)midCash;
    expect(w.society().conserved(), "Money is conserved");
}

void rumoursSpread()
{
    auto f = strip();
    auto w = load(f);
    w.tick(.6);
    w.bonds().change("e1", "e2", {10, 10, 60, 0, 0}, w.calendarDays());
    w.believe("e1", "player-ada", "breaks promises", "was let down", .9);
    auto saved = w.save();
    saved.calendarDays += 1;
    expect(w.restore(saved).ok, "A day passes");
    w.tick(.6);
    const auto heard = w.rumoursAbout("e2", "player-ada", "Ada");
    expect(heard.find("Ada breaks promises") != std::string::npos && heard.find("from East 1") != std::string::npos,
           "e2 heard it from e1: " + heard);
    double first = 0, second = 0;
    for (const auto& b : *w.beliefsOf("e1")) first = b.confidence;
    for (const auto& b : *w.beliefsOf("e2")) if (b.subject == "player-ada") second = b.confidence;
    expect(second < first, "less sure than the one who told it");
    // Saved and restored with the rest of the roads.
    World again = load(f);
    expect(again.restore(w.save()).ok && again.rumoursAbout("e2", "player-ada", "Ada") == heard, "Rumours survive a restart");
}
} // namespace

// Shops (Docs/Design/39): each sells a handful of its kind's cheap goods, has them on its shelves from the start, and
// makes more as they sell; food shops keep meals, a smith swords, an herbalist herbs.
void shopsSellTheirGoods()
{
    auto f = strip("EE", resident("baker1", "Bram Loaf", "merchant", "baker at The Amber Loaf", 0, 4.5, 1, 4.5) +
                             resident("baker2", "Tilly Crust", "merchant", "baker at The Morning Oven", 0, 5.5, 1, 5.5) +
                             resident("smith1", "Ivo Anvil", "merchant", "smith at The Gate Forge", 0, 6.5, 1, 6.5) +
                             resident("herb1", "Sage Root", "merchant", "herbalist at Valley Remedies", 0, 7.5, 1, 7.5) +
                             resident("inn1", "Mabel Cup", "merchant", "innkeeper at The Hollow Cup", 0, 8.5, 1, 8.5));
    auto w = load(f);
    const auto& soc = w.society();
    const auto bread = soc.wares("baker1");
    expect(std::find(bread.begin(), bread.end(), "meal") != bread.end(), "A baker keeps meals for the town to eat");
    int goods = 0;
    for (const auto& ware : bread)
        if (ware != "meal")
        {
            ++goods;
            const auto* good = items::good(ware);
            expect(good && good->price <= Society::CheapPrice, "and sells only cheap goods: " + ware);
            expect(stock(w, "baker1", ware) == Society::GoodsKept, "with some of each on the shelves from the start: " + ware);
        }
    expect(goods >= 1 && goods <= 6, "a handful of the bakery's goods (" + std::to_string(goods) + ")");
    expect(soc.wares("baker1") == bread, "the same handful every day");
    const auto iron = soc.wares("smith1");
    expect(iron.front() == "sword" && std::find(iron.begin(), iron.end(), "nails") != iron.end() &&
               std::find(iron.begin(), iron.end(), "meal") == iron.end(),
           "A smith sells the swords and nails it forges (Data/Items/crafts.json), and no meals");
    const auto herbs = soc.wares("herb1");
    expect(std::find(herbs.begin(), herbs.end(), "herbs") != herbs.end(), "An herbalist sells herbs");
    expect(soc.wares("inn1") == std::vector<std::string>({"meal", "herbs"}), "An innkeeper deals in meals and herbs as ever");
    // Bought, and made again at work.
    std::string ware;
    for (const auto& x : bread)
        if (x != "meal")
            ware = x;
    auto& ada = w.addPlayer("player-ada", "Ada");
    ada.cellId = f.cells.begin()->first;
    if (const auto* baker = w.entity("baker1"))
    {
        ada.cellId = baker->cellId;
        ada.position = {baker->position.x + 1, baker->position.y};
    }
    w.society().shift("treasury", "player-ada", "", 0, 50, "test: a purse");
    const auto bought = w.trade("player-ada", "baker1", ware, 1, true);
    expect(bought.ok && stock(w, "player-ada", ware) == 1, "A player buys one (" + bought.message + ")");
    expect(std::string(Society::itemName(ware)) == items::good(ware)->name, "by its catalog name");
}

// A town's own buyers (doc 35, Part 7): funded by the treasury, they ask for what the market can't sell them, as
// contracts for goods, and a player delivers, a few at a time, paid by the piece.
void townBuyersAndContractsForGoods()
{
    // East with a hundred more townsfolk: a town big enough for its Town Works to want a cartload.
    std::string folk;
    for (int n = 0; n < 100; ++n)
        folk += resident("ex" + std::to_string(n), "East Folk " + std::to_string(n), "civilian", "working", 0, 2.5 + n % 12, 0, 2.5 + n % 12);
    auto f = strip("EE.....WW", folk);
    auto w = load(f);
    w.addPlayer("player-ada", "Ada");
    w.tick(.6);
    std::string kid;
    int quantity = 0;
    std::int64_t reward = 0;
    for (int d = 0; d < 4 && kid.empty(); ++d)
    {
        nextMorning(w);
        run(w, 2);
        for (const auto& c : w.roads().contracts)
            if (c.kind == "procure" && c.status == "open" && c.town == "east" && c.item == "stone")
            {
                kid = c.id;
                quantity = c.quantity;
                reward = c.reward;
            }
    }
    expect(!kid.empty(), "East's Town Works asks for the stone its market doesn't sell");
    expect(w.society().account("town:east:works") && quantity >= 2 &&
               reward >= quantity * Society::pennies(w.society().townPrice("east", "stone")),
           "a contract for goods, paid from its funds, at the going price (doc 64, Phase 4) and more");
    expect(w.society().conserved(), "money stays conserved");
    auto* ada = w.entity("player-ada");
    ada->cellId = id(0);
    ada->position = {w.entity("em")->position.x + 1, w.entity("em")->position.y};
    expect(w.takeContract("player-ada", kid).ok, "Ada takes it on");
    expect(!w.deliverContract("player-ada", kid).ok, "with nothing to deliver, nothing is delivered");
    const auto purse = w.society().account("player-ada")->cash;
    w.society().create("player-ada", "stone", 1, "test: quarried");
    w.tick(.6);
    expect(w.deliverable("player-ada").size() == 1, "a merchant takes delivery");
    auto r = w.deliverContract("player-ada", kid);
    expect(r.ok && w.society().account("player-ada")->cash > purse, "one delivered, and paid for: " + r.message);
    w.society().create("player-ada", "stone~fine", quantity, "test: quarried");
    r = w.deliverContract("player-ada", kid);
    expect(r.ok && w.society().account("player-ada")->cash == purse + reward, "the rest delivered (any quality): the whole reward");
    expect(Society::stockAll(*w.society().account("player-ada"), "stone") == 1, "and only what was wanted is taken");
    expect(Society::stockAll(*w.society().account("town:east:works"), "stone") >= quantity, "the stone goes to the Town Works");
    for (const auto& c : w.roads().contracts)
        if (c.id == kid)
            expect(c.status == "done", "the work is done");
    expect(w.society().conserved(), "money stays conserved");
}

// Town purses and the month's reckoning (Docs/Design/42-money-in-circulation.md, Phase 1): each town has its own
// treasury and pays its own wages; every 28 days a tenth of each resident's profit goes to its town and a tenth to its
// church; the towns send the capital a tenth of their tax. Nothing is made: money stays conserved throughout.
void townPursesAndTheReckoning()
{
    auto f = strip();
    auto w = load(f);
    const auto treasury = w.society().account("treasury")->cash;
    w.tick(.6);
    const auto westPurse = w.society().account("stores:west")->cash;
    expect(westPurse == treasury * 6 / 12, "West keeps its own purse: its share of the treasury by how many live there (" +
                                               std::to_string(westPurse) + "p of " + std::to_string(treasury) + "p)");
    expect(w.society().capital() == "east", "East is the capital");
    expect(w.society().treasuryOfResident("w1") == "stores:west" && w.society().treasuryOfResident("e1") == "treasury" &&
               w.society().churchOf("stores:west") == Society::SharedChurch && w.society().churchOf("treasury") == Society::SharedChurch,
           "Each pays its own town, and every town's church keeps the land's one purse");
    // A working morning: west's wages come from west's purse.
    {
        auto saved = w.save();
        saved.calendarDays = std::floor(saved.calendarDays) + 1.4;
        expect(w.restore(saved).ok, "To the next morning");
    }
    w.tick(.6);
    std::int64_t westWages = 0, eastToWest = 0;
    for (int t = 0; t < 1500; ++t)
    {
        w.tick(1);
        for (const auto& e : w.takeEvents())
            if (e.kind == "economy" && e.detail == "service wages")
            {
                westWages += e.actor == "stores:west" && e.target.front() == 'w' ? e.coins : 0;
                eastToWest += e.actor == "treasury" && e.target.front() == 'w' ? e.coins : 0;
            }
    }
    expect(westWages > 0 && eastToWest == 0, "West's townsfolk are paid by west (" + std::to_string(westWages) + "p), not the capital");
    const auto* church = w.society().account(Society::SharedChurch);
    expect(church && church->cash > 0, "The land's church was founded");
    // A week's doings (doc 64, Phase 7: tax and tithe on income): w1 is paid for work, w2 inherits, w3 spends all it has.
    const auto& books = w.society().state().books;
    expect(books.month == 0 && books.start.count("w1") && books.start.count("wm"), "The week's books are open");
    expect(w.society().shift("treasury", "w1", "", 0, 30, "day labour"), "w1 is paid for some work");
    expect(w.society().operatorTransfer("treasury", "w2", "", 0, 300).ok, "w2 is left 300p (not earned)");
    w.society().shift("w3", "wm", "", 0, w.society().account("w3")->cash, "test: spends it all");
    w.takeEvents();
    const auto told = w.reckonNow();
    std::int64_t tax = 0, tithes = 0, toCapital = 0;
    std::map<std::string, std::int64_t> taxed, tithed;
    bool w3Paid = false, logged = false, wealth = false;
    for (const auto& e : w.takeEvents())
    {
        if (e.kind == "economy" && e.detail == "town tax" && e.target == "stores:west")
            tax += e.coins;
        if (e.kind == "economy" && e.detail == "town tax")
            taxed[e.actor] += e.coins;
        if (e.kind == "economy" && e.detail == "tithe" && e.target == Society::SharedChurch)
        {
            tithed[e.actor] += e.coins;
            tithes += e.actor.front() == 'w' ? e.coins : 0;
        }
        if (e.kind == "economy" && e.detail == "capital's share" && e.actor == "stores:west")
            toCapital += e.coins;
        wealth |= e.kind == "economy" && (e.detail == "a wealth tithe" || e.detail == "market dues");
        w3Paid |= e.kind == "economy" && e.actor == "w3" && (e.detail == "town tax" || e.detail == "tithe");
        logged |= e.kind == "reckoning" && e.actor == "stores:west" && e.detail.find("West's reckoning") != std::string::npos;
    }
    expect(taxed["w1"] >= std::llround(30 * Society::TaxRate) && std::abs(double(tithed["w1"]) - double(taxed["w1"]) * .75) <= 1,
           "w1 pays 8% of its week's income in tax and 6% in tithe (" + std::to_string(taxed["w1"]) + "p, " + std::to_string(tithed["w1"]) + "p)");
    expect(taxed["w2"] < std::llround(300 * Society::TaxRate), "w2 pays nothing on what it inherited");
    expect(!w3Paid, "w3, with nothing in its purse, pays nothing");
    expect(!wealth, "and there's no wealth tithe or market dues");
    expect(tax > 0 && tithes > 0 && tithes <= tax && toCapital == tax / Society::CapitalShare,
           "West took in " + std::to_string(tax) + "p in tax and the church " + std::to_string(tithes) + "p in tithes; west sent the capital a tenth");
    expect(logged && told.find("West's reckoning") != std::string::npos, "The reckoning is told: " + told);
    expect(books.start.at("w1") == w.society().account("w1")->cash && books.unearned.empty(), "New books are opened");
    expect(w.society().conserved(), "Money stays conserved");
    // The books are saved with the society.
    World again = load(f);
    expect(again.restore(w.save()).ok && again.society().state().books.start == books.start &&
               again.society().state().books.month == books.month,
           "The books survive a restart");
    // And the reckoning comes by itself each week (the user, 2026-10-05), on the 7th day.
    {
        auto saved = w.save();
        saved.calendarDays = 7.3;
        expect(w.restore(saved).ok, "A week on");
    }
    w.takeEvents();
    run(w, 2);
    bool reckoned = false;
    for (const auto& e : w.takeEvents())
        reckoned |= e.kind == "reckoning";
    expect(reckoned && w.society().state().books.month == 1, "The week's reckoning comes on day 7");
    expect(w.society().conserved(), "Money stays conserved");
}

// Who pays whom (doc 42, Phase 2): a shop's help by its keeper, the watch by the town, the clergy by the church; a
// farmer lives by what it brings in, and a child draws no wages.
void whoPaysWages()
{
    auto f = strip("EE.....WW", resident("help", "Holly Help", "civilian", "helping at the stall", 0, 9.5, 1, 9.5) +
                                    resident("watch1", "Ward Gate", "guard", "keeping the gate", 0, 10.5, 0, 10.5) +
                                    resident("chap", "Cleric Bell", "civilian", "keeps the Down Chapel", 0, 11.5, 0, 11.5) +
                                    resident("farm", "Barley Field", "civilian", "farms the valley fields", 0, 12.5, 0, 12.5) +
                                    resident("lamp", "Lamp Wick", "civilian", "lights the lamps", 0, 13.5, 0, 13.5) +
                                    resident("home", "Hearth Keeper", "civilian", "keeps the house", 0, 14.5, 0, 14.5) +
                                    resident("maid", "Tidy Paws", "civilian", "serving the household", 0, 7.5, 7, 7.5) +
                                    resident("smoker", "Kipper Hand", "civilian", "smoking fish", 0, 6.5, 0, 6.5));
    auto w = load(f);
    w.tick(.6);
    const auto& soc = w.society();
    const auto payer = [&](const std::string& who, int age = 30) { return soc.payerOf(who, *soc.jobOf(who), age); };
    expect(payer("help").account == soc.tillOf("em") && payer("help").whom == "the shop", "A shop's help is paid by its shop's till");
    expect(payer("watch1").account == "treasury", "the watch by the town");
    expect(payer("chap").account == Society::SharedChurch, "a chapel keeper by the church");
    expect(payer("farm").account.empty(), "a farmer lives by what it brings in");
    expect(payer("lamp").account == "treasury", "the lamplighter by the town");
    expect(payer("lamp", 12).account.empty(), "and a child draws no wages");
    expect(payer("w1").account == "stores:west", "West's folk by west");
    expect(payer("home").account.empty(), "Keeping one's own house earns no wage (doc 42)");
    w.society().shift("treasury", "w3", "", 0, 50, "test: the richest of west's house");
    expect(payer("maid").account == "w3" && payer("maid").whom == "the house", "A household's servant is paid by its richest (" + payer("maid").account + ")");
    expect(payer("smoker").account == "treasury", "A trade's hand with no keeper of the trade in town falls to the town");
    // A working day: the help's wages come out of the shop's till (Ember's purse before the shop had one: doc 46).
    {
        auto saved = w.save();
        saved.calendarDays = std::floor(saved.calendarDays) + 1.4;
        expect(w.restore(saved).ok, "To the next morning");
    }
    w.society().shift("treasury", "em", "", 0, 50, "test: a good week");
    w.tick(.6);
    std::int64_t fromShop = 0, fromTown = 0;
    for (int t = 0; t < 1500; ++t)
    {
        w.tick(1);
        for (const auto& e : w.takeEvents())
            if (e.kind == "economy" && e.detail == "service wages" && e.target == "help")
                (e.actor == w.society().tillOf("em") ? fromShop : fromTown) += e.coins;   // (Its till, since doc 46.)
    }
    expect(fromShop > 0 && fromTown == 0, "The help is paid by the shop (" + std::to_string(fromShop) + "p)");
    // The shop goes broke: the help waits on its wages, and after a week labours for the Town Works instead. (The town's
    // wage support went with the orchestrator's channels: doc 64, Phase 12.)
    const auto emTill = w.society().tillOf("em");
    w.society().shift(emTill, "treasury", "", 0, w.society().account(emTill)->cash, "test: a bad week");
    w.society().shift("treasury", "stores:west", "", 0, w.society().account("treasury")->cash - 100, "test: a lean year");
    if (const auto* church = w.society().account(Society::SharedChurch))
        w.society().shift(Society::SharedChurch, "stores:west", "", 0, church->cash, "test: a lean year");
    run(w, 700);
    const auto* help = w.society().resident("help");
    expect(help->reason.find("Waiting on wages from Ember Oak") != std::string::npos, "Unpaid, the help says so: " + help->reason);
    {
        auto saved = w.save();
        saved.calendarDays = std::floor(saved.calendarDays) + 8.4;
        expect(w.restore(saved).ok, "A week on");
    }
    std::int64_t labour = 0;
    for (int t = 0; t < 1500; ++t)
    {
        w.tick(1);
        for (const auto& e : w.takeEvents())
            if (e.kind == "economy" && e.detail == "day labour" && e.target == "help")
                labour += e.coins;
    }
    expect(labour > 0 && w.society().resident("help")->task == Society::LabourTitle, "and a week on, labours for the town instead");
    expect(w.society().conserved(), "Money stays conserved");
}

// A living for every grown wolf (doc 42, Phase 3): out of work, one labours for the Town Works at the square, paid by
// the town; at 65, one retires; a foreman's unpaid post is paid by the town; a beggar's isn't.
void aLivingForEveryone()
{
    const auto aged = [](std::string line, int age) { return line.replace(line.find("\"Hello.\" 30 "), 12, "\"Hello.\" " + std::to_string(age) + " "); };
    auto foreman = resident("boss", "Iron Boss", "civilian", "overseeing the ironworks", 0, 9.5, 0, 9.5);
    foreman.replace(foreman.find(" 1 8 17 "), 8, " 0 8 17 ");          // Authored unpaid.
    auto beggar = resident("beg", "Poor Tom", "civilian", "begging in the market", 0, 10.5, 1, 4.5);
    beggar.replace(beggar.find(" 1 8 17 "), 8, " 0 8 17 ");
    auto f = strip("EE.....WW", resident("idle", "Idle Hand", "civilian", "-", 0, 11.5, 0, 11.5) +
                                    aged(resident("old", "Old Grey", "civilian", "-", 0, 12.5, 0, 12.5), 70) + foreman + beggar);
    auto w = load(f);
    {
        auto saved = w.save();
        saved.calendarDays = std::floor(saved.calendarDays) + 1.4;
        expect(w.restore(saved).ok, "To the next morning");
    }
    w.tick(.6);
    std::map<std::string, std::int64_t> paid;
    for (int t = 0; t < 1500; ++t)
    {
        w.tick(1);
        for (const auto& e : w.takeEvents())
            if (e.kind == "economy" && (e.detail == "day labour" || e.detail == "service wages") && e.actor == "treasury")
                paid[e.target + "|" + e.detail] += e.coins;
    }
    const auto* idle = w.society().resident("idle");
    expect(idle && idle->task == Society::LabourTitle, "Out of work, Idle labours for the Town Works (" + (idle ? idle->task : "") + ")");
    expect(paid["idle|day labour"] > 0, "and is paid by the town for it");
    expect(w.society().resident("old")->task != Society::LabourTitle && !paid.count("old|day labour"), "Old Grey, at 70, has retired");
    expect(paid["boss|service wages"] > 0, "The foreman's unpaid post is paid now");
    expect(!paid.count("beg|service wages"), "A beggar is not paid (alms come from the church)");
    expect(w.society().conserved(), "Money stays conserved");
}

// Working out of town (doc 42, Phase 3b): out of work, a wolf takes up a trade on the wild ground near its town, works
// it from the same patches players forage, and sells what it brings back to a shop that buys it.
void workingOutOfTown()
{
    auto f = strip("EE.....WW", resident("idle", "Idle Hand", "civilian", "-", 0, 11.5, 0, 11.5) +
                                    resident("herb", "Sage Root", "merchant", "herbalist at Valley Remedies", 0, 12.5, 1, 6.5) +
                                    resident("store", "Toll Keeper", "merchant", "shopkeeper at The Toll House Store", 0, 13.5, 1, 10.5));
    // Grass and woods in the wild cell beside east.
    {
        auto& text = f.cells[id(2)];
        const auto grid = text.find("grid:\n") + 6;
        std::string rows;
        for (int y = 0; y < Side; ++y)
            rows += "." + std::string(Side / 2 - 1, ',') + std::string(Side / 2 - 2, y % 3 ? '.' : 'Y') + "..\n";   // (Its edges open.)
        text = text.substr(0, grid) + rows;
    }
    auto w = load(f);
    w.society().shift("treasury", "herb", "", 0, 200, "test: a shop's float");
    w.society().shift("treasury", "store", "", 0, 200, "test: a shop's float");
    {
        auto saved = w.save();
        saved.calendarDays = std::floor(saved.calendarDays) + 1.36;
        expect(w.restore(saved).ok, "To the next morning");
    }
    w.tick(.6);
    std::int64_t brought = 0, sold = 0;
    bool wentOut = false;
    for (int t = 0; t < 9 * 600; ++t)
    {
        w.tick(1);
        if (const auto* e = w.entity("idle"); e && e->cellId == id(2))
            wentOut = true;


        for (const auto& e : w.takeEvents())
        {
            if (e.kind == "economy" && e.target == "idle" && (e.detail == "brought in" || e.detail == "hunted"))
                brought += e.quantity;
            if (e.kind == "economy" && e.detail == "brought in and sold" && e.target == "idle")
                sold += e.coins;
        }
    }
    const auto* life = w.society().resident("idle");
    expect(Society::outworkTitled(life->task) || life->task == "selling" || wentOut, "Idle takes up a trade out of town (" + life->task + ")");
    expect(wentOut, "and walks out to the wild to work it");
    expect(brought > 0, "bringing goods in from the land (" + std::to_string(brought) + ")");
    expect(sold > 0, "and sells them in town (" + std::to_string(sold) + "p)");
    {
        Position out;
        out.title = life->task == "selling" ? Society::outworkTitle("gathering") : life->task;
        expect(!Society::outworkTitled(out.title) || w.society().payerOf("idle", out, 30).account.empty(), "Nobody pays it a wage");
    }
    expect(w.society().conserved(), "Money stays conserved");
}

// Great houses (doc 42, Phase 5b): a house owns its town's industry and some shops; each has a till of its own; the
// manager is paid a wage from it; what is above its float goes to the house each night; the house props up a till run
// low, and sells a business that keeps needing it to another house.
void greatHouses()
{
    const auto unpaid = [](std::string line) { line.replace(line.find(" 1 8 17 "), 8, " 0 8 17 "); return line; };
    auto f = strip("EE.....WW", unpaid(resident("lord", "Lord Ash", "civilian", "ruling House Ash", 0, 9.5, 0, 9.5)) +
                                    unpaid(resident("lady", "Lady Vesk", "civilian", "keeping Vesk Manor", 0, 10.5, 0, 10.5)) +
                                    resident("tanner", "Hide Tanner", "merchant", "tanner at The Hide Yard", 0, 11.5, 1, 11.5));
    auto w = load(f);
    w.society().shift("treasury", "tanner", "", 0, 300, "test: savings");
    w.tick(.6);
    nextMorning(w);
    run(w, 2);
    const auto& soc = w.society();
    expect(soc.houses().size() == 2 && soc.houses()[0].community == "east", "East has two great houses");
    const auto pid = soc.jobOf("tanner")->id;
    const auto house = soc.ownerOf(pid);
    expect(house == "house:house_ash" || house == "house:vesk_manor", "The tannery (industry) is a house's: " + house);
    const auto till = "till:" + pid;
    expect(soc.tillOf("tanner") == till && soc.account(till), "with a till of its own");
    expect(soc.account("house:house_ash")->cash >= Society::HouseFortune / 2 && soc.account("house:vesk_manor")->cash >= Society::HouseFortune / 2,
           "Each house was founded with a fortune of its own");
    expect(soc.account("tanner")->cash <= soc.pennies(Society::ManagerWage, "") * (Society::FloatDays + 3),
           "the manager keeps only its own week's wage (" + std::to_string(soc.account("tanner")->cash) + "p)");
    expect(Society::stockAll(*soc.account(till), "leather") > 0 && Society::stockAll(*soc.account("tanner"), "leather") == 0,
           "and the shop's goods are on the till's shelves");
    // A player buys from the shop: the till is paid.
    auto& ada = w.addPlayer("player-ada", "Ada");
    ada.cellId = w.entity("tanner")->cellId;
    ada.position = {w.entity("tanner")->position.x + 1, w.entity("tanner")->position.y};
    w.society().shift("treasury", "player-ada", "", 0, 100, "test: a purse");
    const auto tillBefore = soc.account(till)->cash, managerBefore = soc.account("tanner")->cash;
    std::string ware;
    for (const auto& x : soc.wares("tanner"))
        if (soc.quote("player-ada", "tanner", x, 1, true).ok)
            ware = x;
    expect(!ware.empty() && w.trade("player-ada", "tanner", ware, 1, true).ok, "Ada buys " + ware + " at the tannery");
    expect(soc.account(till)->cash > tillBefore && soc.account("tanner")->cash == managerBefore, "and pays the till, not the manager");
    // The manager is paid its wage from the till each day.
    w.takeEvents();
    nextMorning(w);
    run(w, 2);
    std::int64_t wage = 0;
    for (const auto& e : w.takeEvents())
        if (e.kind == "economy" && e.detail == "manager's wage" && e.target == "tanner")
            wage += e.coins;
    // (Its wage as it stood when paid: the town's going rate is worked out afresh after the houses' day.)
    expect(wage > 0 && std::abs(double(wage) - soc.employerWage(till)) < 2, "The manager is paid its business's wage from the till (" +
                                                                             std::to_string(wage) + "p)");
    // On the business's day of the month, all above its working money is its house's (doc 64, Phase 5: no longer each
    // night's takings above a float).
    auto saved = w.save();
    saved.calendarDays = MonthDays + Society::ownersDay(pid) - 1 + 23. / 24;
    expect(w.restore(saved).ok, "To the eve of its day of the month");
    w.tick(.6);
    w.society().shift("treasury", till, "", 0, soc.workingMoney(pid) + 500, "test: a good month");
    w.takeEvents();
    run(w, 600 * 2);
    std::int64_t profit = 0;
    for (const auto& e : w.takeEvents())
        if (e.kind == "economy" && e.detail == "the owner's profit" && e.actor == till && e.target == house)
            profit += e.coins;
    expect(profit > 0 && soc.account(till)->cash <= soc.workingMoney(pid) + 50,
           "The month's profit goes to the house (" + std::to_string(profit) + "p); the till keeps its working money");
    // Run dry the evening before its day of the week: the house puts money in (no rescue loans, no forced sale).
    saved = w.save();
    std::int64_t next = std::int64_t(std::floor(saved.calendarDays)) + 1;
    while (next % 7 != Society::ownersWeekday(pid))
        ++next;
    saved.calendarDays = double(next) - 1 + 23. / 24;
    expect(w.restore(saved).ok, "To the eve of its day of the week");
    w.tick(.6);
    w.society().shift(till, "treasury", "", 0, soc.account(till)->cash, "test: a bad week");
    w.takeEvents();
    run(w, 600 * 2);
    std::int64_t invested = 0, rent = 0;
    for (const auto& e : w.takeEvents())
    {
        invested += e.kind == "economy" && e.detail == "an owner's investment" && e.actor == house && e.target == till ? e.coins : 0;
        rent += e.kind == "economy" && e.detail == "rent" && e.actor == till ? e.coins : 0;
    }
    expect(invested > 0, "A till run dry is topped up by its house (" + std::to_string(invested) + "p)");
    expect(rent == 0, "and a house's own business pays no rent");
    expect(soc.state().books.start.count(house), "Houses keep books for the reckoning");
    const auto other = house;
    World again = load(f);
    expect(again.restore(w.save()).ok && again.society().ownerOf(pid) == other && again.society().tillOf("tanner") == till,
           "Who owns what survives a restart");
    expect(w.society().conserved(), "Money stays conserved");
}

// The rule against hoarding (doc 42): a church with more than it needs feeds the poor; a town hires hands for its
// works; neither sits on its money. Money stays conserved.
// One church (doc 42, "One church", the user, 2026-10-06): an older save's churches each kept money of their own; on the
// next day it is all in the land's one purse, and nothing is made or lost.
void oneChurch()
{
    auto f = strip();
    auto w = load(f);
    w.tick(.6);
    auto& s = w.society();
    s.openAccount("town:west:church");
    expect(s.shift("stores:west", "town:west:church", "", 0, 120, "test: an older save's church purse"),
           "An older save: West's church holds 120p of its own");
    w.takeEvents();
    nextMorning(w);
    std::int64_t joined = 0;
    for (const auto& e : w.takeEvents())
        if (e.kind == "economy" && e.detail == "the churches' purses joined" && e.actor == "town:west:church" &&
            e.target == Society::SharedChurch)
            joined += e.coins;
    expect(joined == 120 && s.account("town:west:church")->cash == 0, "The next day it is in the land's purse (" + std::to_string(joined) + "p)");
    expect(s.conserved(), "Money stays conserved");
}

// Daily relief (doc 64, Phase 7, in place of doc 42's dole): a household short of a day's food money is given it, by the
// church and then its town, out of what each set aside for the week.
void theRelief()
{
    auto f = strip();
    auto w = load(f);
    w.tick(.6);
    auto& s = w.society();
    s.openAccount(Society::SharedChurch);
    expect(s.shift("treasury", Society::SharedChurch, "", 0, std::min<std::int64_t>(3000, s.account("treasury")->cash / 2), "test: a full church"),
           "The church is full");
    // (The household is penniless: its members share one purse.)
    const auto home = s.resident("w1")->homeCell;
    for (const auto& [id, life] : s.state().residents)
        if (life.homeCell == home && s.account(id)->cash > 0)
            s.shift(id, "treasury", "", 0, s.account(id)->cash, "test: penniless");
    w.takeEvents();
    nextMorning(w);
    std::int64_t given = 0;
    for (const auto& e : w.takeEvents())
        if (e.kind == "economy" && e.detail == "relief")
            given += e.coins;
    expect(given > 0, "A penniless household is given a day's food money in relief (" + std::to_string(given) + "p)");
    expect(s.conserved(), "Money stays conserved");
}

// Spoilage (doc 42, "Spoilage", the user, 2026-10-06): food keeps its days (items.json `keeps`; fresh fish three), then
// spoils; what keeps (salt fish, months) doesn't.
void spoilage()
{
    auto f = strip();
    auto w = load(f);
    w.tick(.6);
    auto& s = w.society();
    expect(s.openAccount("contract:larder-test"), "A store nobody draws on");
    s.create("contract:larder-test", "fish", 6, "test: caught");
    s.create("contract:larder-test", "salt_fish", 4, "test: salted");
    for (int day = 0; day < 3; ++day)
        nextMorning(w);
    expect(Society::stock(*s.account("contract:larder-test"), "fish") == 6, "Fresh fish keeps three days");
    nextMorning(w);
    nextMorning(w);
    expect(Society::stock(*s.account("contract:larder-test"), "fish") == 0, "then it spoils");
    expect(Society::stock(*s.account("contract:larder-test"), "salt_fish") == 4, "Salt fish keeps");
}

void noHoarding()
{
    auto f = strip("EE.....WW", resident("baker", "Bram Loaf", "merchant", "baker at The Amber Loaf", 7, 9.5, 8, 9.5));
    auto w = load(f);
    w.tick(.6);
    nextMorning(w);
    run(w, 2);
    // The church is rich, and w1 is penniless with nothing to eat.
    const auto church = Society::SharedChurch;
    expect(w.society().account(church) != nullptr, "The land has a church");
    expect(w.society().shift("stores:west", church, "", 0, std::min<std::int64_t>(800, w.society().account("stores:west")->cash - 50), "test: a generous month"),
           "The church is given a great deal");
    w.society().shift("w1", "treasury", "", 0, w.society().account("w1")->cash, "test: penniless");
    for (const auto& [item, n] : std::map<std::string, int>(w.society().account("w1")->stock.begin(), w.society().account("w1")->stock.end()))
        w.society().consume("w1", item, n, "test: nothing to eat");
    const auto churchBefore = w.society().account(church)->cash;
    w.takeEvents();
    nextMorning(w);
    run(w, 2);
    bool alms = false, spent = false;
    std::string told;
    for (const auto& e : w.takeEvents())
    {
        alms |= e.kind == "economy" && e.detail == "alms" && e.target == "w1";
        if (e.kind == "surplus spent" && e.actor == church)
            spent = true, told = e.detail;
    }
    (void)churchBefore;
    (void)spent;                                     // (What it holds it spends by its own rules: institutionsWeek.)
    expect(alms && !Society::bestFood(*w.society().account("w1")).empty(), "Penniless w1 is given food as alms by the church");
    expect(w.society().conserved(), "Money stays conserved");
}

// Travel keeps to the roads (the user, 2026-10-05): two rows of three cells, a road from the top left down and along
// the bottom row and up to the top right, and none in the top middle. From the top left to the top right the road's way
// (five cells) is taken over the straight one across open country (three); with no roads, the straight one.
void travelKeepsToTheRoads()
{
    for (const bool roads : {true, false})
    {
        Fixture f;
        const auto cellId = [](int x, int y) { return "g_" + std::to_string(x) + "_" + std::to_string(y); };
        std::ostringstream m;
        m << "RATW_WORLD 3\n";
        std::map<std::string, std::set<std::string>> exits;
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 3; ++x)
            {
                const char ground = roads && !(x == 1 && y == 0) ? 'd' : '.';
                std::ostringstream cell;
                cell << "id: " << cellId(x, y) << "\nname: Ground\ndescription: Open ground.\nworld: " << x * Side << ' ' << y * Side
                     << " 0\noutdoors: true\nweather: clear\nsize: " << Side << ' ' << Side << "\ngrid:\n";
                for (int row = 0; row < Side; ++row)
                    cell << std::string(Side, ground) << '\n';
                f.cells[cellId(x, y)] = cell.str();
                m << "area \"" << cellId(x, y) << "\"\n";
            }
        int seam = 0;
        const auto join = [&](int ax, int ay, int bx, int by) {
            const bool across = bx > ax;            // East, else south.
            for (int k = 0; k < Side; ++k)
            {
                const std::string a = "seam_" + std::to_string(seam) + "_a", b = "seam_" + std::to_string(seam) + "_b";
                const double px = across ? Side - .5 : k + .5, py = across ? k + .5 : Side - .5;
                const double qx = across ? .5 : k + .5, qy = across ? k + .5 : .5;
                std::ostringstream one, two;
                one << "door \"" << a << "\" \"Open boundary\" \"" << cellId(ax, ay) << "\" " << px << ' ' << py << " \"" << cellId(bx, by)
                    << "\" " << qx << ' ' << qy << " \"" << b << "\" 1 0 1 1 \"" << (across ? 'E' : 'S') << "\"\n";
                two << "door \"" << b << "\" \"Open boundary\" \"" << cellId(bx, by) << "\" " << qx << ' ' << qy << " \"" << cellId(ax, ay)
                    << "\" " << px << ' ' << py << " \"" << a << "\" 1 0 1 1 \"" << (across ? 'W' : 'N') << "\"\n";
                f.seams[cellId(ax, ay)] += one.str();
                f.seams[cellId(bx, by)] += two.str();
                ++seam;
            }
            exits[cellId(ax, ay)].insert(cellId(bx, by));
            exits[cellId(bx, by)].insert(cellId(ax, ay));
        };
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 3; ++x)
            {
                if (x + 1 < 3)
                    join(x, y, x + 1, y);
                if (y + 1 < 2)
                    join(x, y, x, y + 1);
            }
        for (const auto& [from, to] : exits)
        {
            m << "exits \"" << from << "\" " << to.size();
            for (const auto& next : to)
                m << " \"" << next << '"';
            m << '\n';
        }
        m << "spawn \"" << cellId(0, 0) << "\" 8.5 8.5\n";
        f.manifest = m.str();
        auto w = load(f);
        const auto route = w.routeBetween(cellId(0, 0), cellId(2, 0));
        std::string seen;
        for (const auto& c : route)
            seen += c + " ";
        if (roads)
            expect(route == std::vector<std::string>{"g_0_0", "g_0_1", "g_1_1", "g_2_1", "g_2_0"},
                   "Along the road, the long way round, rather than across open country: " + seen);
        else
            expect(route == std::vector<std::string>{"g_0_0", "g_1_0", "g_2_0"}, "With no road, straight across: " + seen);
    }
}

// Standing orders (the user, 2026-10-05) are renegotiated: when one is due, a porter of the buyer's town walks to the
// selling town's market, and there agrees it again at the price the goods fetch now, for four weeks more.
void standingOrdersAreRenegotiated()
{
    auto f = strip("EE.....WW", resident("miller", "Flour Dust", "merchant", "keeps the water mill at Eastford", 0, 9.5, 1, 9.5) +
                                    resident("baker", "Bram Loaf", "merchant", "baker at The Amber Loaf", 7, 9.5, 8, 9.5) +
                                    resident("porter", "Pell Strap", "civilian", "carries loads for hire", 7, 11.5, 7, 11.5));
    auto w = load(f);
    w.tick(.6);
    w.roads().camps.clear();
    const auto mill = w.society().tillOf("miller"), bakery = w.society().tillOf("baker"), west = w.society().tillOf("wm");
    w.society().create(mill, "flour", 60, "test: a good harvest");
    for (const auto& kind : Society::kindsHeld(*w.society().account(bakery), "flour"))
        w.society().consume(bakery, kind, Society::stock(*w.society().account(bakery), kind), "test: a bad week");
    for (const auto& kind : Society::kindsHeld(*w.society().account(west), "flour"))
        w.society().consume(west, kind, Society::stock(*w.society().account(west), kind), "test: a bad week");
    w.society().shift("treasury", bakery, "", 0, 200, "test: takings");
    nextMorning(w);
    auto order = std::find_if(w.roads().orders.begin(), w.roads().orders.end(), [&](const StandingOrder& o) { return o.buyer == bakery; });
    expect(order != w.roads().orders.end(), "The bakery has a standing order for flour from east");
    const auto id = order->id;
    order->review = 0;                             // (Due now.)
    w.takeEvents();
    nextMorning(w);
    order = std::find_if(w.roads().orders.begin(), w.roads().orders.end(), [&](const StandingOrder& o) { return o.id == id; });
    expect(order != w.roads().orders.end() && order->negotiator == "porter", "West's porter is sent to renegotiate it");
    bool provisioned = false, agreed = false;
    for (const auto& e : w.takeEvents())
        provisioned |= e.kind == "economy" && (e.detail == "provisions for the road" || e.detail == "money for the road") && e.target == "porter";
    expect(provisioned, "with food for the road (or the money for it) from the bakery");
    for (int t = 0; t < 6000 && !agreed; ++t)
    {
        w.tick(1);
        for (const auto& e : w.takeEvents())
            agreed |= (e.kind == "standing order renegotiated" || e.kind == "standing order ends") && e.target == bakery;
    }
    expect(agreed, "The porter reaches east's market and the order is agreed again");
    order = std::find_if(w.roads().orders.begin(), w.roads().orders.end(), [&](const StandingOrder& o) { return o.id == id; });
    expect(order == w.roads().orders.end() || (order->negotiator.empty() && order->review > w.calendarDays()),
           "for four weeks more");
    expect(w.society().conserved(), "Money stays conserved");
}

// A shopkeeper doesn't hoard either (the user, 2026-10-05). Since doc 46's Phase 2 every shop keeps a till of its own,
// apart from its keeper's purse: the shop's goods and money move into it on the first day (all but a month's living), and
// it pays its keeper a wage each day. Since doc 64 what it holds over its working money goes to its owner monthly
// (theOwnersProfit), and it pays no market dues and no tax of its own: its owner is taxed on what it's paid.
void shopkeepersDontHoard()
{
    auto f = strip("EE.....WW", resident("cook", "Pell Ladle", "merchant", "keeps the cookshop", 7, 9.5, 8, 9.5) +
                                    resident("help", "Tam Ladle", "civilian", "helping at the cookshop", 7, 10.5, 8, 10.5));
    auto w = load(f);
    w.tick(.6);
    const auto till = w.society().tillOf("cook");
    expect(till.rfind("till:", 0) == 0, "The cookshop has a till of its own: " + till);
    expect(w.society().account("cook")->cash <= w.society().pennies(MonthDays * Society::FoodADay, ""),
           "and its keeper keeps at most a month's living (" + std::to_string(w.society().account("cook")->cash) + "p)");
    w.society().shift("treasury", till, "", 0, 2000, "test: a fine year");
    w.takeEvents();
    bool wage = false, levied = false;
    for (int d = 0; d < 8; ++d)
    {
        nextMorning(w);
        run(w, 2);
        for (const auto& e : w.takeEvents())
        {
            wage |= e.kind == "economy" && e.detail == "the keeper's wage" && e.actor == till && e.target == "cook";
            levied |= e.kind == "economy" && (e.detail == "market dues" || e.detail == "town tax" || e.detail == "a wealth tithe") && e.actor == till;
        }
    }
    expect(wage, "The till pays its keeper a wage");
    expect(!levied, "and pays no dues or tax of its own");
    expect(w.society().conserved(), "Money stays conserved");
}

// Rent on a House's land (doc 64, Phase 5): a farm in a town with great houses pays one of them a share of its week's
// takings, on its own day of the week; and its rent counts in its price.
void rentOnHouseLand()
{
    const auto unpaid = [](std::string line) { line.replace(line.find(" 1 8 17 "), 8, " 0 8 17 "); return line; };
    auto f = strip("EE.....WW", unpaid(resident("lord", "Lord Ash", "civilian", "ruling House Ash", 0, 9.5, 0, 9.5)) +
                                    unpaid(resident("lady", "Lady Vesk", "civilian", "keeping Vesk Manor", 0, 10.5, 0, 10.5)) +
                                    resident("farmer", "Barley Field", "civilian", "farms the valley fields", 0, 12.5, 0, 12.5));
    auto w = load(f);
    w.tick(.6);
    nextMorning(w);
    run(w, 2);
    auto& s = w.society();
    const auto till = s.tillOf("farmer");
    expect(till.rfind("till:", 0) == 0, "The farm keeps its own till");
    const auto pid = till.substr(5);
    const auto landlord = s.landlordOf(pid);
    expect(landlord == "house:house_ash" || landlord == "house:vesk_manor", "and works a House's land: " + landlord);
    // Its takings this week, then its day of the week.
    auto saved = w.save();
    std::int64_t next = std::int64_t(std::floor(saved.calendarDays)) + 1;
    while (next % 7 != Society::ownersWeekday(pid))
        ++next;
    saved.calendarDays = double(next) - 1 + 23. / 24;
    expect(w.restore(saved).ok, "To the eve of its day of the week");
    w.tick(.6);
    const std::string wheat = "wheat";
    s.create(till, wheat, 5, "test: the harvest");
    expect(s.sale(till, "treasury", wheat, 1, 100, "test: a week's sales"), "The farm sells " + wheat);
    w.takeEvents();
    run(w, 600 * 2);
    std::int64_t rent = 0;
    for (const auto& e : w.takeEvents())
        rent += e.kind == "economy" && e.detail == "rent" && e.actor == till && e.target == landlord ? e.coins : 0;
    expect(rent == 15, "It pays its landlord 15% of the week's takings (" + std::to_string(rent) + "p)");
    expect(s.conserved(), "Money stays conserved");
}

// The owner's profit (doc 64, Phase 5): a business keeps its working money (a month of its staffing and materials), and
// on its own day of the month the rest goes to its owner (here its keeper); on its own day of the week, one short of a
// week's working money is topped up by its owner.
void theOwnersProfit()
{
    auto f = strip("EE.....WW", resident("cook", "Pell Ladle", "merchant", "keeps the cookshop", 7, 9.5, 8, 9.5));
    auto w = load(f);
    w.tick(.6);
    nextMorning(w);
    auto& s = w.society();
    const auto till = s.tillOf("cook");
    expect(till.rfind("till:", 0) == 0, "The cookshop has its own till");
    const auto pid = till.substr(5);
    expect(s.ownerOfBusiness(pid) == "cook", "and its keeper owns it");
    const auto working = s.workingMoney(pid);
    expect(working > 0, "It keeps working money (" + std::to_string(working) + "p)");
    // To the evening before its day of the month, with a fine month's takings in the till.
    auto saved = w.save();
    saved.calendarDays = MonthDays + Society::ownersDay(pid) - 1 + 23. / 24;
    expect(w.restore(saved).ok, "To the eve of its day of the month");
    w.tick(.6);
    s.shift("treasury", till, "", 0, working + 600, "test: a fine month's takings");
    w.takeEvents();
    run(w, 600 * 2);
    std::int64_t profit = 0;
    for (const auto& e : w.takeEvents())
        if (e.kind == "economy" && e.actor == till && e.detail == "the owner's profit" && e.target == "cook")
            profit += e.coins;
    expect(profit > 0, "Its keeper takes the month's profit (" + std::to_string(profit) + "p)");
    // (Its working money, and what it has sold in the hours since.)
    expect(s.account(till)->cash <= s.workingMoney(pid) + 50, "and the till keeps its working money (" + std::to_string(s.workingMoney(pid)) + "p): " +
                                                                  std::to_string(s.account(till)->cash) + "p");
    // Run dry the evening before its day of the week: its keeper puts money in.
    saved = w.save();
    const auto today = std::int64_t(std::floor(saved.calendarDays));
    std::int64_t next = today + 1;
    while (next % 7 != Society::ownersWeekday(pid))
        ++next;
    saved.calendarDays = double(next) - 1 + 23. / 24;
    expect(w.restore(saved).ok, "To the eve of its day of the week");
    w.tick(.6);
    s.shift(till, "treasury", "", 0, s.account(till)->cash, "test: a bad week");
    s.shift("treasury", "cook", "", 0, 500, "test: the keeper's savings");
    w.takeEvents();
    run(w, 600 * 2);
    std::int64_t invested = 0;
    for (const auto& e : w.takeEvents())
        if (e.kind == "economy" && e.actor == "cook" && e.detail == "an owner's investment" && e.target == till)
            invested += e.coins;
    expect(invested > 0, "Run dry, its owner tops it up (" + std::to_string(invested) + "p)");
    expect(s.conserved(), "Money stays conserved");
}

// Children's stipends (the user, 2026-10-05): with work by the hours, a child living with grown family is given a little
// each day out of the household's want fund, only what it can spare over the household rule's target and the week's tax
// and tithe; and children spend a little, on a trinket (doc 64, Phase 13: never the baker's flour and firewood). With
// the hours off, the old pace, as it was: a penny more for every hundred spared, and what it buys kept.
void childrensStipends()
{
    Society::setWorkByHours(true);
    for (const bool comfortable : {true, false})
    {
        auto kid = resident("kid", "Pip Small", "civilian", "plays in the streets", 0, 9.5, 0, 9.5);
        kid.replace(kid.find(" 30 "), 4, " 8 ");    // (Eight years old.)
        auto f = strip("EE.....WW", kid + resident("shop", "Tess Sweet", "merchant", "baker at The Sweet Crust", 0, 10.5, 1, 9.5) +
                                        resident("trinkets", "Bel Bauble", "merchant", "keeps the jeweller's", 0, 11.5, 1, 10.5));
        auto w = load(f);
        w.tick(.6);
        if (comfortable)
            w.society().shift("treasury", "e1", "", 0, 1000, "test: a good year");
        else
            for (const auto* who : {"e1", "e2", "e3", "e4", "e5", "em", "shop", "trinkets"})   // (Everyone grown at home.)
                w.society().shift(who, "treasury", "", 0, w.society().account(who)->cash, "test: a hard year");
        w.takeEvents();
        bool given = false, spent = false, trinkets = true, materials = false;
        for (int d = 0; d < (comfortable ? 2 : 1); ++d)   // (The poor one's wages would soon let it spare a little.)
        {
            nextMorning(w);
            // (Fed: a hungry child buys a bite instead, RatwDemand.cpp.)
            if (!Society::hasFood(*w.society().account("kid")))
                w.society().create("kid", "porridge", 2, "test: breakfast");
            run(w, 2);
            for (const auto& e : w.takeEvents())
            {
                given |= e.kind == "economy" && e.detail == "a child's stipend" && e.target == "kid";
                if (e.kind == "economy" && e.detail == "a child's spending" && e.actor == "kid")
                {
                    // (Jewellery, a hat, a scarf, paw wear or a toy: what the jeweller sells, never the baker's.)
                    const auto* good = items::good(items::baseOf(e.item));
                    const auto c = good ? good->category : std::string();
                    spent = true;
                    trinkets = trinkets && e.target == w.society().tillOf("trinkets") &&
                               (c == "jewelry" || c == "hat" || c == "scarf" || c == "paw_wear" || c == "luxury");
                }
                const auto base = items::baseOf(e.item);
                materials |= e.kind == "economy" && (e.actor == "kid" || e.target == "kid") && (base == "flour" || base == "firewood");
            }
        }
        if (comfortable)
        {
            expect(given, "A comfortable household gives its child a stipend");
            expect(spent, "and the child spends some of it");
            expect(trinkets, "on trinkets, every one from the jeweller");
            expect(!materials, "and never the baker's flour or firewood");
        }
        else
            expect(!given, "A household with nothing to spare after food and tax gives none");
        expect(w.society().conserved(), "Money stays conserved");
    }
    Society::setWorkByHours(false);
    auto kid = resident("kid", "Pip Small", "civilian", "plays in the streets", 0, 9.5, 0, 9.5);
    kid.replace(kid.find(" 30 "), 4, " 8 ");
    auto f = strip("EE.....WW", kid + resident("shop", "Tess Sweet", "merchant", "baker at The Sweet Crust", 0, 10.5, 1, 9.5) +
                                    resident("trinkets", "Bel Bauble", "merchant", "keeps the jeweller's", 0, 11.5, 1, 10.5));
    auto w = load(f);
    w.tick(.6);
    w.society().shift("treasury", "e1", "", 0, 1000, "test: a good year");
    w.takeEvents();
    nextMorning(w);
    if (!Society::hasFood(*w.society().account("kid")))
        w.society().create("kid", "porridge", 2, "test: breakfast");
    run(w, 2);
    std::int64_t stipend = 0;
    for (const auto& e : w.takeEvents())
        if (e.kind == "economy" && e.detail == "a child's stipend" && e.target == "kid")
            stipend = std::max(stipend, e.coins);
    expect(stipend > Society::MostStipend, "At the old pace, as it was: a penny more for every hundred spared (" + std::to_string(stipend) + "p)");
    expect(w.society().conserved(), "Money stays conserved");
}

// Wants (the user, 2026-10-05; doc 64, Phase 13): a household with money to spare buys things it simply wants, one a day
// by whichever of its grown members' turn it is (a piece of finery, a treat, a drink, scent, a pastime); one with nothing
// to spare after food and tax doesn't.
void grownWolvesBuyWhatTheyWant()
{
    const auto ofTheHousehold = [](const std::string& who) {
        return who == "e1" || who == "e2" || who == "e3" || who == "e4" || who == "e5" || who == "em";
    };
    auto f = strip("EE.....WW", resident("shop", "Tess Sweet", "merchant", "baker at The Sweet Crust", 0, 10.5, 1, 9.5) +
                                    resident("trinkets", "Bel Bauble", "merchant", "keeps the jeweller's", 0, 11.5, 1, 10.5));
    auto w = load(f);
    w.tick(.6);
    // (A windfall goes on wants a little at a time: the household rule's share of what it holds over its month's
    // income and ten days' needs, a2 a month, half of it to its want fund. Doc 64, Phase 7.)
    w.society().shift("treasury", "e1", "", 0, std::min<std::int64_t>(5000, w.society().account("treasury")->cash / 2), "test: a good year");
    w.takeEvents();
    bool rich = false;
    for (int d = 0; d < 7 && !rich; ++d)
    {
        nextMorning(w);
        run(w, 2);
        for (const auto& e : w.takeEvents())
            rich |= e.kind == "economy" && e.detail.rfind("a want: ", 0) == 0 && ofTheHousehold(e.actor);
    }
    expect(rich, "A well-off household buys something it wants");
    // (They all share one household, whose purse passes money round: penniless, the whole household.)
    auto hard = load(f);
    hard.tick(.6);
    for (const char* who : {"e1", "e2", "e3", "e4", "e5", "em", "shop", "trinkets"})
        if (hard.society().account(who))
            hard.society().shift(who, "treasury", "", 0, hard.society().account(who)->cash, "test: a hard year");
    hard.takeEvents();
    nextMorning(hard);
    run(hard, 2);
    bool poor = false;
    for (const auto& e : hard.takeEvents())
        poor |= e.kind == "economy" && e.detail.rfind("a want: ", 0) == 0 && ofTheHousehold(e.actor);
    expect(!poor, "A penniless one doesn't");
    // Over eight grown under one roof (a bunkhouse, a barracks) share no purse, and each is its own household by the rule:
    // a windfall goes on its wants too (doc 64, "The households' gap": a bunkhouse's purses only filled).
    auto nine = strip("EE.....WW", resident("shop", "Tess Sweet", "merchant", "baker at The Sweet Crust", 0, 10.5, 1, 9.5) +
                                       resident("trinkets", "Bel Bauble", "merchant", "keeps the jeweller's", 0, 11.5, 1, 10.5) +
                                       resident("e9", "Ned Bunk", "civilian", "works at The Sweet Crust", 0, 12.5, 1, 9.5));
    auto bunk = load(nine);
    bunk.tick(.6);
    bunk.society().shift("treasury", "e1", "", 0, std::min<std::int64_t>(5000, bunk.society().account("treasury")->cash / 2), "test: a good year");
    bunk.takeEvents();
    bool lodger = false;
    for (int d = 0; d < 7 && !lodger; ++d)
    {
        nextMorning(bunk);
        run(bunk, 2);
        for (const auto& e : bunk.takeEvents())
            lodger |= e.kind == "economy" && e.detail.rfind("a want: ", 0) == 0 && e.actor == "e1";
    }
    expect(lodger, "A well-off wolf of a bunkhouse buys something it wants");
    expect(bunk.society().conserved(), "Money stays conserved");
    expect(hard.society().conserved(), "Money stays conserved");
    expect(w.society().conserved(), "Money stays conserved");
}

// Households (the user, 2026-10-05): the grown members share one purse; a household comfortable for a week keeps one at
// home (shopping, minding, about the town), and sends it back to work when money runs low.
void householdsShareAndKeepHouse()
{
    auto f = strip("EE.....WW");
    auto w = load(f);
    w.tick(.6);
    w.society().shift("treasury", "e1", "", 0, 3000, "test: a windfall");
    nextMorning(w);
    expect(w.society().account("e2")->cash > 300 && w.society().account("e1")->cash < 1500, "The household shares its purse (e1 " +
               std::to_string(w.society().account("e1")->cash) + "p, e2 " + std::to_string(w.society().account("e2")->cash) + "p)");
    std::string keeper;
    for (int d = 0; d < 20 && keeper.empty(); ++d)   // (A day's skip here tends the households every other time.)
    {
        nextMorning(w);
        const auto& kept = w.society().state().memory.keeper;
        if (const auto found = kept.find(w.society().resident("e1")->homeCell); found != kept.end())
            keeper = found->second;
    }
    expect(!keeper.empty(), "After a comfortable week one of them keeps the house");
    {
        auto saved = w.save();
        saved.calendarDays = std::floor(saved.calendarDays) + 1.5;   // (Mid-morning, of a working day.)
        while (calendar::weekdayOf(saved.calendarDays) == calendar::Restday)
            saved.calendarDays += 1;
        expect(w.restore(saved).ok, "To the morning");
    }
    run(w, 30);
    const auto task = w.society().resident(keeper)->task;
    expect(task == "shopping for the household" || task == "minding the children" || task == "about the town",
           keeper + " keeps the house: " + task + " (" + w.society().resident(keeper)->reason + ")");
    // The money runs low: back to work. (Its shopkeeper's purse is the household's too, since its shop keeps a till of its
    // own: doc 46.)
    for (const auto* who : {"e1", "e2", "e3", "e4", "e5", "em"})
        w.society().shift(who, "treasury", "", 0, w.society().account(who)->cash, "test: a bad year");
    for (int d = 0; d < 3 && w.society().state().memory.keeper.count(w.society().resident("e1")->homeCell); ++d)
        nextMorning(w);
    expect(!w.society().state().memory.keeper.count(w.society().resident("e1")->homeCell), "Money gone, it goes back to work");
    expect(w.society().conserved(), "Money stays conserved");
}

// Residents fill contracts for goods (doc 42, Phase 4): after a day for the players, someone out of work in the town that
// wants them fetches them from a shop that has them to spare (here, in another town), carries them back and hands them
// in; the shop is paid its price from the reward, the carrier the rest.
void residentsFillContractsForGoods()
{
    std::string folk;
    for (int n = 0; n < 100; ++n)
        folk += resident("ex" + std::to_string(n), "East Folk " + std::to_string(n), "civilian", "working", 0, 2.5 + n % 12, 0, 2.5 + n % 12);
    auto f = strip("EE.....WW", folk + resident("carter", "Cart Wright", "civilian", "-", 0, 14.5, 0, 14.5) +
                                    resident("store", "Toll Keeper", "merchant", "shopkeeper at The West Store", 7, 13.5, 8, 12.5));
    auto w = load(f);
    w.tick(.6);
    w.society().create(w.society().tillOf("store"), "stone", 40, "test: a quarry's load");
    std::string kid;
    for (int d = 0; d < 4 && kid.empty(); ++d)
    {
        nextMorning(w);
        run(w, 2);
        for (const auto& c : w.roads().contracts)
            if (c.kind == "procure" && c.status == "open" && c.town == "east" && c.item == "stone")
                kid = c.id;
    }
    expect(!kid.empty(), "East's Town Works asks for stone");
    // (Its till's: what the keeper's purse does is its own, and it tops up its till when it runs short, doc 64.)
    const auto storeTill = w.society().tillOf("store");
    std::int64_t paidShop = 0;
    // A day on, nobody (no player) has taken it: a resident does.
    nextMorning(w);
    run(w, 2);
    const Contract* k = nullptr;
    for (const auto& c : w.roads().contracts)
        if (c.id == kid)
            k = &c;
    expect(k && k->status == "taken" && !k->source.empty() && w.entity(k->taker) && !w.society().jobOf(k->taker),
           "A wolf out of work takes it on (" + (k ? k->taker + " from " + k->source : std::string()) + ")");
    const auto taker = k->taker;
    // Going to another town, it is given food for the road out of the reward (the user, 2026-10-05).
    expect(!Society::bestFood(*w.society().account(taker)).empty() || w.society().account(taker)->cash > 0,
           "and is given provisions for the road");
    w.roads().camps.clear();
    bool done = false, paid = false;
    for (int t = 0; t < 6000 && !done; ++t)
    {
        w.tick(1);
        for (const auto& e : w.takeEvents())
        {
            done |= e.kind == "contract done" && e.detail.find("stone") != std::string::npos;
            paid |= e.kind == "economy" && e.detail == "carrier's pay" && e.target == taker && e.coins > 0;
            paidShop += e.kind == "economy" && e.detail == "sold on a contract" && e.target == storeTill ? e.coins : 0;
        }
    }
    expect(done, "fetches the stone from the shop in the west and delivers it");
    expect(Society::stockAll(*w.society().account("town:east:works"), "stone") > 0, "The stone reaches the Town Works");
    expect(paidShop > 0, "the shop is paid its price");
    expect(paid, "and the carrier the rest");
    expect(w.society().conserved(), "Money stays conserved");
}

// Phase 5's buyers (doc 42): tools a trade wears out, bought from the smith out of the worker's purse; a stables' horses
// eating hay from its till; the church's bread given to the hungry poor; pennies for a beggar.
void tradesWearAndCharity()
{
    std::string folk;
    for (int n = 0; n < 100; ++n)
        folk += resident("ex" + std::to_string(n), "East Folk " + std::to_string(n), "civilian", "working", 0, 2.5 + n % 12, 0, 2.5 + n % 12);
    const auto unpaid = [](std::string line) { line.replace(line.find(" 1 8 17 "), 8, " 0 8 17 "); return line; };
    auto f = strip("EE.....WW", folk + resident("miner", "Ore Digger", "civilian", "digs ore at the Hill Workings", 0, 13.5, 0, 13.5) +
                                    resident("smith", "Anvil Ring", "merchant", "smith at The Gate Forge", 0, 14.5, 1, 12.5) +
                                    resident("ostler", "Hay Rake", "merchant", "keeps the stables at the gate", 0, 14.5, 1, 13.5) +
                                    resident("farmer", "Barley Field", "civilian", "farms the valley fields", 0, 9.5, 0, 9.5) +
                                    unpaid(resident("beggar", "Poor Tom", "civilian", "begging in the market", 0, 8.5, 1, 4.5)) +
                                    // (Alone at home in east's second cell: no household purse to share with.)
                                    resident("lone", "Lone Wren", "civilian", "working", 1, 15.5, 1, 15.5));
    auto w = load(f);
    w.tick(.6);
    w.society().shift("treasury", "miner", "", 0, 60, "test: savings");
    w.society().shift("treasury", "em", "", 0, 200, "test: a good year");
    w.society().create(w.society().tillOf("smith"), "mining_pick", 3, "test: forged");
    w.society().create("farmer", "hay", 20, "test: the barn");
    w.society().shift("treasury", "ostler", "", 0, 100, "test: takings");
    std::map<std::string, int> seen;
    // (A pick lasts 15 days of spells, worn by the hours worked: Society::produce, doc 64, Phase 3b. A pick's worth of
    // the mine's work, as these days are only their mornings.)
    w.society().wearTools(w.society().tillOf("miner"), "mine", 15 * Society::PaidSpells);
    for (int d = 0; d < 16; ++d)
    {
        nextMorning(w);
        run(w, 2);
        for (const auto& e : w.takeEvents())
            if (e.kind == "economy")
                ++seen[e.detail + "|" + e.target + "|" + e.actor];
    }
    // (Each business out of its till, since doc 46's Phase 2: the mine's, the smithy's, the stables'.)
    const auto& soc = w.society();
    expect(seen.count("tools for the work|" + soc.tillOf("smith") + "|" + soc.tillOf("miner")),
           "The miner buys a pick from the smith, out of its mine's till");
    expect(seen.count("worn out at work|consumed|" + soc.tillOf("miner")), "and wears it out at work");
    bool fed = false, penny = false;
    for (const auto& [k, n] : seen)
    {
        fed |= k.rfind("eaten by the horses|consumed|" + soc.tillOf("ostler"), 0) == 0;
        penny |= k.rfind("a penny for a beggar|beggar|", 0) == 0;
    }
    expect(fed, "The stables' horses eat hay, bought from the farmer");
    expect(penny, "The beggar is given pennies");
    // The church's bread goes to a hungry wolf with nothing (one living alone: a household would share its purse).
    w.society().shift("lone", "treasury", "", 0, w.society().account("lone")->cash, "test: penniless");
    for (const auto& [item, n] : std::map<std::string, int>(w.society().account("lone")->stock.begin(), w.society().account("lone")->stock.end()))
        w.society().consume("lone", item, n, "test: nothing to eat");
    for (const auto& kind : Society::StoreKinds)
        if (const auto* store = w.society().account(Society::homeStore(w.society().resident("lone")->homeCell, kind)))
            for (const auto& [item, n] : std::map<std::string, int>(store->stock.begin(), store->stock.end()))
                w.society().consume(Society::homeStore(w.society().resident("lone")->homeCell, kind), item, n, "test: an empty larder");
    w.society().create("town:east:church", "bread", 10, "test: baked");
    bool alms = false;
    for (int d = 0; d < 3 && !alms; ++d)
    {
        nextMorning(w);
        run(w, 2);
        for (const auto& e : w.takeEvents())
            alms |= e.kind == "economy" && e.detail == "alms" && e.actor == "town:east:church";
    }
    expect(alms, "The church's bread is given to the hungry poor");
    expect(w.society().conserved(), "Money stays conserved");
}

// Trade between towns (doc 42, Phase 7): east's mill has flour to spare and west's bakery has none; a trader's caravan
// buys it in east with the treasury's money, carries it and the change to west, sells it to the bakery a little dearer,
// and brings the takings home.
void tradeCaravansCarryGoodsAndMoney()
{
    auto f = strip("EE.....WW", resident("miller", "Flour Dust", "merchant", "keeps the water mill at Eastford", 0, 9.5, 1, 9.5) +
                                    resident("baker", "Bram Loaf", "merchant", "baker at The Amber Loaf", 7, 9.5, 8, 9.5));
    auto w = load(f);
    w.tick(.6);
    w.roads().camps.clear();                       // (No bandits this time.)
    const auto mill = w.society().tillOf("miller"), bakery = w.society().tillOf("baker");
    w.society().create(mill, "flour", 60, "test: a good harvest");
    for (const auto& kind : Society::kindsHeld(*w.society().account(bakery), "flour"))
        w.society().consume(bakery, kind, Society::stock(*w.society().account(bakery), kind), "test: a bad week");
    const auto west = w.society().tillOf("wm");
    for (const auto& kind : Society::kindsHeld(*w.society().account(west), "flour"))
        w.society().consume(west, kind, Society::stock(*w.society().account(west), kind), "test: a bad week");
    w.society().shift("treasury", bakery, "", 0, 200, "test: takings");
    nextMorning(w);
    w.roads().camps.clear();
    const Caravan* trade = nullptr;
    for (const auto& c : w.roads().caravans)
        if (!c.trader.empty() && c.from == "east" && c.to == "west")
            trade = &c;
    expect(trade != nullptr, "A trader's caravan sets out from east for west");
    // On a standing order (the user, 2026-10-05): the bakery buys flour from east week after week.
    const auto order = std::find_if(w.roads().orders.begin(), w.roads().orders.end(),
                                    [&](const StandingOrder& o) { return o.buyer == bakery && o.item == "flour" && o.from == "east"; });
    expect(order != w.roads().orders.end() && order->perWeek > 0 && order->price > 0, "on a standing order for flour, at an agreed price");
    const auto id = trade->id;
    expect(Society::stockAll(*w.society().account(trade->account), "flour") > 0, "carrying flour bought from the mill");
    const auto bakeryCash = w.society().account(bakery)->cash;
    bool sold = false, home = false;
    std::int64_t takings = 0;
    for (int t = 0; t < 6000 && !home; ++t)
    {
        w.tick(1);
        for (const auto& e : w.takeEvents())
        {
            sold |= e.kind == "economy" && (e.detail == "a standing order" || e.detail == "carted in by caravan") &&
                    e.target == trade->account && e.actor == bakery;
            if (e.kind == "economy" && e.detail == "trade takings")
                takings += e.coins;
            home |= e.kind == "caravan home" && e.actor == id;
        }
    }
    expect(sold && w.society().account(bakery)->cash < bakeryCash, "It sells the flour to west's bakery, which pays for it");
    expect(Society::stockAll(*w.society().account(bakery), "flour") > 0, "and the bakery has flour again");
    expect(home && takings > 0, "The takings ride home to the trader (" + std::to_string(takings) + "p)");
    expect(w.society().conserved(), "Money stays conserved");
}

// The church's care (doc 42, Phase 5): a poor wolf with healing wounds is tended free by a chapel keeper, with the
// church's bandages, once a day: a quarter of each wound's rest, never all of it.
void theChurchCares()
{
    auto f = strip("EE.....WW", resident("chap", "Cleric Bell", "civilian", "keeps the Down Chapel", 0, 11.5, 0, 11.5));
    auto w = load(f);
    w.tick(.6);
    auto& ada = w.addPlayer("player-ada", "Ada");
    const auto* chap = w.entity("chap");
    ada.cellId = chap->cellId;
    ada.position = {chap->position.x + 1, chap->position.y};
    expect(w.addInjury("player-ada", "cracked_rib", 2, "", "a bandit").ok, "Ada has a cracked rib");
    const auto before = w.entity("player-ada")->injuries.front().restLeft;
    w.society().shift("player-ada", "treasury", "", 0, w.society().account("player-ada")->cash - 5, "test: down to her last pennies");
    expect(w.churchCares("player-ada", "chap"), "Poor and hurt, she may ask the chapel keeper for care");
    const auto church = w.churchStoreOf("chap");    // (Its town's church's storehouse: its goods are its own.)
    expect(!w.churchCare("player-ada", "chap").ok, "but not with no bandages in the church");
    w.society().openAccount(church);
    w.society().create(church, "bandages", 2, "test: the apothecary's");
    const auto r = w.churchCare("player-ada", "chap");
    const auto& rib = w.entity("player-ada")->injuries.front();
    expect(r.ok && rib.restLeft < before && rib.restLeft >= 1, "The keeper binds her wounds: less rest needed, not none (" + r.message + ")");
    expect(Society::stockAll(*w.society().account(church), "bandages") == 1, "with one of the church's bandages");
    expect(!w.churchCare("player-ada", "chap").ok, "once a day");
    w.society().shift("treasury", "player-ada", "", 0, 100, "test: a purse");
    expect(!w.churchCares("player-ada", "chap"), "A wolf who can pay a healer isn't offered it");
}

// Town repair (doc 42): a town's buildings wear a point a day; the Town Works mends them with the materials it uses.
void townsWearAndAreMended()
{
    std::string folk;
    for (int n = 0; n < 100; ++n)
        folk += resident("ex" + std::to_string(n), "East Folk " + std::to_string(n), "civilian", "working", 0, 2.5 + n % 12, 0, 2.5 + n % 12);
    auto f = strip("EE.....WW", folk);
    auto w = load(f);
    w.tick(.6);
    for (int d = 0; d < 3; ++d)
    {
        nextMorning(w);
        run(w, 2);
    }
    const auto worn = w.society().condition("east");
    expect(worn < 100, "With nothing to mend them with, east's buildings wear (" + std::to_string(worn) + ")");
    // Materials in the Works' stores: it mends.
    for (const auto* item : {"stone", "timber", "limestone", "cord", "lime", "planks", "nails", "bricks", "roof_tiles", "iron_bar", "fittings", "chain", "lock"})
        w.society().create("town:east:works", item, 40, "test: a cartload");
    for (int d = 0; d < 3; ++d)
    {
        nextMorning(w);
        run(w, 2);
    }
    expect(w.society().condition("east") > worn, "With materials, the Town Works mends them (" + std::to_string(w.society().condition("east")) + ")");
    World again = load(f);
    expect(again.restore(w.save()).ok && std::abs(again.society().condition("east") - w.society().condition("east")) < 1e-6,
           "The town's repair is saved");
}

// The economy's monitor (doc 46; doc 64, Phase 12): it measures every evening and weighs the week the evening of the
// weekly reckoning, after the taxes and tithes; nothing it measures moves a penny; and its measures are saved with the
// society. And the Dungeon Master's levers: the tax rate, the mint, a relief grant, each in the ledger.
void theMonitorWatches()
{
    auto f = strip("EE.....WW");
    auto w = load(f);
    w.tick(.6);
    auto saved = w.save();
    saved.calendarDays = 6 + 22. / 24;              // The evening before the week's reckoning (day 7).
    expect(w.restore(saved).ok, "To the eve of the reckoning");
    w.tick(.6);
    auto& s = w.society();
    const auto supply = s.moneySupply();
    run(w, 600 * 3);                                 // (A game hour is 600 seconds: past 23:00 and the turn of day 7.)
    const auto& o = s.orchestrator();
    expect(o.last.day == 7 && !o.last.decided && !o.last.towns.empty(), "Day 7's measures, from the evening before: " + std::to_string(o.last.day));
    expect(o.decision.day < 0, "and no week weighed yet");
    run(w, 600 * 24);                                // (Past the reckoning's evening and the turn of day 8.)
    expect(o.last.day == 8 && o.last.decided && o.decision.day == 8, "The evening of the reckoning, it weighs the week");
    expect(!o.decision.towns.empty() && o.decision.towns.front().week >= 0, "and each town's week");
    expect(o.last.moneySupply > 0 && o.last.gini >= 0 && o.last.bottomShare >= 0 && o.last.bottomShare <= 1, "and how the land's money is spread");
    expect(s.conserved() && s.moneySupply() == supply, "Nothing it measures moves a penny");
    const auto back = wire::readSociety(wire::society(s.state()));
    expect(back.orchestrator.decision.day == 8 && back.orchestrator.last.day == 8 && back.orchestrator.memory.decided == 8,
           "Its measures are saved");

    // The levers.
    const auto ledgered = [&](const std::string& kind) {
        for (const auto& e : s.state().ledger)
            if (e.kind.rfind(kind, 0) == 0)
                return true;
        return false;
    };
    expect(s.taxRate() == Society::TaxRate, "The tax rate starts at its constant");
    expect(!s.setTaxRate(.9, "dm:test").ok && s.taxRate() == Society::TaxRate, "A tax rate past half is refused");
    expect(s.setTaxRate(.1, "dm:test").ok && s.taxRate() == .1 && ledgered("the tax rate set from 8% to 10%"), "A Dungeon Master sets it, in the ledger");
    expect(wire::readSociety(wire::society(s.state())).memory.taxRate == .1, "and it is saved");
    expect(s.setMint(false, "dm:test").ok && !s.state().memory.mint.on && ledgered("the mint turned off"), "The mint turned off, in the ledger");
    expect(s.setMint(true, "dm:test").ok && s.state().memory.mint.on, "and on again");
    const auto west = s.treasuryOf("west"), capital = s.treasuryOf(s.capital());
    expect(west != capital && s.account(west), "West has a treasury of its own");
    const auto before = s.account(west)->cash, held = s.account(capital)->cash;
    expect(!s.reliefGrant("west", held + 1).ok && !s.reliefGrant(s.capital(), 10).ok, "No grant past what the capital holds, nor to itself");
    expect(s.reliefGrant("west", 25).ok && s.account(west)->cash == before + 25 && s.account(capital)->cash == held - 25 && ledgered("a relief grant"),
           "A relief grant moves coins from the capital's treasury to the town's, in the ledger");
    expect(s.conserved() && s.moneySupply() == supply, "Every penny is kept");
}

// The realm's pace, steadied (doc 64, Phase 13), with work by the hours: each turn of the day it takes the monitor's
// reading of the realm's food in hand (homes and shelves, days a head, a town's at most PaceCoverCap), with its day.
// Until it has a reading from exactly a week ago it only holds; then it steps back when the week's food fell by more
// than PaceSlide, goes on when it didn't (the strip has no land to keep up with); and back faster while the realm goes
// hungry.
void thePaceIsSteadied()
{
    Society::setWorkByHours(true);
    auto f = strip("EE.....WW");
    auto w = load(f);
    w.tick(.6);
    using Readings = std::vector<std::pair<std::int64_t, double>>;
    const auto evening = [&](double day, Readings cover, bool hungry) {
        auto saved = w.save();
        saved.calendarDays = day + 22. / 24;
        saved.society.memory.pace = .5;
        saved.society.memory.paceCover = std::move(cover);
        if (hungry)                                  // (Hungry, and nothing to eat at home or about them.)
            for (auto& [id, life] : saved.society.residents)
            {
                life.hunger = 95;
                for (const auto& key : {id, Society::homeStore(life.homeCell, "larder")})
                    if (const auto a = saved.society.accounts.find(key); a != saved.society.accounts.end())
                        for (auto it = a->second.stock.begin(); it != a->second.stock.end();)
                            it = Society::edible(it->first) ? a->second.stock.erase(it) : std::next(it);
            }
        expect(w.restore(saved).ok, "To an evening");
        run(w, 600 * 3);                             // (Past 23:00 and the turn of the day.)
        return w.society().state().memory;
    };
    auto m = evening(3, {}, false);
    const auto& last = w.society().orchestrator().last;
    double held = 0, people = 0;
    for (const auto& t : last.towns)
        held += t.people * std::min(Society::PaceCoverCap, t.shopFoodDays + t.homeFoodDays), people += t.people;
    expect(last.day == 4 && m.paceCover.size() == 1 && people > 0 && m.paceCover.back().first == 4 &&
               std::abs(m.paceCover.back().second - held / people) < 1e-9,
           "At the turn of day 4, a reading: the monitor's homes and shelves, days a head (" + std::to_string(held / people) + ")");
    expect(m.pace == .5, "and with no week of them yet, the pace holds");
    // (A week's readings before the turn after `day`'s evening: a week ago first, then each day since; `missed` a day the
    // monitor missed.)
    const auto week = [](std::int64_t day, double weekAgo, double since, std::int64_t missed = -1000) {
        Readings v;
        for (std::int64_t d = day + 1 - (Society::PaceWeek - 1); d <= day; ++d)
            if (d != missed)
                v.push_back({d, d == day + 1 - (Society::PaceWeek - 1) ? weekAgo : since});
        return v;
    };
    m = evening(4, week(4, Society::PaceCoverCap, Society::PaceCoverCap), false);
    expect(m.paceCover.size() == std::size_t(Society::PaceWeek) && m.paceCover.back().second < Society::PaceCoverCap - Society::PaceSlide &&
               std::abs(m.pace - (.5 - Society::PaceStep)) < 1e-9,
           "A week in, with more food a week ago: the pace steps back (" + std::to_string(m.pace) + ")");
    m = evening(5, week(5, 0, Society::PaceCoverCap), false);
    expect(std::abs(m.pace - (.5 + Society::PaceStep)) < 1e-9,
           "With more now than a week ago, it goes on: a realm with no land to keep up with keeps up (" + std::to_string(m.pace) + ")");
    m = evening(6, week(6, 0, Society::PaceCoverCap, 6 + 1 - (Society::PaceWeek - 1)), false);
    expect(std::abs(m.pace - .5) < 1e-9 && m.paceCover.size() == std::size_t(Society::PaceWeek - 1),
           "With last week's same day missed, there's no week to weigh: it holds (" + std::to_string(m.pace) + ")");
    m = evening(7, week(7, 0, Society::PaceCoverCap, 5), false);
    expect(std::abs(m.pace - (.5 + Society::PaceStep)) < 1e-9, "A day missed between doesn't move the week's ends: it goes on");
    // (A week of readings a fortnight old, from before the server was away: dropped, and nothing to weigh.)
    m = evening(16, week(7, 0, Society::PaceCoverCap), false);
    expect(std::abs(m.pace - .5) < 1e-9 && m.paceCover.size() == 1 && m.paceCover.front().first == 17,
           "Readings older than the week are dropped, and it holds (" + std::to_string(m.paceCover.size()) + " left)");
    m = evening(17, week(17, 0, Society::PaceCoverCap), true);
    expect(std::abs(m.pace - (.5 - Society::PaceBack)) < 1e-9, "and while the realm goes hungry it steps back faster (" + std::to_string(m.pace) + ")");
    // (Within PaceNoise of a week ago the week's food didn't fall: a reading wanders by a few hundredths. Doc 64, Phase 13.)
    {
        const auto base = w.save();
        const auto fellBy = [&](double fall, double* now) {
            auto saved = base;
            saved.calendarDays = 20 + 22. / 24;
            saved.society.memory.pace = .5;
            saved.society.memory.paceCover = week(20, (now ? *now : 0) + fall, Society::PaceCoverCap);
            for (auto& [id, life] : saved.society.residents)
                life.hunger = 0;                     // (Fed again, after the hungry evening above.)
            expect(w.restore(saved).ok, "To day 20's evening");
            run(w, 600 * 3);
            if (now)
                return w.society().state().memory.pace;
            return w.society().state().memory.paceCover.back().second;   // (Today's reading, from the same evening each time.)
        };
        double now = fellBy(0, nullptr);
        const double little = fellBy(Society::PaceNoise / 2, &now), more = fellBy((Society::PaceNoise + Society::PaceSlide) / 2, &now);
        expect(std::abs(little - (.5 + Society::PaceStep)) < 1e-9,
               "Food down a few hundredths of a day a head on the week: it didn't fall, and the pace goes on (" + std::to_string(little) + ")");
        expect(std::abs(more - .5) < 1e-9, "down by more, though under PaceSlide: it holds (" + std::to_string(more) + ")");
    }
    expect(w.society().conserved(), "Nothing it reads moves a penny");
    Society::setWorkByHours(false);
}

// And with land (doc 64, Phase 13): a rabbit farm on the strip, its holder its one hand, its shelf bare (a producer's goods
// go on to the shops). Its sales wanting a hand's day, the land's hands keep up and the pace goes on, though the farm,
// planning half again for its bare shelf, hires; its sales wanting three hands' days, they don't, and it holds. But with
// no work for more hands (its store full, its work in hand full at two turns of the day), it counts at the hand it has,
// whatever its sales want (it lets hands go, not hires them): the pace goes on.
void thePaceStepsUpWithTheLand()
{
    Society::setWorkByHours(true);
    auto f = strip("EE.....WW", resident("rk", "Rook Hutch", "civilian", "keeps rabbits by the road", 0, 9.5, 1, 9.5));
    auto w = load(f);
    w.tick(.6);
    auto& s = w.society();
    const auto till = s.tillOf("rk");
    const auto* farm = items::producerFor("keeps rabbits by the road");
    const auto* rabbit = items::good("rabbit");
    expect(farm && rabbit && s.account(till), "The rabbit farm keeps a till");
    using Readings = std::vector<std::pair<std::int64_t, double>>;
    // An evening: its sales over the day enough for `hours` of its work a day by now, its shelf bare at the turn.
    const auto evening = [&](std::int64_t day, double hours) {
        auto saved = w.save();
        saved.calendarDays = double(day) + 22. / 24;
        saved.society.memory.pace = .5;
        saved.society.memory.paceCover.clear();
        for (std::int64_t d = day + 1 - (Society::PaceWeek - 1); d <= day; ++d)
            saved.society.memory.paceCover.push_back({d, d == day + 1 - (Society::PaceWeek - 1) ? 0. : Society::PaceCoverCap});
        // (Its hours a day for rabbits sold, at the pace, three a yield: the rate those take, after the day's average.)
        const double old = std::max(1., farm->seconds / 3600.), perYield = old + .5 * (farm->hours - old);
        const double rate = hours * 3 / perYield / (rabbit->staple ? Society::StapleSlack : 1.);
        const auto* rates = s.sellRates(till);
        const double before = rates && rates->count("rabbit") ? rates->at("rabbit") : 0.;
        const int n = std::max(0, int(std::lround((rate - .7 * before) / .3)));
        saved.society.accounts.at(till).stock["rabbit"] = n;
        saved.society.accounts.at(till).stock.erase("small_pelt");
        saved.society.accounts.at("treasury").cash -= n;
        saved.society.accounts.at("e1").cash += n;
        expect(w.restore(saved).ok, "To an evening");
        expect(n == 0 || s.sale(till, "e1", "rabbit", n, 1, "test: the day's rabbits"), "the day's rabbits sold");
        run(w, 600 * 3);
        return s.state().memory.pace;
    };
    // (A hand's day: 6 hours. Half again would be 9, two hands.)
    double pace = .5;
    for (std::int64_t d = 3; d < 6; ++d)
        pace = evening(d, 6);
    expect(std::abs(pace - (.5 + Society::PaceStep)) < 1e-9 && s.handsWanted(till) >= 2 && s.handsWanted(till, false) == 1,
           "Its sales a hand's day, the land keeps up and the pace goes on (" + std::to_string(pace) + "; it plans for " +
               std::to_string(s.handsWanted(till)) + ", sells for " + std::to_string(s.handsWanted(till, false)) + ")");
    for (std::int64_t d = 6; d < 9; ++d)
        pace = evening(d, 24);
    expect(std::abs(pace - .5) < 1e-9 && s.handsWanted(till, false) >= 3,
           "Its sales three hands' days with only its own, the land falls behind: it holds (" + std::to_string(pace) + ")");
    // Its store full and no money to hire with, for days: its work in hand fills, and is full at two turns of the day.
    // (Its premises improved, so its hours go further; its folk fed, and its till emptied, each evening.)
    {
        auto saved = w.save();
        saved.calendarDays = 9 + 7. / 24;
        auto& farmStock = saved.society.accounts.at(till);
        farmStock.stock["rabbit"] = 900;
        farmStock.stock["small_pelt"] = 900;
        saved.society.memory.improved[s.jobOf("rk")->id] = Society::MostImprovement;
        expect(w.restore(saved).ok, "To a morning, the farm's store full");
    }
    const auto until = [&](double day) {
        while (w.calendarDays() < day)
            w.tick(1);
    };
    for (std::int64_t d = 9; d <= 13; ++d)
    {
        until(double(d) + 22. / 24);
        auto state = s.state();
        for (auto& [id, life] : state.residents)
            life.hunger = life.fatigue = 0;
        state.accounts.at("treasury").cash += state.accounts.at(till).cash;
        state.accounts.at(till).cash = 0;
        state.memory.pace = .5;
        state.memory.paceCover.clear();
        for (std::int64_t r = d + 1 - (Society::PaceWeek - 1); r <= d; ++r)
            state.memory.paceCover.push_back({r, r == d + 1 - (Society::PaceWeek - 1) ? 0. : Society::PaceCoverCap});
        expect(s.restore(state), "The evening: its folk fed, its till emptied");
    }
    {
        // (Its sales three hands' days.)
        const double old = std::max(1., farm->seconds / 3600.), perYield = old + .5 * (farm->hours - old);
        const double rate = 24 * 3 / perYield / (rabbit->staple ? Society::StapleSlack : 1.);
        const auto* rates = s.sellRates(till);
        const double before = rates && rates->count("rabbit") ? rates->at("rabbit") : 0.;
        const int n = std::max(0, int(std::lround((rate - .7 * before) / .3)));
        expect(n == 0 || s.shift("treasury", "e1", "", 0, n, "test: the day's money"), "the day's money");
        for (int left = n; left > 0; left -= 99)    // (A ledger entry holds 99 at most.)
            expect(s.sale(till, "e1", "rabbit", std::min(left, 99), 1, "test: the day's rabbits"), "the day's rabbits sold");
    }
    until(14 + 1. / 24);
    pace = s.state().memory.pace;
    expect(s.idleBusiness(till) && s.handsWanted(till, false) >= 3 && std::abs(pace - (.5 + Society::PaceStep)) < 1e-9,
           "Its sales three hands' days, but no work for more: at its one, the land keeps up and the pace goes on (" + std::to_string(pace) + "; its sales " +
               std::to_string(s.handsWanted(till, false)) + " hands, its work in hand " + std::to_string(s.workBankedOf("rk")) + ")");
    expect(s.conserved(), "Every penny is kept");
    Society::setWorkByHours(false);
}

// One price for a good in a town (doc 46, Phase 3): the orchestrator's, the same at every shop of the town, a quality at
// its share of it; and what the land is paid for it, that price times the orchestrator's margin.
void sellersPriceFromCost()
{
    // Each seller its own price (doc 64, Phase 4): a good it makes at its normal cost (the batch's hours at the town's
    // labourer's wage, its materials at what it would pay, shared among the batch) and its margin, 15% to start; the
    // land paid its normal cost; the town's going price the good's worth at its wage.
    auto f = strip("EE.....WW", resident("baker1", "Bram Loaf", "merchant", "baker at The Amber Loaf", 0, 4.5, 1, 4.5) +
                                    resident("baker2", "Tilly Crust", "merchant", "baker at The Morning Oven", 0, 5.5, 1, 5.5));
    auto w = load(f);
    w.tick(.6);
    auto& s = w.society();
    const double day = s.labourDay("east");
    const double worth = items::worthOf("bread") * day;
    const double cost = s.normalCost("baker1", "bread");
    expect(std::abs(s.townPrice("east", "bread") - worth) < 1e-9, "The going price is the good's worth at the town's wage");
    expect(std::abs(cost * 1.15 - worth) < worth * .05, "A baker's loaf, from its hours and flour, sits on the ladder: " +
                                                           std::to_string(cost * 1.15) + "p against " + std::to_string(worth) + "p");
    expect(s.shopPrice("baker1", "bread") == Society::pennies(cost * 1.15), "and sells at its cost and 15%");
    expect(std::abs(s.offerAt("baker1", "bread") - cost) < 1e-9, "Bought from the land at its normal cost");
    const auto* bread = items::good("bread");
    const auto* fine = items::good("bread~fine");
    expect(bread && fine && std::abs(s.priceAt("baker1", "bread~fine") - s.priceAt("baker1", "bread") * fine->price / bread->price) < 1e-6,
           "A fine loaf at its quality's share of it");
    auto state = s.state();
    state.memory.margins[s.tillOf("baker1") + "|bread"] = .6;
    expect(s.restore(state), "A seller's margin restores");
    expect(std::abs(s.priceAt("baker1", "bread") - s.normalCost("baker1", "bread") * 1.6) < 1e-6 && std::abs(s.priceAt("baker2", "bread") - s.normalCost("baker2", "bread") * 1.15) < 1e-6,
           "Each bakery prices by its own margin");
    expect(std::abs(s.buyingPrice("east", "bread") - worth / 1.15) < 1e-9, "The town's buyers pay the land the going price less the margin");
}

// A town's market is in the town (the user, 2026-10-12): west's two shops stand side by side, and out in its fields,
// eighty tiles off, a mill has two merchants of its own. Caravans load at the shops, not at the mill, though the mill
// has more merchants than either shop; and the town's Marketday is held there too.
void aTownsMarketIsInTheTown()
{
    const std::string extra = resident("wb", "Bram Reed", "merchant", "keeping the shop", 3, 9.5, 5, 4.5) +
                              resident("mill1", "Mill One", "merchant", "keeping the mill", 9, 3.5, 9, 8.5) +
                              resident("mill2", "Mill Two", "merchant", "keeping the mill", 9, 4.5, 9, 9.5);
    auto f = strip("EE.WWWWWWW", extra);
    auto w = load(f);
    w.tick(.6);
    const Town* west = nullptr;
    for (const auto& t : w.towns())
        if (t.id == "west")
            west = &t;
    expect(west != nullptr, "West is a town");
    expect(west->market == id(4), "West's market is its shop in the town, not the mill in its fields: " + west->market);
}

// The Houses keep to their trades (Docs/Design/61; Data/Economy/houses.json). In the capital: House Fell (timber),
// Grayrock Hall (stone and iron), Mossgrave Court (wool and cloth), and the Rain Hall, whose caretaker is no House.
// A sawmill is Fell's by its trade; a forge named for Fell is Fell's by its name, though forges are Grayrock's trade;
// a foundry is Grayrock's; a weaver is Mossgrave's; a bakery is its keeper's own. A save from before, in which the
// Rain Hall was a House and owned the bakery, is sorted out once: the bakery its keeper's, the Rain Hall's money the
// treasury's, every penny still there.
void housesKeepToTheirTrades()
{
    const auto unpaid = [](std::string line) { line.replace(line.find(" 1 8 17 "), 8, " 0 8 17 "); return line; };
    auto f = strip("EE.....WW",
                   unpaid(resident("fell", "Lord Fell", "civilian", "ruling House Fell", 0, 9.5, 0, 9.5)) +
                   unpaid(resident("gray", "Lady Grayrock", "civilian", "keeping Grayrock Hall", 0, 10.5, 0, 10.5)) +
                   unpaid(resident("moss", "Lord Mossgrave", "civilian", "keeping Mossgrave Court", 0, 11.5, 0, 11.5)) +
                   unpaid(resident("rain", "Old Gerrit", "civilian", "keeping the Rain Hall", 0, 12.5, 0, 12.5)) +
                   resident("saw", "Saw Master", "merchant", "runs The Old Sawmill", 0, 13.5, 1, 5.5) +
                   resident("forge", "Fell Smith", "merchant", "smith at The Fell Forge", 0, 14.5, 1, 6.5) +
                   resident("found", "Bell Founder", "merchant", "runs The Bell Foundry", 0, 3.5, 1, 7.5) +
                   resident("weave", "Weaver Wren", "merchant", "weaver at The Wool Loft", 0, 4.5, 1, 10.5) +
                   resident("bake", "Baker Bryn", "merchant", "baker at The Loaf", 0, 5.5, 1, 12.5));
    auto w = load(f);
    w.tick(.6);
    nextMorning(w);
    run(w, 2);
    auto& soc = w.society();
    std::set<std::string> ids;
    for (const auto& h : soc.houses())
        ids.insert(h.id + ":" + h.trade);
    expect(ids == std::set<std::string>{"house:house_fell:fell", "house:grayrock_hall:grayrock", "house:mossgrave_court:mossgrave"},
           "Three great houses, each with its trade; the Rain Hall's caretaker is no House");
    const auto owner = [&](const std::string& who) { return soc.ownerOf(soc.jobOf(who)->id); };
    expect(owner("saw") == "house:house_fell", "A sawmill is Fell's, by its trade: " + owner("saw"));
    expect(owner("forge") == "house:house_fell", "A forge named for Fell is Fell's, by its name: " + owner("forge"));
    expect(owner("found") == "house:grayrock_hall", "A foundry is Grayrock's: " + owner("found"));
    expect(owner("weave") == "house:mossgrave_court", "A weaver is Mossgrave's: " + owner("weave"));
    expect(owner("bake").empty(), "A bakery is its keeper's own: " + owner("bake"));
    // A save from before doc 61: the Rain Hall a House, with money, owning the bakery.
    auto old = soc.state();
    old.houses.plan.clear();
    old.houses.owner[soc.jobOf("bake")->id] = "house:the_rain_hall";
    old.accounts["house:the_rain_hall"].cash = 500;
    old.accounts["treasury"].cash -= 500;
    expect(soc.restore(old), "The old save restores");
    nextMorning(w);
    run(w, 2);
    expect(owner("bake").empty(), "Sorted out: the bakery is its keeper's again");
    expect(!soc.account("house:the_rain_hall") || soc.account("house:the_rain_hall")->cash == 0, "The Rain Hall keeps no money");
    expect(!soc.state().houses.plan.empty() && owner("saw") == "house:house_fell", "and the rest stay as they were");
    expect(soc.conserved(), "Money is conserved");
}

// A town's trade on the roads (Docs/Design/61): iron is Frosthollow's. Its smelter has 30 bars and can make no more
// today; the smiths of east and west have none. A town promises only what it has to spare, but a town whose trade the
// good is keeps making it all week, so it may promise twice that: both smiths sign standing orders with Frosthollow.
void aTradeTownSuppliesTheRoads()
{
    std::string extra = resident("smelter", "Rune Ore", "merchant", "runs The Frost Smelter", 3, 9.5, 4, 9.5) +
                        resident("eastsmith", "Edda Anvil", "merchant", "smith at The East Forge", 0, 12.5, 1, 12.5) +
                        resident("westsmith", "Wyn Anvil", "merchant", "smith at The West Forge", 7, 12.5, 8, 12.5);
    for (int n = 1; n <= 5; ++n)
        extra += resident("f" + std::to_string(n), "Frost " + std::to_string(n), "civilian", "working", 3, 3.5 + n, 3, 3.5 + n);
    auto f = strip("EE.FF..WW", extra);
    auto w = load(f);
    w.tick(.6);
    auto& soc = w.society();
    const auto clear = [&](const std::string& who, const std::string& item) {
        const auto till = soc.tillOf(who);
        for (const auto& kind : Society::kindsHeld(*soc.account(till), item))
            soc.consume(till, kind, Society::stock(*soc.account(till), kind), "test: none left");
    };
    for (const auto* item : {"iron_ore", "charcoal", "iron_bar", "timber"})
        clear("smelter", item);
    clear("eastsmith", "iron_bar");
    clear("westsmith", "iron_bar");
    soc.create(soc.tillOf("smelter"), "iron_bar", 30, "test: the week's bars");
    nextMorning(w);
    std::set<std::string> buyers;
    for (const auto& o : w.roads().orders)
        if (o.item == "iron_bar" && o.from == "frosthollow")
            buyers.insert(o.buyer);
    expect(buyers.count(soc.tillOf("eastsmith")) && buyers.count(soc.tillOf("westsmith")),
           "Both smiths order their iron from Frosthollow (" + std::to_string(buyers.size()) + " orders)");
    expect(soc.conserved(), "Money is conserved");
}

// A maker wants one store of each material, whatever it goes into (doc 61): Frosthollow's own smith, with fourteen
// crafts that take iron, wants a store of bars, not fourteen; counted fourteen times over, it left its town nothing to
// spare, and no other town ever had iron from Frosthollow.
void aSmithWantsOneStoreOfIron()
{
    std::string extra = resident("smelter", "Rune Ore", "merchant", "runs The Frost Smelter", 3, 9.5, 4, 9.5) +
                        resident("frostsmith", "Hild Anvil", "merchant", "smith at The Pick and Hammer", 3, 10.5, 4, 12.5) +
                        resident("westsmith", "Wyn Anvil", "merchant", "smith at The West Forge", 7, 12.5, 8, 12.5);
    for (int n = 1; n <= 5; ++n)
        extra += resident("f" + std::to_string(n), "Frost " + std::to_string(n), "civilian", "working", 3, 3.5 + n, 3, 3.5 + n);
    auto f = strip("EE.FF..WW", extra);
    auto w = load(f);
    w.tick(.6);
    auto& soc = w.society();
    const auto clear = [&](const std::string& who, const std::string& item) {
        const auto till = soc.tillOf(who);
        for (const auto& kind : Society::kindsHeld(*soc.account(till), item))
            soc.consume(till, kind, Society::stock(*soc.account(till), kind), "test: none left");
    };
    for (const auto* item : {"iron_ore", "charcoal", "iron_bar", "timber"})
        clear("smelter", item);
    clear("frostsmith", "iron_bar");
    clear("westsmith", "iron_bar");
    soc.create(soc.tillOf("smelter"), "iron_bar", 60, "test: the week's bars");
    nextMorning(w);
    bool west = false;
    for (const auto& o : w.roads().orders)
        west |= o.item == "iron_bar" && o.from == "frosthollow" && o.buyer == soc.tillOf("westsmith");
    expect(west, "and the west's smith has its iron from Frosthollow");
}

// A town with nobody free to go (doc 61): an order due is agreed again by the trader whose caravan brings it. (The towns'
// people all have work; no porter ever went, and their iron stayed at the few bars a week of its first day.)
void theTraderAgreesItAgain()
{
    std::string extra = resident("smelter", "Rune Ore", "merchant", "runs The Frost Smelter", 3, 9.5, 4, 9.5) +
                        resident("westsmith", "Wyn Anvil", "merchant", "smith at The West Forge", 7, 12.5, 8, 12.5);
    for (int n = 1; n <= 5; ++n)
        extra += resident("f" + std::to_string(n), "Frost " + std::to_string(n), "civilian", "working", 3, 3.5 + n, 3, 3.5 + n);
    auto f = strip("EE.FF..WW", extra);
    auto w = load(f);
    w.tick(.6);
    w.roads().camps.clear();
    auto& soc = w.society();
    const auto smith = soc.tillOf("westsmith");
    for (const auto& kind : Society::kindsHeld(*soc.account(smith), "iron_bar"))
        soc.consume(smith, kind, Society::stock(*soc.account(smith), kind), "test: none left");
    soc.create(soc.tillOf("smelter"), "iron_bar", 60, "test: the week's bars");
    soc.shift("treasury", smith, "", 0, 500, "test: takings");
    nextMorning(w);
    auto order = std::find_if(w.roads().orders.begin(), w.roads().orders.end(),
                              [&](const StandingOrder& o) { return o.buyer == smith && o.item == "iron_bar"; });
    expect(order != w.roads().orders.end(), "The west's smith orders iron from Frosthollow; its caravan sets out");
    const auto id = order->id;
    order->review = 0;                             // (Due.)
    w.takeEvents();
    bool agreed = false, sent = false;
    for (int t = 0; t < 20000 && !agreed; ++t)
    {
        w.tick(1);
        for (const auto& e : w.takeEvents())
        {
            sent |= e.kind == "sent to renegotiate";
            agreed |= e.kind == "standing order renegotiated" && e.target == smith && e.detail.find("with the trader") != std::string::npos;
        }
    }
    expect(!sent && agreed, "Nobody in the west is free to go: the trader agrees it again when the caravan comes");
    order = std::find_if(w.roads().orders.begin(), w.roads().orders.end(), [&](const StandingOrder& o) { return o.id == id; });
    expect(order == w.roads().orders.end() || order->review > w.calendarDays(), "and it is not due again at once");
    expect(soc.conserved(), "Money is conserved");
}

// A town's makers keep what the roads take of them (doc 61): Frosthollow's two smelters, their bars ordered 180 a week,
// smelt past their usual store (twice a supplier's, a trade good's), each toward its share of a week of the orders, so
// the caravan has them to take; not each the whole of it (the city's smiths had each kept the towns' week of nails).
// And a house's business keeps a week of its own orders' cost in its till, not swept to the house each night.
void makersKeepWhatIsOrdered()
{
    std::string extra = resident("smelter", "Rune Ore", "merchant", "runs The Frost Smelter", 3, 9.5, 4, 9.5) +
                        resident("smelter2", "Bera Ore", "merchant", "runs The Low Smelter", 3, 10.5, 4, 10.5) +
                        resident("westsmith", "Wyn Anvil", "merchant", "smith at The West Forge", 7, 12.5, 8, 12.5);
    for (int n = 1; n <= 5; ++n)
        extra += resident("f" + std::to_string(n), "Frost " + std::to_string(n), "civilian", "working", 3, 3.5 + n, 3, 3.5 + n);
    auto f = strip("EE.FF..WW", extra);
    auto w = load(f);
    w.tick(.6);
    auto& soc = w.society();
    const std::string smelters[] = {soc.tillOf("smelter"), soc.tillOf("smelter2")};
    for (const auto& smelter : smelters)
    {
        for (int n = 0; n < 4; ++n)
            soc.create(smelter, "iron_ore", 99, "test: a good month at the mine");
        for (int n = 0; n < 2; ++n)
            soc.create(smelter, "charcoal", 99, "test: the burners' month");
    }
    StandingOrder o;
    o.id = "so-test";
    o.buyer = soc.tillOf("westsmith");
    o.town = "west";
    o.item = "iron_bar";
    o.from = "frosthollow";
    o.perWeek = 180;
    o.price = 12;
    o.review = 1e9;
    w.roads().orders.push_back(o);
    nextMorning(w);
    const int usual = int(Society::SuppliesKept * items::tradeKeep()), share = 180 / 2 + Society::SuppliesKept / 2;
    int most[2] = {0, 0};
    for (int t = 0; t < 14 * 3600 && (most[0] < share || most[1] < share); ++t)
    {
        w.tick(1);
        for (int k = 0; k < 2; ++k)
            most[k] = std::max(most[k], Society::stockAll(*soc.account(smelters[k]), "iron_bar"));
    }
    for (int k = 0; k < 2; ++k)
        expect(most[k] > usual && most[k] <= share, "Each smelter smelts past its usual store of bars, to its share (" +
                                                       std::to_string(most[k]) + ": usual " + std::to_string(usual) + ", share " +
                                                       std::to_string(share) + ")");
    expect(soc.conserved(), "Money is conserved");
}

void aHousesTillKeepsItsOrders()
{
    const auto unpaid = [](std::string line) { line.replace(line.find(" 1 8 17 "), 8, " 0 8 17 "); return line; };
    auto f = strip("EE.....WW", unpaid(resident("gray", "Lady Grayrock", "civilian", "keeping Grayrock Hall", 0, 10.5, 0, 10.5)) +
                                    resident("forge", "Ash Smith", "merchant", "smith at The Gray Forge", 0, 14.5, 1, 6.5));
    auto w = load(f);
    w.tick(.6);
    nextMorning(w);
    run(w, 2);
    auto& soc = w.society();
    const auto position = soc.jobOf("forge")->id;
    expect(soc.ownerOf(position) == "house:grayrock_hall", "The forge is Grayrock's");
    const auto before = soc.floatOf(position);
    soc.setStandingOrders({}, {{"till:" + position, 400}});
    expect(soc.floatOf(position) == before + 400, "and its till keeps a week of its standing orders besides (" +
                                                    std::to_string(soc.floatOf(position)) + "p, from " + std::to_string(before) + "p)");
}

// An order catches up with what its shop uses (doc 61): signed on the first day, before a smith has sold anything, it is
// for a few bars; it is looked at again a week on (not four), and raised to a week of the smith's use, and looked at
// again a week after that, till it has caught up. (Left a month, the towns' smiths had 8 bars a week while they used 30.)
void anOrderCatchesUpWithUse()
{
    std::string extra = resident("smelter", "Rune Ore", "merchant", "runs The Frost Smelter", 3, 9.5, 4, 9.5) +
                        resident("westsmith", "Wyn Anvil", "merchant", "smith at The West Forge", 7, 12.5, 8, 12.5) +
                        resident("porter", "Pell Strap", "civilian", "carries loads for hire", 7, 11.5, 7, 11.5);
    for (int n = 1; n <= 5; ++n)
        extra += resident("f" + std::to_string(n), "Frost " + std::to_string(n), "civilian", "working", 3, 3.5 + n, 3, 3.5 + n);
    auto f = strip("EE.FF..WW", extra);
    auto w = load(f);
    w.tick(.6);
    w.roads().camps.clear();
    auto& soc = w.society();
    const auto smith = soc.tillOf("westsmith");
    const auto clear = [&](const std::string& till, const std::string& item) {
        for (const auto& kind : Society::kindsHeld(*soc.account(till), item))
            soc.consume(till, kind, Society::stock(*soc.account(till), kind), "test: none left");
    };
    for (const auto* item : {"iron_ore", "charcoal", "iron_bar", "timber"})
        clear(soc.tillOf("smelter"), item);
    clear(smith, "iron_bar");
    soc.create(soc.tillOf("smelter"), "iron_bar", 60, "test: the week's bars");
    soc.shift("treasury", smith, "", 0, 500, "test: takings");
    nextMorning(w);
    auto order = std::find_if(w.roads().orders.begin(), w.roads().orders.end(),
                              [&](const StandingOrder& o) { return o.buyer == smith && o.item == "iron_bar"; });
    expect(order != w.roads().orders.end(), "The west's smith orders iron from Frosthollow");
    const auto id = order->id;
    const int first = order->perWeek;
    expect(order->review - order->since <= 7.01, "to be looked at again in a week (" + std::to_string(order->review - order->since) + " days)");
    // A week on (the order due), a busy day: nails by the hundred (a bar makes six), counted into the smith's sales at the
    // turn of the day, when the porter is sent. (A day passes as nextMorning's does, the sale made after the restore,
    // which doesn't keep a day's sales.)
    auto saved = w.save();
    saved.calendarDays = std::floor(saved.calendarDays) + 1.3;
    expect(w.restore(saved).ok, "A day passes");
    soc.create(smith, "nails", 96, "test: made");
    soc.shift("treasury", "porter", "", 0, 96, "test: a builder's errand");
    expect(soc.sale(smith, "porter", "nails", 96, 1, "test: sold"), "A busy day: the smith sells 96 nails");
    soc.consume("porter", "nails", 96, "test: a roof mended");
    order = std::find_if(w.roads().orders.begin(), w.roads().orders.end(), [&](const StandingOrder& o) { return o.id == id; });
    order->review = 0;
    w.takeEvents();
    for (int t = 0; t < 3; ++t)
        w.tick(1);
    const int weekly = int(std::ceil(soc.useRate(smith, "iron_bar") * 7));
    expect(weekly > first, "The smith now uses more than the order brings (" + std::to_string(weekly) + " a week, against " +
                               std::to_string(first) + ")");
    order = std::find_if(w.roads().orders.begin(), w.roads().orders.end(), [&](const StandingOrder& o) { return o.id == id; });
    expect(order != w.roads().orders.end() && order->negotiator == "porter", "The order is due: the porter is sent");
    bool agreed = false;
    for (int t = 0; t < 8000 && !agreed; ++t)
    {
        w.tick(1);
        for (const auto& e : w.takeEvents())
            agreed |= e.kind == "standing order renegotiated" && e.target == smith;
    }
    expect(agreed, "The porter agrees it again at Frosthollow");
    order = std::find_if(w.roads().orders.begin(), w.roads().orders.end(), [&](const StandingOrder& o) { return o.id == id; });
    // (Twice over at most, doc 64, Phase 13: a city potter's clay went from 23 a week to 1,133 at once.)
    expect(order != w.roads().orders.end() && order->perWeek > first && order->perWeek <= 2 * first,
           "raised toward a week of the smith's use, twice over at most (" +
               std::to_string(order == w.roads().orders.end() ? 0 : order->perWeek) + ", from " + std::to_string(first) + ")");
    expect(order->review - w.calendarDays() <= 7.01, "and looked at again in a week, being raised, till it has caught up");
    expect(soc.conserved(), "Money is conserved");
}

// A caravan carries the whole of its orders (doc 64, Phase 13): 99 at a time, as a ledger entry holds, from one holder
// and to one buyer, not one sale a quality (Larkfield's fifteen farms sent the city 1,485 oats a week, whatever was
// ordered); and short of a good, each order has its share of what came, not the first signed all it wanted.
void caravansCarryWholeOrders()
{
    std::string extra = resident("smelter", "Rune Ore", "merchant", "runs The Frost Smelter", 3, 9.5, 4, 9.5) +
                        resident("westsmith", "Wyn Anvil", "merchant", "smith at The West Forge", 7, 12.5, 8, 12.5) +
                        resident("lowsmith", "Wil Anvil", "merchant", "smith at The Low Forge", 7, 13.5, 8, 13.5);
    for (int n = 1; n <= 5; ++n)
        extra += resident("f" + std::to_string(n), "Frost " + std::to_string(n), "civilian", "working", 3, 3.5 + n, 3, 3.5 + n);
    auto f = strip("EE.FF..WW", extra);
    auto w = load(f);
    w.tick(.6);
    w.roads().camps.clear();
    auto& soc = w.society();
    const auto clear = [&](const std::string& till, const std::string& item) {
        for (const auto& kind : Society::kindsHeld(*soc.account(till), item))
            soc.consume(till, kind, Society::stock(*soc.account(till), kind), "test: none left");
    };
    const auto smelter = soc.tillOf("smelter");
    const std::string smiths[] = {soc.tillOf("westsmith"), soc.tillOf("lowsmith")};
    for (const auto* item : {"iron_ore", "charcoal", "iron_bar", "timber"})
        clear(smelter, item);
    for (const int n : {99, 99, 30})
        soc.create(smelter, "iron_bar", n, "test: a month's bars");
    expect(soc.account("stores:frosthollow") && soc.shift("treasury", "stores:frosthollow", "", 0, 4500, "test: the town's trade money"),
           "Frosthollow's trader has the money for a big load");
    for (const auto& smith : smiths)
    {
        clear(smith, "iron_bar");
        soc.shift("treasury", smith, "", 0, 600, "test: takings");
        StandingOrder o;
        o.id = "so-" + smith;
        o.buyer = smith;
        o.town = "west";
        o.item = "iron_bar";
        o.from = "frosthollow";
        o.perWeek = 120;
        o.price = 5;
        o.review = 1e9;
        w.roads().orders.push_back(o);
    }
    // On its road's day of the week (another wagon, with only the day's new orders, home first).
    const auto onTheRoad = [&](bool iron) -> const Caravan* {
        for (const auto& c : w.roads().caravans)
            if (!c.trader.empty() && c.from == "frosthollow" && c.to == "west" && c.status != "arrived" &&
                (Society::stockAll(*soc.account(c.account), "iron_bar") > 0) == iron)
                return &c;
        return nullptr;
    };
    const Caravan* trip = nullptr;
    for (int d = 0; d < 9 && !trip; ++d)
    {
        nextMorning(w);
        w.roads().camps.clear();
        trip = onTheRoad(true);
        for (int t = 0; t < 30000 && !trip && onTheRoad(false); ++t)
            w.tick(1);
    }
    expect(trip != nullptr, "The week's caravan sets out from Frosthollow");
    const auto id = trip->id;
    const int loaded = Society::stockAll(*soc.account(trip->account), "iron_bar");
    expect(loaded > 2 * 99 && loaded < 240, "with all the smelter can spare of the 240 ordered, not 99: " + std::to_string(loaded));
    bool sold = false;
    w.takeEvents();
    for (int t = 0; t < 20000 && !sold; ++t)
    {
        w.tick(1);
        for (const auto& e : w.takeEvents())
            sold |= e.kind == "trade caravan sells" && e.actor == id;
    }
    const int a = Society::stockAll(*soc.account(smiths[0]), "iron_bar"), b = Society::stockAll(*soc.account(smiths[1]), "iron_bar");
    expect(sold && a > 99 && b > 99 && std::abs(a - b) <= 1 && a + b == loaded,
           "Each smith has its share, 99 at a time: " + std::to_string(a) + " and " + std::to_string(b) + " of " + std::to_string(loaded));
    expect(soc.conserved(), "Money is conserved");
}

// A food that rots before the week is out (fish keeps three days) isn't ordered by the roads (doc 64, Phase 13): the
// west's fishmonger, out of fish while east's fisher has a catch to spare, signs no standing order for it.
void noRoadOrdersForWhatRots()
{
    auto f = strip("EE.....WW", resident("fisher", "Finn Net", "civilian", "fishes the river", 0, 12.5, 1, 12.5) +
                                    resident("monger", "Mag Scale", "merchant", "fishmonger at The Wet Slab", 7, 12.5, 8, 12.5));
    auto w = load(f);
    w.tick(.6);
    auto& soc = w.society();
    const auto monger = soc.tillOf("monger");
    expect(items::businessFor("fishmonger at The Wet Slab") && items::producerFor("fishes the river"), "a fishmonger and a fisher");
    soc.create(soc.tillOf("fisher"), "fish", 60, "test: a good catch");
    for (const auto& kind : Society::kindsHeld(*soc.account(monger), "fish"))
        soc.consume(monger, kind, Society::stock(*soc.account(monger), kind), "test: sold out");
    soc.shift("treasury", monger, "", 0, 300, "test: takings");
    nextMorning(w);
    const bool fish = std::any_of(w.roads().orders.begin(), w.roads().orders.end(), [](const StandingOrder& o) { return o.item == "fish"; });
    expect(!fish, "No standing order for fish by the week's caravan");
    expect(soc.conserved(), "Money is conserved");
}

// The road from Frosthollow to the west, for the standing orders' tests below (doc 64, Phase 13): Frosthollow's smelter
// with the bars it is given and nothing to make more from, one to three of the west's smiths, their bars gone and money
// in their tills, and Frosthollow's trader (its stores: it has no great house) with the money for a load.
Fixture ironRoad(int smiths)
{
    std::string extra = resident("smelter", "Rune Ore", "merchant", "runs The Frost Smelter", 3, 9.5, 4, 9.5);
    const std::pair<const char*, const char*> forges[] = {{"westsmith", "West"}, {"lowsmith", "Low"}, {"farsmith", "Far"}};
    for (int k = 0; k < smiths && k < 3; ++k)
        extra += resident(forges[k].first, std::string(forges[k].second) + " Anvil", "merchant", std::string("smith at The ") + forges[k].second + " Forge",
                          7, 12.5 + k, 8, 12.5 + k);
    for (int n = 1; n <= 5; ++n)
        extra += resident("f" + std::to_string(n), "Frost " + std::to_string(n), "civilian", "working", 3, 3.5 + n, 3, 3.5 + n);
    return strip("EE.FF..WW", extra);
}

void emptyOf(Society& soc, const std::string& till, const std::string& item)
{
    for (const auto& kind : Society::kindsHeld(*soc.account(till), item))
        soc.consume(till, kind, Society::stock(*soc.account(till), kind), "test: none left");
}

World ironWorld(Fixture& f, int bars)
{
    auto w = load(f);
    w.tick(.6);
    w.roads().camps.clear();
    auto& soc = w.society();
    const auto smelter = soc.tillOf("smelter");
    for (const auto* item : {"iron_ore", "charcoal", "iron_bar", "timber"})
        emptyOf(soc, smelter, item);
    for (int left = bars; left > 0; left -= 99)
        soc.create(smelter, "iron_bar", std::min(left, 99), "test: the month's bars");
    for (const auto* smith : {"westsmith", "lowsmith", "farsmith"})
        if (soc.account(soc.tillOf(smith)) && soc.spec(smith))
        {
            emptyOf(soc, soc.tillOf(smith), "iron_bar");
            soc.shift("treasury", soc.tillOf(smith), "", 0, 300, "test: takings");
        }
    expect(soc.shift("treasury", "stores:frosthollow", "", 0, 4500, "test: the town's trade money"), "Frosthollow's trader has the money for a load");
    return w;
}

StandingOrder ironOrder(const World& w, const std::string& smith, int perWeek)
{
    StandingOrder o;
    o.id = "so-" + smith;
    o.buyer = w.society().tillOf(smith);
    o.town = "west";
    o.item = "iron_bar";
    o.from = "frosthollow";
    o.perWeek = perWeek;
    o.price = 5;
    o.review = 1e9;
    return o;
}

StandingOrder* orderOf(World& w, const std::string& id)
{
    for (auto& o : w.roads().orders)
        if (o.id == id)
            return &o;
    return nullptr;
}

// Days from `day` to the road's week's caravan, its own day of the week (as tradeCaravans draws it).
int dueIn(const std::string& road, double day)
{
    return int((std::hash<std::string>{}(road) % 7 + 7 - std::uint64_t(std::int64_t(std::floor(day)) % 7)) % 7);
}

// A trade caravan on its way from Frosthollow to the west.
bool ironOnTheWay(const World& w)
{
    return std::any_of(w.roads().caravans.begin(), w.roads().caravans.end(), [](const Caravan& c) {
        return !c.trader.empty() && c.from == "frosthollow" && c.to == "west" && c.status == "travelling";
    });
}

// Ticks till no trade caravan is on its way from Frosthollow (or a day's worth, at most); the departures seen.
int tickWhileOnTheWay(World& w, std::vector<WorldEvent>* seen = nullptr)
{
    int departed = 0;
    const auto count = [&] {
        for (const auto& e : w.takeEvents())
        {
            departed += e.kind == "trade caravan departs" && e.target == "west" && e.detail.find("stores:frosthollow") != std::string::npos;
            if (seen)
                seen->push_back(e);
        }
    };
    count();
    for (int t = 0; t < 30000 && ironOnTheWay(w); ++t)
    {
        w.tick(1);
        count();
    }
    return departed;
}

// An order is raised when its seller kept up, a quarter short at most of all it was to send since it was last agreed,
// and no more than twice over at once, nor past what the seller's town has to spare (doc 64, Phase 13: a city potter's
// clay went from 23 a week to 1,133 at once, and Mirebridge sent none of it; and a monthly order its seller sent a little
// short of each week was never raised). The load came for the order before: no shortfall is laid on the raise. And a
// shop with more than two weeks of its use piled up isn't raised by the lowering either (a smith's charcoal went from 18
// a week to 126 so, its use a week over the order).
void anOrderRisesNoFasterThanItsSeller()
{
    enum Case { Raised, NothingToSpare, PiledUp };
    for (const auto which : {Raised, NothingToSpare, PiledUp})
    {
        auto f = ironRoad(1);
        auto w = ironWorld(f, which == NothingToSpare ? 0 : 297);
        auto& soc = w.society();
        const auto smith = soc.tillOf("westsmith");
        if (which == PiledUp)
            for (int n = 0; n < 7; ++n)
                soc.create(smith, "iron_bar", 99, "test: a year's bars");
        auto o = ironOrder(w, "westsmith", 8);
        o.review = 0;                               // (Due: a month's, a quarter short.)
        o.delivered = 21;
        o.shortfall = 7;
        w.roads().orders.push_back(o);
        // Its week's caravan, and the trader agreeing it again on arrival (nobody in the west is free to go); the smith
        // busy meanwhile, nails by the hundred.
        std::vector<WorldEvent> seen;
        bool agreed = false;
        for (int d = 0; d < 9 && !agreed; ++d)
        {
            soc.create(smith, "nails", 96, "test: made");
            soc.shift("treasury", "w1", "", 0, 96, "test: a builder's errand");
            expect(soc.sale(smith, "w1", "nails", 96, 1, "test: sold"), "A busy day: the smith sells 96 nails");
            soc.consume("w1", "nails", 96, "test: a roof mended");
            nextMorning(w);
            w.roads().camps.clear();
            seen.clear();
            tickWhileOnTheWay(w, &seen);
            for (const auto& e : seen)
                agreed |= e.kind == "standing order renegotiated" && e.target == smith && e.detail.find("with the trader") != std::string::npos;
        }
        const auto* agreedAgain = orderOf(w, o.id);
        const int use = int(soc.useRate(smith, "iron_bar") * 7);
        expect(agreed && agreedAgain && use > 16, "The trader agrees the smith's order again when the week's caravan comes, its use " +
                                                      std::to_string(use) + " a week");
        if (which == Raised)
        {
            expect(agreedAgain->perWeek == 16, "raised twice over, no more, toward its use (" + std::to_string(agreedAgain->perWeek) + ")");
            expect(agreedAgain->delivered == 8 && agreedAgain->shortfall == 0,
                   "the eight loaded for it handed over, and nothing short: " + std::to_string(agreedAgain->delivered) + " delivered, " +
                       std::to_string(agreedAgain->shortfall) + " short");
            expect(agreedAgain->review - w.calendarDays() <= 7.01, "and looked at again in a week");
        }
        else
            expect(agreedAgain->perWeek == 8, std::string(which == NothingToSpare ? "Frosthollow with no bars to spare" : "Its bars piled up") +
                                                  ": not raised (" + std::to_string(agreedAgain->perWeek) + ")");
        // (A raise its seller's room held where it was is looked at again in a week, as a raise is; one lowered, or left,
        // in a month.)
        if (which == NothingToSpare)
            expect(agreedAgain->review - w.calendarDays() <= 7.01, "with nothing to spare, its raise is looked at again in a week");
        if (which == PiledUp)
            expect(agreedAgain->review - w.calendarDays() > 27, "piled up, in a month");
        expect(soc.conserved(), "Money is conserved");
    }
}

// An order its seller falls short of (doc 64, Phase 13: Southmeadow's flour with Larkfield brought nothing after its
// first week, and was never cut, nor signed with another town). A week its road's caravan doesn't go (its trader without
// the money) is a week short; and when it is next agreed, an order its seller sent under half of is halved, and one its
// seller sent nothing of in two weeks' worth ends, so another town may be signed: not the one that failed it, for a
// month (it was signed again with Frosthollow the next day, for more). That is saved with the roads.
void anOrderItsSellerCantFillIsCut()
{
    auto f = ironRoad(3);
    auto w = ironWorld(f, 297);
    auto& soc = w.society();
    const std::string road = "frosthollow>west";
    const auto far = soc.tillOf("farsmith");
    soc.create(far, "iron_bar", 40, "test: a full rack");   // (Short only on the road's day: an order new that day.)
    w.roads().orders.push_back(ironOrder(w, "westsmith", 8));
    w.roads().orders.push_back(ironOrder(w, "lowsmith", 8));
    bool dry = false;
    for (int d = 0; d < 9 && !dry; ++d)
    {
        if ((dry = dueIn(road, std::floor(w.calendarDays()) + 1) == 0))
        {
            if (const auto cash = soc.account("stores:frosthollow")->cash; cash > 0)
                soc.shift("stores:frosthollow", "treasury", "", 0, cash, "test: the trade money spent");
            emptyOf(soc, far, "iron_bar");
        }
        nextMorning(w);
        w.roads().camps.clear();
        expect(!dry || !ironOnTheWay(w), "Frosthollow's trader, without a penny, sends no caravan on its road's day");
        tickWhileOnTheWay(w);
    }
    const auto fresh = std::find_if(w.roads().orders.begin(), w.roads().orders.end(), [&](const StandingOrder& o) { return o.buyer == far; });
    expect(dry && orderOf(w, "so-westsmith")->shortfall == 8 && orderOf(w, "so-lowsmith")->shortfall == 8,
           "and each of its orders is a week short: " + std::to_string(orderOf(w, "so-westsmith")->shortfall));
    expect(fresh != w.roads().orders.end() && fresh->shortfall == 0, "but not one signed that day, whose week begins");
    soc.shift("treasury", "stores:frosthollow", "", 0, 4500, "test: the town's trade money");
    auto* cut = orderOf(w, "so-westsmith");
    cut->delivered = 2, cut->shortfall = 6, cut->review = 0;   // (A quarter of what it was to send.)
    auto* none = orderOf(w, "so-lowsmith");
    none->delivered = 0, none->shortfall = 16, none->review = 0;   // (Nothing in two weeks.)
    std::vector<WorldEvent> seen;
    for (int d = 0; d < 9 && orderOf(w, "so-westsmith") && orderOf(w, "so-westsmith")->review <= w.calendarDays(); ++d)
    {
        nextMorning(w);
        w.roads().camps.clear();
        tickWhileOnTheWay(w, &seen);
    }
    const bool ended = std::any_of(seen.begin(), seen.end(), [&](const WorldEvent& e) {
        return e.kind == "standing order ends" && e.target == soc.tillOf("lowsmith");
    });
    expect(orderOf(w, "so-westsmith") && orderOf(w, "so-westsmith")->perWeek == 4,
           "An order its seller sent a quarter of is halved: " + std::to_string(orderOf(w, "so-westsmith") ? orderOf(w, "so-westsmith")->perWeek : 0));
    expect(ended && !orderOf(w, "so-lowsmith"), "and one it sent nothing of in two weeks ends");
    const auto low = soc.tillOf("lowsmith");
    const auto failed = [&](const std::string& till) { return w.roads().failed.count(till + "|iron_bar|frosthollow") > 0; };
    expect(failed(low) && failed(soc.tillOf("westsmith")), "Both sellers' failures are kept");
    // The west's three forges short of bars, with no orders: Frosthollow (the only town with iron) signs the far forge's,
    // never the two it failed.
    w.roads().orders.clear();
    bool again = false, farSigned = false;
    for (int d = 0; d < 3; ++d)
    {
        for (const auto* smith : {"westsmith", "lowsmith", "farsmith"})
        {
            emptyOf(soc, soc.tillOf(smith), "iron_bar");   // (Short of bars, and wanting them.)
            soc.shift("treasury", soc.tillOf(smith), "", 0, 100, "test: takings");
        }
        nextMorning(w);
        w.roads().camps.clear();
        tickWhileOnTheWay(w);
        for (const auto& o : w.roads().orders)
        {
            again |= o.buyer == low || o.buyer == soc.tillOf("westsmith");
            farSigned |= o.buyer == far && o.from == "frosthollow";
        }
    }
    expect(farSigned && !again, "The far forge is signed with Frosthollow; the low and west forges, short again, aren't, with the town that "
                                "failed them");
    {
        checkpoint::ServerState server;
        PersistedWorld back;
        std::string problem;
        const auto key = low + "|iron_bar|frosthollow";
        expect(checkpoint::decode(checkpoint::encode(w.save(), server, {}, 0), back, server, problem) && back.roads.failed.count(key) &&
                   back.roads.failed.at(key) == w.roads().failed.at(key),
               "and its failure is saved with the roads: " + problem);
    }
    expect(soc.conserved(), "Money is conserved");
}

// What a week's caravan couldn't buy for want of money is no sale its town's producers missed (doc 64, Phase 13): its
// money is the load at the town's price, and a burner holding only its finest charcoal asks more; it still has charcoal
// to spare, and its sales a day are what it sold.
void aWagonShortOfMoneyMissesNoSale()
{
    std::string extra = resident("burner", "Fen Ash", "civilian", "burns charcoal", 3, 12.5, 4, 12.5) +
                        resident("fstall", "Moss Bin", "merchant", "keeping the stall", 3, 9.5, 4, 9.5) +   // (Its market's.)
                        resident("westsmith", "West Anvil", "merchant", "smith at The West Forge", 7, 12.5, 8, 12.5);
    for (int n = 1; n <= 5; ++n)
        extra += resident("f" + std::to_string(n), "Frost " + std::to_string(n), "civilian", "working", 3, 3.5 + n, 3, 3.5 + n);
    auto f = strip("EE.FF..WW", extra);
    auto w = load(f);
    w.tick(.6);
    w.roads().camps.clear();
    auto& soc = w.society();
    const auto burner = soc.tillOf("burner");
    const auto finest = items::withQuality("charcoal", 3);
    emptyOf(soc, burner, "charcoal");
    soc.create(burner, finest, 90, "test: a fine burn");
    soc.shift("treasury", soc.tillOf("westsmith"), "", 0, 600, "test: takings");
    expect(soc.shift("treasury", "stores:frosthollow", "", 0, 4500, "test: the town's trade money"), "Frosthollow's trader has money");
    expect(Society::pennies(soc.priceAt(burner, finest)) > Society::pennies(soc.townPrice("frosthollow", "charcoal")) * 1.2,
           "the finest charcoal dearer than the town's price for charcoal: " + std::to_string(soc.priceAt(burner, finest)) + "p, against " +
               std::to_string(soc.townPrice("frosthollow", "charcoal")) + "p");
    auto o = ironOrder(w, "westsmith", 30);
    o.item = "charcoal";
    w.roads().orders.push_back(o);
    int bought = -1;
    for (int d = 0; d < 9 && bought < 0; ++d)
    {
        nextMorning(w);
        w.roads().camps.clear();
        std::vector<WorldEvent> seen;
        tickWhileOnTheWay(w, &seen);
        for (const auto& e : seen)
            if (e.kind == "trade caravan departs" && e.target == "west")
                bought = e.quantity;
    }
    expect(bought > 0 && bought < 30 && Society::stockAll(*soc.account(burner), "charcoal") > 30,
           "The week's caravan buys what its money stretches to: " + std::to_string(bought) + " of 30");
    nextMorning(w);
    const auto* rates = soc.sellRates(burner);
    const double rate = rates && rates->count("charcoal") ? rates->at("charcoal") : 0;
    // (Its running average: the day's sales a third of it, and a day on, seven tenths of that.)
    expect(rate > 0 && rate <= .3 * bought + 1e-9, "and the burner's sales a day are what it sold, none missed: " + std::to_string(rate / .3) +
                                                       " counted, " + std::to_string(bought) + " sold");
    expect(soc.conserved(), "Money is conserved");
}

// A road's week's caravan due a day or two after an order new on it goes with that order's, early (a wagon out wouldn't
// be back for its day), and then not again on its day (doc 64, Phase 13: it went again, each order had two weeks' in two
// days, and its town's producers were told of a week's sales missed that they had made the day before). The road's week
// served is saved through the checkpoint, as an order over the 999 a week it was once cut to is.
void anEarlyCaravanIsTheWeeks()
{
    auto f = ironRoad(2);
    auto w = ironWorld(f, 297);
    auto& soc = w.society();
    const std::string road = "frosthollow>west";
    const auto low = soc.tillOf("lowsmith");
    soc.create(low, "iron_bar", 40, "test: a full rack");   // (Not short yet.)
    w.roads().orders.push_back(ironOrder(w, "westsmith", 4));
    const auto tomorrow = [&] { return dueIn(road, std::floor(w.calendarDays()) + 1); };
    for (int d = 0; d < 9 && (tomorrow() < 1 || tomorrow() > 2); ++d)
    {
        nextMorning(w);
        w.roads().camps.clear();
        tickWhileOnTheWay(w);
    }
    const int early = tomorrow();
    expect(early >= 1 && early <= 2, "The day before one a day or two from the road's own");
    emptyOf(soc, low, "iron_bar");                  // (Short now: an order signed in the morning.)
    orderOf(w, "so-westsmith")->delivered = 0;
    nextMorning(w);
    w.roads().camps.clear();
    std::vector<WorldEvent> seen;
    int departed = tickWhileOnTheWay(w, &seen);
    const bool signed_ = std::any_of(seen.begin(), seen.end(), [&](const WorldEvent& e) { return e.kind == "standing order" && e.actor == low; });
    const auto served = w.roads().sentEarly.find(road);
    expect(signed_ && departed == 1 && served != w.roads().sentEarly.end() && served->second == std::int64_t(std::floor(w.calendarDays())) + early,
           "A new order for the low forge, and the week's caravan goes with it, early, its week served");
    {
        // (Through the checkpoint: the road's week served, and an order of 1,500 a week.)
        orderOf(w, "so-westsmith")->perWeek = 1500;
        checkpoint::ServerState server;
        PersistedWorld back;
        std::string problem;
        const auto doc = checkpoint::encode(w.save(), server, {}, 0);
        expect(checkpoint::decode(doc, back, server, problem), "The checkpoint reads back: " + problem);
        const auto again = back.roads.sentEarly.find(road);
        const auto big = std::find_if(back.roads.orders.begin(), back.roads.orders.end(), [](const StandingOrder& o) { return o.id == "so-westsmith"; });
        expect(again != back.roads.sentEarly.end() && again->second == served->second && big != back.roads.orders.end() && big->perWeek == 1500,
               "its week served is saved, and the order's 1,500");
        orderOf(w, "so-westsmith")->perWeek = 4;
    }
    for (int d = 0; d < early; ++d)
    {
        nextMorning(w);
        w.roads().camps.clear();
        departed += tickWhileOnTheWay(w);
    }
    expect(dueIn(road, w.calendarDays()) == 0 && departed == 1, "On its day it doesn't go again: " + std::to_string(departed) + " caravans");
    expect(orderOf(w, "so-westsmith")->delivered == 4, "and the forge's order had its week once: " + std::to_string(orderOf(w, "so-westsmith")->delivered));
    expect(soc.conserved(), "Money is conserved");
}

// An order agreed the day a world is loaded, before that day's trade (doc 64, Phase 13): what its seller's town has to
// spare isn't known (World::canSpare_, not saved). A raise isn't agreed on an unknown room (an in-process restore read
// the last timeline's, a restart none), and the order stays due: its week's load is handed over, and it is agreed at
// the next caravan.
void anOrderWaitsForItsSellersRoom()
{
    auto f = ironRoad(1);
    auto w = ironWorld(f, 297);
    auto& soc = w.society();
    const auto smith = soc.tillOf("westsmith");
    auto o = ironOrder(w, "westsmith", 8);
    o.review = 0;                                   // (Due: a month's, a quarter short, as one to be raised.)
    o.delivered = 21;
    o.shortfall = 7;
    w.roads().orders.push_back(o);
    const auto busy = [&] {
        soc.create(smith, "nails", 96, "test: made");
        soc.shift("treasury", "w1", "", 0, 96, "test: a builder's errand");
        expect(soc.sale(smith, "w1", "nails", 96, 1, "test: sold"), "A busy day: the smith sells 96 nails");
        soc.consume("w1", "nails", 96, "test: a roof mended");
    };
    bool out = false;
    for (int d = 0; d < 9 && !out; ++d)
    {
        busy();
        nextMorning(w);
        w.roads().camps.clear();
        out = ironOnTheWay(w);
        if (!out)
            tickWhileOnTheWay(w);
    }
    expect(out, "The week's caravan sets out");
    auto saved = w.save();
    expect(w.restore(saved).ok, "The world loaded again while it is on the road");
    std::vector<WorldEvent> seen;
    tickWhileOnTheWay(w, &seen);
    const bool early = std::any_of(seen.begin(), seen.end(), [&](const WorldEvent& e) { return e.kind == "standing order renegotiated" && e.target == smith; });
    const auto* waiting = orderOf(w, o.id);
    expect(!early && waiting && waiting->perWeek == 8 && waiting->review <= w.calendarDays() && waiting->delivered == 21 + 8,
           "Arrived before the day's trade: not agreed, still due, its week handed over (" +
               std::to_string(waiting ? waiting->delivered : -1) + " delivered)");
    bool agreed = false;
    for (int d = 0; d < 9 && !agreed; ++d)
    {
        busy();
        nextMorning(w);
        w.roads().camps.clear();
        seen.clear();
        tickWhileOnTheWay(w, &seen);
        for (const auto& e : seen)
            agreed |= e.kind == "standing order renegotiated" && e.target == smith;
    }
    expect(agreed && orderOf(w, o.id) && orderOf(w, o.id)->perWeek > 8, "and agreed, raised, at the next caravan, its room known");
    expect(soc.conserved(), "Money is conserved");
}

// What the buyer couldn't pay for isn't its seller's shortfall (doc 64, Phase 13: a smith short of cash two weeks had its
// order ended, and signed it again): the week's eight came, its till was empty; nothing delivered, nothing short.
void whatTheBuyerCantPayForIsntShort()
{
    auto f = ironRoad(1);
    auto w = ironWorld(f, 297);
    auto& soc = w.society();
    const auto smith = soc.tillOf("westsmith");
    w.roads().orders.push_back(ironOrder(w, "westsmith", 8));
    bool out = false;
    for (int d = 0; d < 9 && !out; ++d)
    {
        nextMorning(w);
        w.roads().camps.clear();
        out = ironOnTheWay(w);
        if (!out)
            tickWhileOnTheWay(w);
    }
    expect(out, "The week's caravan sets out");
    for (const auto& who : {smith, std::string("westsmith")})   // (Its till, and its keeper, who would top it up.)
        if (const auto cash = soc.account(who)->cash; cash > 0)
            soc.shift(who, "treasury", "", 0, cash, "test: the till emptied");
    tickWhileOnTheWay(w);
    const auto* o = orderOf(w, "so-westsmith");
    expect(o && o->delivered == 0 && o->shortfall == 0, "Its till empty: nothing handed over, and nothing short (" +
                                                           std::to_string(o ? o->shortfall : -1) + ")");
    expect(soc.conserved(), "Money is conserved");
}

// A load is for the orders it set out with, as they were then (doc 64, Phase 13; Caravan::loaded, saved): an order raised
// while the wagon is on the road (by a porter agreeing it at the selling town) has the eight it was loaded for, and
// isn't laid the raise as a shortfall its seller didn't make.
void aLoadIsForTheOrdersItLeftWith()
{
    auto f = ironRoad(1);
    auto w = ironWorld(f, 297);
    auto& soc = w.society();
    w.roads().orders.push_back(ironOrder(w, "westsmith", 8));
    bool out = false;
    for (int d = 0; d < 9 && !out; ++d)
    {
        nextMorning(w);
        w.roads().camps.clear();
        out = ironOnTheWay(w);
        if (!out)
            tickWhileOnTheWay(w);
    }
    const auto wagon = std::find_if(w.roads().caravans.begin(), w.roads().caravans.end(), [](const Caravan& c) {
        return !c.trader.empty() && c.from == "frosthollow" && c.status == "travelling";
    });
    expect(out && wagon != w.roads().caravans.end() && wagon->loaded.count("so-westsmith") && wagon->loaded.at("so-westsmith") == 8,
           "The week's caravan sets out loaded for the order's eight");
    {
        checkpoint::ServerState server;
        PersistedWorld back;
        std::string problem;
        expect(checkpoint::decode(checkpoint::encode(w.save(), server, {}, 0), back, server, problem), "The checkpoint reads back: " + problem);
        const auto again = std::find_if(back.roads.caravans.begin(), back.roads.caravans.end(), [&](const Caravan& c) { return c.id == wagon->id; });
        expect(again != back.roads.caravans.end() && again->loaded == wagon->loaded, "what it was loaded for is saved");
    }
    orderOf(w, "so-westsmith")->perWeek = 16;       // (Raised by a porter at Frosthollow's market meanwhile.)
    tickWhileOnTheWay(w);
    const auto* o = orderOf(w, "so-westsmith");
    expect(o && o->delivered == 8 && o->shortfall == 0, "Its eight handed over, nothing short: " + std::to_string(o ? o->delivered : -1) + " delivered, " +
                                                           std::to_string(o ? o->shortfall : -1) + " short");
    expect(soc.conserved(), "Money is conserved");
}

int main()
{
    try
    {
        caravansCarryWholeOrders();
        noRoadOrdersForWhatRots();
        anOrderRisesNoFasterThanItsSeller();
        anOrderItsSellerCantFillIsCut();
        aWagonShortOfMoneyMissesNoSale();
        anEarlyCaravanIsTheWeeks();
        anOrderWaitsForItsSellersRoom();
        whatTheBuyerCantPayForIsntShort();
        aLoadIsForTheOrdersItLeftWith();
        travelKeepsToTheRoads();
        aTownsMarketIsInTheTown();
        housesKeepToTheirTrades();
        aTradeTownSuppliesTheRoads();
        aSmithWantsOneStoreOfIron();
        anOrderCatchesUpWithUse();
        makersKeepWhatIsOrdered();
        aHousesTillKeepsItsOrders();
        theTraderAgreesItAgain();
        standingOrdersAreRenegotiated();
        shopkeepersDontHoard();
        theOwnersProfit();
        rentOnHouseLand();
        childrensStipends();
        grownWolvesBuyWhatTheyWant();
        householdsShareAndKeepHouse();
        shopsSellTheirGoods();
        townsAndCaravans();
        banditsAndContracts();
        anEscortWhoIsntThere();
        couriersAndSupplies();
        rumoursSpread();
        banditsInPerson();
        banditsCreepUp();
        aWatcherCatchesTheCreep();
        aFight();
        beatenAndRobbed();
        banditsCalled();
        residentsTakeWork();
        tradeAndPrices();
        townBuyersAndContractsForGoods();
        townPursesAndTheReckoning();
        whoPaysWages();
        aLivingForEveryone();
        workingOutOfTown();
        greatHouses();
        noHoarding();
        oneChurch();
        theRelief();
        spoilage();
        residentsFillContractsForGoods();
        tradesWearAndCharity();
        tradeCaravansCarryGoodsAndMoney();
        theChurchCares();
        townsWearAndAreMended();
        theMonitorWatches();
        thePaceIsSteadied();
        thePaceStepsUpWithTheLand();
        sellersPriceFromCost();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Road tests passed: " << checks << " checks.\n";
    return 0;
}
