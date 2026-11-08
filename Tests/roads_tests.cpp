// The roads (RatwRoads.h): towns and their stores, caravans that carry real goods, bandits who take them, contracts,
// and rumours. A strip of nine cells: the capital "east" at one end, "west" at the other, wild country between.
// Nothing sends a caravan of itself since the economy's rules were erased (Docs/Design/66-worlds-atlas-and-dm.md,
// Phase 5): the tests send them (World::sendCaravan, the hook for the rules to come).
#include "test_purse.h"
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
std::map<int, std::string> named;   // Cells given a map's own ids (communitiesStayPlain: a community's interiors).
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
      << "\"female\" \"average\" \"saddle\" 3 1 5 1 1 8 17 \"-\" 40 0 1 \"" << id(home) << "\" " << hx << " 4.5 \"" << id(at)
      << "\" " << wx << " 8.5 \"" << id(home) << "\" " << hx << " 5.5\n";
    return r.str();
}

// A strip of cells in a row, one letter each: 'E' the capital "east", 'M' a town "mid", 'W' the town "west", 'F'
// Frosthollow, 'C' the community Coldwater (Data/Economy/communities.json), '.' wild country. Each town's first cell is where its people live, its second its market.
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
        return c == 'E' ? "east" : c == 'M' ? "mid" : c == 'W' ? "west" : c == 'F' ? "frosthollow" : c == 'C' ? "coldwater" : "wilds";   // (F: a town with a trade.)
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
        {'E', {"em", "Ember Oak"}}, {'M', {"mm", "Moss Hale"}}, {'W', {"wm", "Wren Reed"}}, {'C', {"cm", "Cole Mere"}}};
    for (const auto& [c, who] : merchants)
    {
        const int home = first(c);
        if (home < 0)
            continue;
        const std::string tag = c == 'E' ? "e" : c == 'M' ? "m" : c == 'C' ? "c" : "w";
        m << resident(who.first, who.second, "merchant", "keeping the stall", home, 2.5, home + 1, 8.5);
        for (int n = 1; n <= 5; ++n)
            m << resident(tag + std::to_string(n),
                          std::string(c == 'E' ? "East" : c == 'M' ? "Mid" : c == 'C' ? "Coldwater" : "West") + " " + std::to_string(n), "civilian", "working",
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

// A caravan from the capital to the last town, with meals from the treasury.
const Caravan* send(World& w, int meals = 6)
{
    const auto* sent = w.sendCaravan(w.towns().front(), w.towns().back(), {{"meal", meals}});
    expect(sent, "A caravan sets out");
    const auto id = sent->id;
    w.tick(.6);                                     // (Its wagon comes out on the road at the next tick.)
    for (const auto& c : w.roads().caravans)
        if (c.id == id)
            return &c;
    return sent;
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
    expect(stock(w, "stores:west", "meal") == 0 && stock(w, "treasury", "meal") == treasuryMeals, "nothing sent of itself");
    expect(w.townOf(id(8)) && w.townOf(id(8))->id == "west" && !w.townOf(id(4)), "Wild country is no town");
    expect(w.roads().caravans.empty(), "No caravan of itself");
    send(w);
    expect(w.roads().caravans.size() == 1 && w.roads().caravans[0].to == "west" && w.roads().caravans[0].route.size() == 8,
           "A caravan sets out for west along the road");
    const auto load = stock(w, w.roads().caravans[0].account, "meal");
    expect(load > 0, "carrying real goods");
    expect(w.society().realBalanced(), "Real money is balanced");
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
    expect(w.society().realBalanced(), "Real money is still balanced");
    expect(stock(w, "treasury", "meal") + stock(w, "stores:west", "meal") == treasuryMeals, "The meals moved, none made");
}

void banditsAndContracts()
{
    auto f = strip();
    auto w = load(f);
    w.addPlayer("player-ada", "Ada");
    w.tick(.6);
    send(w);
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
    expect(w.society().realBalanced(), "The reward was set aside, not made");

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
    send(w);
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
    expect(w.society().realBalanced(), "Real money is balanced");
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
    send(w);
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

void couriers()
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
    expect(w.society().realBalanced(), "Real money is balanced");
}

// Doc 79, 9.1 and Phase 3: a caravan's account is the old economy's (a dead prefix), so until Phase 4's departures, which
// carry no account, a caravan out on the road doesn't outlive a restart (only tests send one now: sendCaravan has no
// caller in play). It is dropped cleanly: the letters it carried and the escorts taken for it open again with their
// coins still in escrow, nothing is made or lost, and the road goes on.
void aCaravanMidTripIsDroppedCleanlyOnRestore()
{
    auto f = strip();
    auto w = load(f);
    w.addPlayer("player-ada", "Ada");
    w.tick(.6);
    auto* ada = w.entity("player-ada");
    ada->cellId = id(8);
    ada->position = {8.5, 8.5};
    w.tick(.6);
    const std::string escort = w.postContract("escort", "treasury", "west", "west", 12, 7, "guarding the next caravan to west").id;
    expect(w.takeContract("player-ada", escort).ok, "Ada takes the escort");
    auto& letter = w.postContract("courier", "em", "west", "wm", 5, 30, "a letter from Ember to Wren");
    const std::string letterId = letter.id;
    letter.created -= 8;                         // (A week untaken: the carters take it.)
    ada->cellId = id(1);
    ada->position = {8.5, 10.5};
    const auto* sent = send(w);
    expect(sent && sent->letters == std::vector<std::string>{letterId} &&
               sent->escorts == std::vector<std::string>{"player-ada"},
           "The caravan carries the letter, Ada its escort");
    const std::string trip = sent ? sent->id : std::string();
    w.roads().camps.clear();                     // (No bandits this time.)
    run(w, 20);                                  // Out on the road (it reaches west in under a minute).
    expect(w.roads().caravans.size() == 1 && w.roads().caravans[0].status == "travelling", "Still on its way");
    const auto escrow = [&](const std::string& k) { return w.society().cash("contract:" + k); };
    const auto letterPay = escrow(letterId), escortPay = escrow(escort);
    const auto purse = w.society().cash("player-ada");
    auto saved = w.save();
    expect(w.restore(saved).ok, "The save restores");
    expect(w.roads().caravans.empty() && !w.entity("road:" + trip), "The caravan is dropped on load, its wagon with it");
    int reopened = 0;
    for (const auto& k : w.roads().contracts)
        if (k.id == letterId || k.id == escort)
            reopened += k.status == "open" && k.taker.empty();
    expect(reopened == 2, "Its letter and its escort are open again");
    expect(escrow(letterId) == letterPay && escrow(escort) == escortPay && w.society().cash("player-ada") == purse,
           "their coins still in escrow, Ada's purse as it was");
    expect(w.society().realBalanced(), "Real money is balanced after the restore");
    run(w, 150);
    expect(w.roads().caravans.empty() && w.society().realBalanced(), "and the road goes on without it");
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
    expect(w.society().realBalanced(), "Real money is balanced");
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
    expect(w.society().realBalanced(), "Real money is balanced");
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
    testPurse(w.society(), "player-ada", 80, "test: a fuller purse");
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
    expect(w.society().realBalanced(), "Real money is balanced");
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
    send(w);                                        // (Its escort taken by the town guard as it sets out.)
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
    expect(w.society().realBalanced(), "Real money is balanced");
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

// Shops (Docs/Design/39): each sells a handful of its kind's cheap goods and has them on its shelves from the start; food
// shops keep meals, a smith swords, an herbalist herbs. A player buys at the catalogue's price.
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
    // Bought, at the catalogue's price.
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
    testPurse(w.society(), "player-ada", 50, "test: a purse");
    const auto bought = w.trade("player-ada", "baker1", ware, 1, true);
    expect(bought.ok && stock(w, "player-ada", ware) == 1 && w.society().account("player-ada")->cash == 50 + Society::PlayerWelcome - Society::catalogPrice(ware),
           "A player buys one at the catalogue's price (" + bought.message + ")");
    expect(std::string(Society::itemName(ware)) == items::good(ware)->name, "by its catalog name");
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

} // namespace

// The Country's communities are no towns yet (communities.json `town` false; the user, 2026-11-03: doc 71, Phase C
// Progress): a world with Coldwater's places, its people at home there and its store kept, has one town, the city, when
// it starts and again after a restart; no stores, roads, camps or caravans of Coldwater's own, its people, posts and
// homes as they were. The old map's towns of the same names (without the communities' interiors) are towns as ever.
void communitiesStayPlain()
{
    named = {{7, "cn_coldwater_cottage_1"}, {8, "cn_coldwater_store"}};
    auto f = strip("EE.....CC");
    const auto plain = [&](World& w, const std::string& when) {
        expect(w.towns().empty() && w.society().capital() == "east", "One town, the city, " + when);
        expect(!w.townOf(id(8)) && w.lawTown(id(8)) == "coldwater" && w.communityOf(id(7)) == "coldwater" &&
                   w.lawTown(id(0)) == "east",
               "Coldwater a plain place of its own " + when + " (" + w.lawTown(id(8)) + ")");
        expect(!w.society().account("stores:coldwater") && w.society().treasuryOf("coldwater") == "treasury",
               "no stores of its own " + when);
        expect(w.roads().camps.empty() && w.roads().caravans.empty(), "no roads' camps or caravans " + when);
    };
    std::map<std::string, std::string> homes, works;
    const auto people = [](const World& w, std::map<std::string, std::string>& home, std::map<std::string, std::string>& work) {
        home.clear(), work.clear();
        for (const auto& [who, life] : w.society().state().residents)
            home[who] = life.homeCell;
        for (const auto& p : w.society().positions())
            work[p.id] = p.work.cell + " " + p.title;
    };
    auto w = load(f);
    w.tick(.6);
    plain(w, "at the start");
    people(w, homes, works);
    expect(homes.at("c1") == id(7) && homes.at("cm") == id(7), "its people at home there");
    for (int day = 0; day < 2; ++day)
        nextMorning(w);
    plain(w, "days on");
    World again = load(f);
    expect(again.restore(w.save()).ok, "A restart");
    again.tick(.6);
    plain(again, "after a restart");
    std::map<std::string, std::string> homesAfter, worksAfter;
    people(again, homesAfter, worksAfter);
    expect(homesAfter == homes && worksAfter == works, "the same people, homes and posts after it");
    bool store = false;
    for (const auto& [post, where] : worksAfter)
        store = store || where.rfind(id(8) + " ", 0) == 0;
    expect(store, "Coldwater's store kept");
    // The old map: a Coldwater without the communities' interiors is a town of its own, with its store.
    named.clear();
    auto old = strip("EE.....CC");
    auto o = load(old);
    o.tick(.6);
    expect(o.towns().size() == 2 && o.towns()[1].id == "coldwater" && o.towns()[1].store == "stores:coldwater",
           "the old map's town of the same name is a town as ever");
}

// NPC-posted courier contracts (the letters): a faucet, at most courierADay posted a day across the realm (doc 79, 9.3).
void lettersCapped()
{
    const auto letters = [](std::int64_t cap, std::map<std::int64_t, int>& byDay) {
        auto f = strip();
        auto w = load(f);
        if (cap >= 0)
        {
            auto o = json::Value::object(), caps = json::Value::object();
            caps.add("courierADay", double(cap));
            o.add("caps", caps);
            w.society().setStatusOverride(&o);
        }
        const std::vector<std::string> east{"em", "e1", "e2", "e3", "e4", "e5"}, west{"wm", "w1", "w2", "w3", "w4", "w5"};
        for (const auto& a : east)
            for (const auto& b : west)
                w.bonds().change(a, b, {100, 50, 50, 0, 0}, 0), w.bonds().change(b, a, {100, 50, 50, 0, 0}, 0);
        for (int day = 0; day < 60; ++day)
        {
            nextMorning(w);
            for (int t = 0; t < 8; ++t)
            {
                for (const auto& a : east)              // (Kept in coin, so only the cap holds them back.)
                    if (w.society().cash(a) < 20)
                        w.society().stagedSet(a, 40);
                w.tick(.6);
            }
        }
        int n = 0;
        for (const auto& k : w.roads().contracts)
            if (k.kind == "courier" && w.entity(k.poster) && w.entity(k.poster)->npc)
                ++byDay[std::int64_t(std::floor(k.created))], ++n;
        // The one gate every NPC courier posting asks (the letters here, the storyline parcels in the Game).
        expect(w.npcCouriersToday() <= cap, "today's NPC courier contracts within the cap");
        expect(w.npcCourierRoom() == (w.npcCouriersToday() < cap), "the gate says no once the day's cap is posted");
        return n;
    };
    std::map<std::int64_t, int> none, one, many;
    expect(letters(1000, many) > 0, "a cap well over the day's letters: the letters are posted as before");
    expect(letters(0, none) == 0, "staged, a cap of 0: no NPC-posted letters");
    expect(letters(1, one) > 0, "staged, a cap of 1: letters still posted");
    for (const auto& [day, n] : one)
        expect(n <= 1, "staged, a cap of 1: at most one letter posted on day " + std::to_string(day) + ": " + std::to_string(n));
}

int main()
{
    try
    {
        townsAndCaravans();
        banditsAndContracts();
        anEscortWhoIsntThere();
        couriers();
        aCaravanMidTripIsDroppedCleanlyOnRestore();
        rumoursSpread();
        banditsInPerson();
        banditsCreepUp();
        aWatcherCatchesTheCreep();
        aFight();
        beatenAndRobbed();
        banditsCalled();
        residentsTakeWork();
        shopsSellTheirGoods();
        travelKeepsToTheRoads();
        communitiesStayPlain();
        lettersCapped();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Road tests passed: " << checks << " checks.\n";
    return 0;
}
