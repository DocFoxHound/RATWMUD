// The roads (RatwRoads.h): towns and their stores, caravans that carry real goods, bandits who take them, contracts,
// and rumours. A strip of nine cells: the capital "east" at one end, "west" at the other, wild country between.
#include "RatwItems.h"
#include "RatwWorld.h"
#include "battle_play.h"

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
    for (int t = 0; t < 4000 && !arrived; ++t)
    {
        w.tick(1);
        for (const auto& e : w.takeEvents())
            arrived |= e.kind == "caravan arrives" && e.actor == trip;
    }
    expect(arrived && stock(w, "stores:west", "meal") >= before + load - 2, "It arrives with west's food");
    expect(w.society().account("stores:mid")->cash > midCash, "and west pays mid for it");
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
        aFight();
        beatenAndRobbed();
        banditsCalled();
        residentsTakeWork();
        tradeAndPrices();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Road tests passed: " << checks << " checks.\n";
    return 0;
}
