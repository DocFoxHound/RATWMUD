// The roads (RatwRoads.h): towns and their stores, caravans that carry real goods, bandits who take them, contracts,
// and rumours. A strip of nine cells: the capital "east" at one end, "west" at the other, wild country between.
#include "RatwItems.h"
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

// A strip of cells in a row, one letter each: 'E' the capital "east", 'M' a town "mid", 'W' the town "west", '.'
// wild country. Each town's first cell is where its people live, its second its market.
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
    const auto region = [](char c) { return c == 'E' ? "east" : c == 'M' ? "mid" : c == 'W' ? "west" : "wilds"; };
    for (int i = 0; i < cells; ++i)
    {
        m << "exits \"" << id(i) << "\" " << ((i > 0) + (i + 1 < cells));
        if (i > 0) m << " \"" << id(i - 1) << '"';
        if (i + 1 < cells) m << " \"" << id(i + 1) << '"';
        m << '\n';
        m << "territory \"" << id(i) << "\" \"" << region(layout[std::size_t(i)]) << "\" \"-\" 0\n";
    }
    m << "spawn \"" << id(0) << "\" 8.5 8.5\n";
    m << "economy 1000 100 50 10 12\n";
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

int stock(const World& w, const std::string& account, const std::string& item)
{
    const auto* a = w.society().account(account);
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
    expect(w.rumoursAbout("wm", "player-ada", "Ada").find("drove the bandits off") != std::string::npos,
           "and the west market hears of it: " + w.rumoursAbout("wm", "player-ada", "Ada"));
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
    const int spare = stock(w, "treasury", "meal");
    expect(w.society().shift("treasury", "stores:mid", "meal", std::min(40, spare), 0, "test: a good harvest"),
           "Mid's good harvest (" + std::to_string(spare) + " to spare)");
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
    expect(w.society().account("town:east:works") && quantity >= 2 && reward >= quantity * 2, "a contract for goods, paid from its funds");
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
               w.society().churchOf("stores:west") == "town:west:church" && w.society().churchOf("treasury") == "town:east:church",
           "Each pays its own town and church");
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
    const auto* church = w.society().account("town:west:church");
    expect(church && church->cash > 0, "West's church was founded with a little from the town");
    // A month's doings: w1 does well, w2 inherits, w3 spends more than it earns.
    const auto& books = w.society().state().books;
    expect(books.month == 0 && books.start.count("w1") && books.start.count("wm"), "The month's books are open");
    expect(w.society().shift("treasury", "w1", "", 0, 30, "test: paid for work"),
           "w1 is paid for some work");
    expect(w.society().operatorTransfer("treasury", "w2", "", 0, 300).ok, "w2 is left 300p (not earned)");
    w.society().shift("w3", "wm", "", 0, w.society().account("w3")->cash, "test: spends it all");
    const auto profit = [&](const std::string& who) {
        return w.society().account(who)->cash - books.start.at(who) - (books.unearned.count(who) ? books.unearned.at(who) : 0);
    };
    const auto w1 = profit("w1"), w2 = profit("w2"), w3 = profit("w3");
    const auto w1Cash = w.society().account("w1")->cash, w2Cash = w.society().account("w2")->cash;
    const auto westBefore = w.society().account("stores:west")->cash, churchBefore = w.society().account("town:west:church")->cash;
    expect(w1 >= 30 && w2 < 300 && w3 < 0, "Profits: w1 " + std::to_string(w1) + "p, w2 " + std::to_string(w2) + "p, w3 " + std::to_string(w3) + "p");
    w.takeEvents();
    const auto told = w.reckonNow();
    std::int64_t tax = 0, tithes = 0, toCapital = 0;
    bool w3Paid = false, logged = false;
    for (const auto& e : w.takeEvents())
    {
        if (e.kind == "economy" && e.detail == "town tax" && e.target == "stores:west")
            tax += e.coins;
        if (e.kind == "economy" && e.detail == "tithe" && e.target == "town:west:church")
            tithes += e.coins;
        if (e.kind == "economy" && e.detail == "capital's share" && e.actor == "stores:west")
            toCapital += e.coins;
        w3Paid |= e.kind == "economy" && e.actor == "w3" && (e.detail == "town tax" || e.detail == "tithe");
        logged |= e.kind == "reckoning" && e.actor == "stores:west" && e.detail.find("West's reckoning") != std::string::npos;
    }
    expect(w.society().account("w1")->cash == w1Cash - w1 / 10 - w1 / 10, "w1 pays a tenth of its profit in tax and a tenth in tithe");
    expect(w.society().account("w2")->cash == w2Cash - std::max<std::int64_t>(0, w2 >= 10 ? w2 / 10 * 2 : 0),
           "w2 pays nothing on what it inherited");
    expect(!w3Paid, "w3, at a loss, pays nothing");
    expect(tax > 0 && tithes == tax && toCapital == tax / 10, "West took in " + std::to_string(tax) + "p in tax and " +
                                                             std::to_string(tithes) + "p in tithes, and sent the capital a tenth");
    expect(w.society().account("stores:west")->cash == westBefore + tax - toCapital &&
               w.society().account("town:west:church")->cash == churchBefore + tithes,
           "into its own purse and its church's");
    expect(logged && told.find("West's reckoning") != std::string::npos, "The reckoning is told: " + told);
    expect(books.start.at("w1") == w.society().account("w1")->cash && books.unearned.empty(), "New books are opened");
    expect(w.society().conserved(), "Money stays conserved");
    // The books are saved with the society.
    World again = load(f);
    expect(again.restore(w.save()).ok && again.society().state().books.start == books.start &&
               again.society().state().books.month == books.month,
           "The books survive a restart");
    // And the reckoning comes by itself on the 28th day.
    {
        auto saved = w.save();
        saved.calendarDays = 28.3;
        expect(w.restore(saved).ok, "Four weeks on");
    }
    w.takeEvents();
    run(w, 2);
    bool reckoned = false;
    for (const auto& e : w.takeEvents())
        reckoned |= e.kind == "reckoning";
    expect(reckoned && w.society().state().books.month == 1, "The month's reckoning comes on day 28");
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
                                    resident("lamp", "Lamp Wick", "civilian", "lights the lamps", 0, 13.5, 0, 13.5));
    auto w = load(f);
    w.tick(.6);
    const auto& soc = w.society();
    const auto payer = [&](const std::string& who, int age = 30) { return soc.payerOf(who, *soc.jobOf(who), age); };
    expect(payer("help").account == "em" && payer("help").whom == "the shop", "A shop's help is paid by its keeper");
    expect(payer("watch1").account == "treasury", "the watch by the town");
    expect(payer("chap").account == "town:east:church", "a chapel keeper by the church");
    expect(payer("farm").account.empty(), "a farmer lives by what it brings in");
    expect(payer("lamp").account == "treasury", "the lamplighter by the town");
    expect(payer("lamp", 12).account.empty(), "and a child draws no wages");
    expect(payer("w1").account == "stores:west", "West's folk by west");
    // A working day: the help's wages come out of Ember's purse.
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
                (e.actor == "em" ? fromShop : fromTown) += e.coins;
    }
    expect(fromShop > 0 && fromTown == 0, "The help is paid by the shop (" + std::to_string(fromShop) + "p)");
    // The shop goes broke: the help waits on its wages, and after a week labours for the Town Works instead.
    w.society().shift("em", "treasury", "", 0, w.society().account("em")->cash, "test: a bad week");
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
    expect(soc.account("tanner")->cash <= Society::ManagerWage * Society::FloatDays + Society::ManagerWage,
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
    // A good day: the takings above the float go to the house; the manager has its wage.
    w.society().shift("treasury", till, "", 0, 500, "test: a good day");
    w.takeEvents();
    nextMorning(w);
    run(w, 2);
    std::int64_t takings = 0, wage = 0;
    for (const auto& e : w.takeEvents())
    {
        if (e.kind == "economy" && e.detail == "house takings" && e.actor == till && e.target == house)
            takings += e.coins;
        if (e.kind == "economy" && e.detail == "manager's wage" && e.target == "tanner")
            wage += e.coins;
    }
    expect(takings > 0 && soc.account(till)->cash == soc.floatOf(pid), "The takings above the float go to the house (" +
                                                                         std::to_string(takings) + "p)");
    expect(wage == Society::ManagerWage, "The manager is paid its wage from the till");
    // Bad days: the till runs dry and the house props it up, until it sells the business to the other house.
    const auto other = house == "house:vesk_manor" ? std::string("house:house_ash") : std::string("house:vesk_manor");
    expect(w.society().shift("treasury", other, "", 0, soc.floatOf(pid) * 2 + 10, "test: the other house's wealth"), "The other house is rich");
    bool propped = false;
    for (int d = 0; d < Society::ProppedDays + 1 && soc.ownerOf(pid) == house; ++d)
    {
        w.society().shift(till, "treasury", "", 0, soc.account(till)->cash, "test: a bad day");
        w.takeEvents();
        nextMorning(w);
        run(w, 2);
        for (const auto& e : w.takeEvents())
            propped |= e.kind == "economy" && e.detail == "propped up by the house" && e.actor == house;
    }
    expect(propped, "A till run dry is propped up by its house");
    expect(soc.ownerOf(pid) == other, "and a business that keeps losing is sold to the other house");
    expect(soc.state().books.start.count(house) && soc.state().books.start.count(other), "Houses keep books for the reckoning");
    World again = load(f);
    expect(again.restore(w.save()).ok && again.society().ownerOf(pid) == other && again.society().tillOf("tanner") == till,
           "Who owns what survives a restart");
    expect(w.society().conserved(), "Money stays conserved");
}

// The rule against hoarding (doc 42): a church with more than it needs feeds the poor; a town hires hands for its
// works; neither sits on its money. Money stays conserved.
void noHoarding()
{
    auto f = strip("EE.....WW", resident("baker", "Bram Loaf", "merchant", "baker at The Amber Loaf", 7, 9.5, 8, 9.5));
    auto w = load(f);
    w.tick(.6);
    nextMorning(w);
    run(w, 2);
    // West's church is rich, and w1 is penniless with nothing to eat.
    const auto church = "town:west:church";
    expect(w.society().account(church) != nullptr, "West has a church");
    expect(w.society().shift("stores:west", church, "", 0, std::min<std::int64_t>(800, w.society().account("stores:west")->cash - 50), "test: a generous month"),
           "West's church is given a great deal");
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
    expect(spent && w.society().account(church)->cash < churchBefore, "The church spends what it holds above its need: " + told);
    expect(alms && !Society::bestFood(*w.society().account("w1")).empty(), "and penniless w1 is given food as alms");
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
    const auto storeBefore = w.society().account("store")->cash;
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
    const auto takerBefore = w.society().account(taker)->cash;
    w.roads().camps.clear();
    bool done = false;
    for (int t = 0; t < 6000 && !done; ++t)
    {
        w.tick(1);
        for (const auto& e : w.takeEvents())
            done |= e.kind == "contract done" && e.detail.find("stone") != std::string::npos;
    }
    expect(done, "fetches the stone from the shop in the west and delivers it");
    expect(Society::stockAll(*w.society().account("town:east:works"), "stone") > 0, "The stone reaches the Town Works");
    expect(w.society().account("store")->cash > storeBefore, "the shop is paid its price");
    expect(w.society().account(taker)->cash > takerBefore, "and the carrier the rest");
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
                                    unpaid(resident("beggar", "Poor Tom", "civilian", "begging in the market", 0, 8.5, 1, 4.5)));
    auto w = load(f);
    w.tick(.6);
    w.society().shift("treasury", "miner", "", 0, 60, "test: savings");
    w.society().shift("treasury", "em", "", 0, 200, "test: a good year");
    w.society().create(w.society().tillOf("smith"), "mining_pick", 3, "test: forged");
    w.society().create("farmer", "hay", 20, "test: the barn");
    w.society().shift("treasury", "ostler", "", 0, 100, "test: takings");
    std::map<std::string, int> seen;
    for (int d = 0; d < 16; ++d)
    {
        nextMorning(w);
        run(w, 2);
        for (const auto& e : w.takeEvents())
            if (e.kind == "economy")
                ++seen[e.detail + "|" + e.target + "|" + e.actor];
    }
    expect(seen.count("tools for the work|smith|miner"), "The miner buys a pick from the smith, out of its own purse");
    expect(seen.count("worn out at work|consumed|miner"), "and wears it out at work");
    bool fed = false, penny = false;
    for (const auto& [k, n] : seen)
    {
        fed |= k.rfind("eaten by the horses|consumed|ostler", 0) == 0;
        penny |= k.rfind("a penny for a beggar|beggar|", 0) == 0;
    }
    expect(fed, "The stables' horses eat hay, bought from the farmer");
    expect(penny, "The beggar is given pennies");
    // The church's bread goes to a hungry wolf with nothing.
    w.society().shift("ex1", "treasury", "", 0, w.society().account("ex1")->cash, "test: penniless");
    for (const auto& [item, n] : std::map<std::string, int>(w.society().account("ex1")->stock.begin(), w.society().account("ex1")->stock.end()))
        w.society().consume("ex1", item, n, "test: nothing to eat");
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
    for (const auto& kind : Society::kindsHeld(*w.society().account("wm"), "flour"))
        w.society().consume("wm", kind, Society::stock(*w.society().account("wm"), kind), "test: a bad week");
    w.society().shift("treasury", bakery, "", 0, 200, "test: takings");
    nextMorning(w);
    w.roads().camps.clear();
    const Caravan* trade = nullptr;
    for (const auto& c : w.roads().caravans)
        if (!c.trader.empty() && c.from == "east" && c.to == "west")
            trade = &c;
    expect(trade != nullptr, "A trader's caravan sets out from east for west");
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
            sold |= e.kind == "economy" && e.detail == "carted in by caravan" && e.target == trade->account && e.actor == bakery;
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

int main()
{
    try
    {
        shopsSellTheirGoods();
        townsAndCaravans();
        banditsAndContracts();
        anEscortWhoIsntThere();
        couriersAndSupplies();
        rumoursSpread();
        banditsInPerson();
        banditsCreepUp();
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
        residentsFillContractsForGoods();
        tradesWearAndCharity();
        tradeCaravansCarryGoodsAndMoney();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Road tests passed: " << checks << " checks.\n";
    return 0;
}
