// A Chapter's own ground (Core/RatwCamps.h; Docs/Design/32-parties-chapters-factions.md, 5.3–5.6): sites, plans,
// building by work, the levels' ground, wear and ruin; then through the game: a camp made in Juniper Yard, a tent
// planned and built by work, the stores reached from a storage pile, wear and mending, a resident stationed at the camp
// and paid, a Hall reached, and a restart.
#include "RatwCamps.h"
#include "RatwGame.h"
#include "RatwWire.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>

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

// A resident's hunger set (the society's state restored with it), and what it carries that feeds eaten up.
void hungry(World& w, const std::string& id, double hunger)
{
    auto& s = w.society();
    std::vector<std::pair<std::string, int>> food;
    for (const auto& [item, n] : s.account(id)->stock)
        if (n > 0 && Society::edible(item))
            food.push_back({item, n});
    for (const auto& [item, n] : food)
        s.consume(id, item, n, "test: eaten");
    auto state = s.state();
    state.residents[id].hunger = hunger;
    expect(s.restore(state), id + " is hungry");
}

json::Value parsed(const std::string& text)
{
    json::Value v;
    std::string error;
    expect(json::parse(text, v, error), "JSON: " + error);
    return v;
}

void groundAlone()
{
    using namespace camp;
    Camps c;
    const auto site = c.found("lodge", "yard", 20, 15, "Lodge Camp", 1, 1000).message;
    expect(!site.empty(), "a camp is begun");
    expect(!c.found("other", "yard", 25, 15, "Too Close", 1, 1000).ok, "another Chapter can't camp within twenty tiles");
    expect(c.found("other", "yard", 45, 15, "Far Enough", 1, 1000).ok, "far enough");
    expect(!c.plan(site, "tent", 40, 40).ok, "not beyond the ground");
    const auto tent = c.plan(site, "tent", 21, 15).message;
    expect(!tent.empty() && !c.plan(site, "firepit", 21, 15).ok, "a tent planned; nothing else on its tile");
    expect(!c.work(tent, 1) && c.work(tent, 1) && c.structure(tent)->built, "two work-hours: the tent stands");
    expect(!c.campStanding("lodge"), "one thing isn't a camp standing");
    for (auto [kind, x] : {std::pair{"firepit", 22}, std::pair{"storage", 23}})
    {
        const auto id = c.plan(site, kind, x, 15).message;
        c.work(id, 5);
    }
    expect(c.campStanding("lodge") && !c.fortified("lodge"), "three built: a camp standing");
    for (int i = 0; i < 8; ++i)
        c.work(c.plan(site, "palisade", 14 + i, 10).message, 5);
    c.work(c.plan(site, "gate", 22, 10).message, 5);
    c.work(c.plan(site, "hall", 20, 18).message, 20);
    expect(c.fortified("lodge"), "a palisade ring, a gate and a hall: fortified");
    // Wear: tents first. Left alone long enough, it all crumbles to a ruin.
    c.wear(5, 1000);
    expect(c.structure(tent)->condition < 100, "worn");
    const double tentBefore = c.structure(tent)->condition;
    c.work(tent, 0.5);
    expect(c.structure(tent)->condition > tentBefore, "mended");
    // What a building's materials cost when planned is saved with it; one saved before has none (-1), and so has one
    // whose sum is past any building's (doc 64, Phase 13).
    const auto paid = c.plan(site, "firepit", 24, 16).message, older = c.plan(site, "firepit", 25, 16).message,
               wild = c.plan(site, "firepit", 26, 16).message;
    c.structure(paid)->paid = 37;
    c.structure(wild)->paid = 37;
    auto saved = parsed(json::dump(c.save()));
    for (auto& j : saved.find("structures")->items())
        if (j.string("id") == wild)
            j.set("paid", 1e12);
    Camps d;
    d.load(saved);
    expect(d.sitesOf("lodge").size() == 1 && d.structuresOf(site).size() == c.structuresOf(site).size() && d.fortified("lodge"),
           "saved and read");
    expect(d.structure(paid)->paid == 37 && d.structure(older)->paid == -1 && d.structure(wild)->paid == -1,
           "what its materials cost, saved; none in an older save, nor past the bound");
    const auto ruined = c.wear(400, 1000 + AbandonedSeconds + 1);
    expect(!ruined.empty() && c.site(site)->state == "ruin" && !c.campStanding("lodge"), "abandoned: a ruin");
}

struct Client final : game::Connection
{
    std::vector<json::Value> events, snapshots;
    sections::Cache cache;
    void event(const std::string& text) override { events.push_back(parsed(text)); }
    void snapshot(const std::string& text) override
    {
        auto v = parsed(text);
        expect(sections::fill(v, cache), "a snapshot can be filled");
        snapshots.push_back(v);
    }
    void motion(const json::Value&) override {}
    bool allowsLocalCredentials() const override { return true; }
    std::string said() const
    {
        std::string all;
        for (const auto& e : events)
            all += e.string("text") + "\n";
        return all;
    }
};

std::string cmd(std::initializer_list<std::pair<const char*, json::Value>> fields)
{
    auto o = json::Value::object();
    for (const auto& [k, v] : fields)
        o.add(k, v);
    return json::dump(o);
}

void campingThroughTheGame(const std::string& save)
{
    std::remove(save.c_str());
    game::Options o;
    o.devIdentity = true;
    o.hiddenNames = false;
    o.forkSnapshots = false;
    o.savePath = save;
    std::string lodge, site;
    {
        game::Game g(o);
        std::string problem;
        expect(g.start(problem), "starts: " + problem);
        Client ada, bo, cy;
        ada.id = 1;
        bo.id = 2;
        cy.id = 3;
        for (auto* c : {&ada, &bo, &cy})
            g.connect(c);
        g.command(&ada, cmd({{"type", "hello"}, {"id", "ada"}, {"name", "Ada"}}));
        g.command(&bo, cmd({{"type", "hello"}, {"id", "bo"}, {"name", "Bo"}}));
        g.command(&cy, cmd({{"type", "hello"}, {"id", "cy"}, {"name", "Cy"}}));
        auto& w = g.world();
        const auto tick = [&](double seconds) {
            for (double t = 0; t < seconds; t += .05)
            {
                g.tick(.05);
                for (auto* c : {&ada, &bo, &cy})
                    if (!c->snapshots.empty())
                        g.acknowledge(c, c->snapshots.back().number("revision"), false);
            }
        };
        auto& ch = g.chapters();
        ch.propose(ada.entityId, {bo.entityId, cy.entityId}, "Ashen Lodge", "#5b8bd9", "", "scene-x", 0);
        ch.agree(bo.entityId, 0);
        lodge = ch.agree(cy.entityId, 0).message;
        w.society().openAccount("chapter:" + lodge);
        w.society().shift("treasury", "chapter:" + lodge, "", 0, 200, "test: dues");
        auto* a = w.entity(ada.entityId);
        a->cellId = "exterior";
        a->position = {30.5, 20.5};
        tick(6);                                      // (The towns are found when the places to let are.)
        g.command(&ada, cmd({{"type", "chapter"}, {"verb", "camp"}, {"name", "Juniper Camp"}}));
        expect(ada.said().find("from Company (level III)") != std::string::npos, "a Gathering can't make camp:\n" + ada.said());
        ch.byId(lodge)->level = 3;
        g.command(&ada, cmd({{"type", "chapter"}, {"verb", "camp"}, {"name", "Juniper Camp"}}));
        expect(ada.said().find("Your Chapter's camp is begun here") != std::string::npos, "a camp is begun:\n" + ada.said());
        site = g.camps().sitesOf(lodge).front()->id;
        // A tent, planned and paid for, then built by Ada's work.
        const auto before = w.society().account("chapter:" + lodge)->cash;
        a->position = {31.5, 20.5};
        g.command(&ada, cmd({{"type", "chapter"}, {"verb", "plan"}, {"kind", "tent"}}));
        expect(w.society().account("chapter:" + lodge)->cash == before - 10, "the tent's materials paid");
        tick(2.5);
        const auto& structures = ada.snapshots.back().array("structures");
        expect(structures.size() == 1 && structures[0].string("kind") == "tent" && !structures[0].boolean("built"), "the tent is planned:\n" +
                                                                                                                      ada.said());
        const auto tent = structures[0].string("id");
        expect(ada.snapshots.back()["self"]["camp"].string("name") == "Juniper Camp", "Ada sees her camp");
        a->position = {30.5, 20.5};
        g.command(&ada, cmd({{"type", "chapter"}, {"verb", "build"}, {"target", tent}}));
        tick(2 * camp::SecondsPerWorkHour + 2);
        expect(g.camps().structure(tent)->built && ada.said().find("A Tent stands at Juniper Camp") != std::string::npos,
               "two minutes' work: the tent stands:\n" + ada.said());
        // Built, the tent blocks the way and shelters the ground about it from the weather.
        tick(1);
        expect(w.obstacles["exterior"].count({31, 20}), "the tent stands in the way");
        a->position = {29.5, 20.5};
        w.moveTo(ada.entityId, 31.5, 20.5);
        bool through = false;
        for (int i = 0; i < 60; ++i)
        {
            tick(.05);
            through |= int(std::floor(a->position.x)) == 31 && int(std::floor(a->position.y)) == 20;
        }
        expect(!through, "and nobody walks into it");
        w.stop(ada.entityId);
        wire::environmentCommand(w, "exterior", "weather", "rain", true);
        const double open = w.environmentAt("exterior", {38.5, 25.5}).intensity, lee = w.environmentAt("exterior", {30.5, 20.5}).intensity;
        expect(open > 0 && lee < open * .3, "in its lee the rain is a quarter as strong (" + std::to_string(lee) + " of " + std::to_string(open) + ")");
        // A firepit and a storage pile: a camp standing, and the stores reached from it.
        for (auto [kind, x] : {std::pair{"firepit", 32.5}, std::pair{"storage", 33.5}})
        {
            a->position = {x, 20.5};
            g.command(&ada, cmd({{"type", "chapter"}, {"verb", "plan"}, {"kind", kind}}));
            tick(.2);
            const auto* st = g.camps().structureAt("exterior", int(x), 20);
            expect(st, std::string("planned: ") + kind);
            const auto id = st->id;
            g.command(&ada, cmd({{"type", "chapter"}, {"verb", "build"}, {"target", id}}));
            tick(camp::SecondsPerWorkHour + 1);
        }
        expect(g.camps().campStanding(lodge), "a camp standing");
        // Taken back unbuilt, a building's materials come back as they were paid for, whatever its kind costs since
        // (doc 64, Phase 13, the price review: by the hours the cost is at the town's day, which moves day to day).
        {
            Society::setWorkByHours(true);
            auto& society = w.society();
            const auto chapter = "chapter:" + lodge;
            a->position = {33.5, 23.5};
            const auto cash = society.account(chapter)->cash;
            g.command(&ada, cmd({{"type", "chapter"}, {"verb", "plan"}, {"kind", "firepit"}}));
            tick(.2);
            const auto* st = g.camps().structureAt("exterior", 33, 23);
            expect(st && st->paid > 0 && st->paid == cash - society.account(chapter)->cash, "a firepit planned, what it cost kept:\n" + ada.said());
            const auto id = st->id;
            const auto paid = st->paid;
            auto state = society.state();
            for (auto& [town, rate] : state.memory.going)
                rate *= 2;
            state.memory.going[""] = 2 * society.goingRate("");
            expect(society.restore(state) && g.buildingCost(*camp::kind("firepit"), "exterior") > paid, "a firepit costs more now");
            const auto planned = society.account(chapter)->cash;
            g.command(&ada, cmd({{"type", "chapter"}, {"verb", "unplan"}, {"target", id}}));
            tick(.2);
            expect(!g.camps().structure(id) && society.account(chapter)->cash == planned + paid,
                   "taken back, what was paid refunded: " + std::to_string(society.account(chapter)->cash - planned) + "p of " + std::to_string(paid) + "p");
            Society::setWorkByHours(false);
        }
        // Not beside a full step up or down: the ground must be level enough to build on.
        auto* ground = w.cell("exterior");
        const double was = ground->tile(38, 20)->height;
        ground->tile(38, 20)->height = ground->tile(37, 20)->height + 1.0;
        a->position = {37.5, 20.5};
        g.command(&ada, cmd({{"type", "chapter"}, {"verb", "plan"}, {"kind", "firepit"}}));
        tick(.2);
        expect(!g.camps().structureAt("exterior", 37, 20) && ada.said().find("too steep") != std::string::npos, "not on steep ground:\n" + ada.said());
        ground->tile(38, 20)->height = was;
        // A cookfire: two bundles of herbs become a meal.
        a->position = {35.5, 20.5};
        g.command(&ada, cmd({{"type", "chapter"}, {"verb", "plan"}, {"kind", "cookfire"}}));
        g.command(&ada, cmd({{"type", "chapter"}, {"verb", "build"}, {"target", g.camps().structureAt("exterior", 35, 20)->id}}));
        tick(2 * camp::SecondsPerWorkHour + 1);
        w.society().shift("treasury", ada.entityId, "herbs", 2, 0, "test: herbs");
        const int meals = Society::stock(*w.society().account(ada.entityId), "meal");
        const int herbs = Society::stock(*w.society().account(ada.entityId), "herbs");
        g.command(&ada, cmd({{"type", "chapter"}, {"verb", "cook"}}));
        tick(.3);
        expect(Society::stock(*w.society().account(ada.entityId), "meal") == meals + 1 &&
                   Society::stock(*w.society().account(ada.entityId), "herbs") == herbs - 2,
               "a meal cooked at the cookfire:\n" + ada.said());
        a->position = {33.5, 20.5};
        w.society().shift("treasury", ada.entityId, "meal", 1, 0, "test: a meal");
        g.command(&ada, cmd({{"type", "chapter"}, {"verb", "store"}, {"item", "meal"}, {"quantity", 1}}));
        expect(Society::stock(*w.society().account("chapter:" + lodge), "meal") == 1, "stored from the camp's storage pile");
        // A Hall, with the renown, members and Stories it takes.
        ch.byId(lodge)->renown = 4000;
        ch.byId(lodge)->storiesTold = 6;
        for (const char* who : {"di", "ed", "fa", "gu", "hy", "io", "ju", "ka", "lu"})
        {
            ch.invite(ada.entityId, who, 0);
            ch.accept(who, 0);
        }
        const double t = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
        for (const auto& [m, member] : ch.byId(lodge)->members)
            ch.touch(m, t);
        tick(6);
        expect(ch.byId(lodge)->level == 4, "a camp standing, and the rest: a Hall (level " + std::to_string(ch.byId(lodge)->level) + ")");
        // Wear, and mending.
        w.advanceCalendar(20);
        tick(1);
        const double worn = g.camps().structure(tent)->condition;
        expect(worn < 100, "the tent wears (" + std::to_string(worn) + ")");
        g.command(&ada, cmd({{"type", "chapter"}, {"verb", "build"}, {"target", tent}}));
        tick(30);
        expect(g.camps().structure(tent)->condition > worn, "and is mended");
        // A resident stationed at the camp: Bracken, hired, stays and is paid from the treasury.
        auto* b = w.entity("npc_scout");
        b->cellId = "exterior";
        b->position = {a->position.x + 1, a->position.y + 1};
        tick(.3);
        g.command(&ada, cmd({{"type", "action"}, {"action", "hire for 6p a day"}, {"target", "npc_scout"}}));
        g.command(&ada, cmd({{"type", "chapter"}, {"verb", "station"}, {"target", "npc_scout"}, {"role", "watch"}}));
        tick(1);
        expect(w.entity("npc_scout")->leaderId == "camp:" + site && g.camps().staff().count("npc_scout"),
               "Bracken keeps the camp:\n" + ada.said());
        const auto treasury = w.society().account("chapter:" + lodge)->cash;
        w.advanceCalendar(1);
        tick(1);
        expect(w.society().account("chapter:" + lodge)->cash == treasury - 6, "and is paid at dawn from the treasury");
        // A keep's work (doc 64, Phase 11): a workshop at the Hall, taken up as a smithy, Bracken its hand; it makes
        // swords from the bronze in the Chapter's stores, into them.
        w.society().shift("treasury", "chapter:" + lodge, "", 0, 200, "test: the Chapter's savings");
        a->position = {37.5, 22.5};
        g.command(&ada, cmd({{"type", "chapter"}, {"verb", "plan"}, {"kind", "workshop"}}));
        tick(.2);
        const auto* bench = g.camps().structureAt("exterior", 37, 22);
        expect(bench && bench->kind == "workshop", "a workshop planned:\n" + ada.said());
        const auto workshop = bench->id;
        g.command(&ada, cmd({{"type", "chapter"}, {"verb", "build"}, {"target", workshop}}));
        tick(10 * camp::SecondsPerWorkHour + 1);
        expect(g.camps().structure(workshop)->built, "and built");
        g.command(&ada, cmd({{"type", "chapter"}, {"verb", "trade"}, {"target", workshop}, {"trade", "nothing"}}));
        tick(.2);
        expect(g.camps().structure(workshop)->trade.empty(), "not a trade that doesn't make things");
        g.command(&ada, cmd({{"type", "chapter"}, {"verb", "trade"}, {"target", workshop}, {"trade", "smithy"}}));
        tick(.2);
        expect(g.camps().structure(workshop)->trade == "smithy", "the workshop takes up the smithy's work:\n" + ada.said());
        g.camps().staff()["npc_scout"].role = "smith's hand";
        const auto store = "chapter:" + lodge;
        for (const auto& [item, n] : {std::pair{"bronze_bar", 3}, std::pair{"charcoal", 3}})
            w.society().create(store, item, n, "test: laid in");
        const int swords = Society::stock(*w.society().account(store), "sword");
        w.advanceCalendar(1);
        tick(1);
        expect(Society::stock(*w.society().account(store), "sword") > swords && Society::stock(*w.society().account(store), "bronze_bar") < 3,
               "a day's work at the bench: swords from the stores' bronze (" +
                   std::to_string(Society::stock(*w.society().account(store), "sword") - swords) + ")");
        // (The first level, open tile along the row.)
        const auto fields = [&] {
            int n = 0;
            for (const auto* st : g.camps().structuresOf(site))
                n += st->kind == "field";
            return n;
        };
        for (const double x : {33.5, 35.5, 37.5, 39.5, 41.5})
            if (fields() == 0)
            {
                a->position = {x, 24.5};
                g.command(&ada, cmd({{"type", "chapter"}, {"verb", "plan"}, {"kind", "field"}}));
                tick(.2);
            }
        expect(fields() == 1, "and a field planned:\n" + ada.said());
        // Kept hands' meals (doc 64, Phase 13): hungry, Bracken eats from the Chapter's stores, which his site's storage
        // pile reaches; the meal goes from them by the ledger.
        expect(w.society().create(store, "meal", 2, "test: laid in"), "meals in the stores");
        const int laidIn = Society::stock(*w.society().account(store), "meal");
        hungry(w, "npc_scout", 61);
        tick(10);
        expect(Society::stock(*w.society().account(store), "meal") == laidIn - 1 && w.society().resident("npc_scout")->hunger < 61 - 50,
               "Bracken eats a meal from the Chapter's stores (hunger " + std::to_string(w.society().resident("npc_scout")->hunger) + ")");
        expect(g.camps().staff().count("npc_scout") && w.entity("npc_scout")->leaderId == "camp:" + site, "and keeps the camp");
        a->position = {37.5, 20.5};
        g.save();
    }
    {
        game::Game g(o);
        std::string problem;
        expect(g.start(problem), "restarts: " + problem);
        expect(g.camps().sitesOf(lodge).size() == 1 && g.camps().structuresOf(site).size() == 6 && g.camps().staff().count("npc_scout"),
               "the camp, its structures and its staff survive a restart");
        expect(g.camps().structureAt("exterior", 37, 22) && g.camps().structureAt("exterior", 37, 22)->trade == "smithy", "and the workshop's trade");
        // The stores bare: the Chapter is told its hands go hungry, once that day; unfed still at KeptLeaveHunger, Bracken
        // leaves the camp for his own life, where he eats as anyone does.
        Client ada;
        ada.id = 1;
        g.connect(&ada);
        g.command(&ada, cmd({{"type", "hello"}, {"id", "ada"}, {"name", "Ada"}}));
        auto& w = g.world();
        const auto store = "chapter:" + lodge;
        std::vector<std::pair<std::string, int>> food;
        for (const auto& [item, n] : w.society().account(store)->stock)
            if (n > 0 && Society::edible(item))
                food.push_back({item, n});
        for (const auto& [item, n] : food)
            w.society().consume(store, item, n, "test: spoiled");
        hungry(w, "npc_scout", Society::KeptToldHunger + 1);
        for (int i = 0; i < 40; ++i)
            g.tick(.05);
        expect(ada.said().find("The Chapter's stores have nothing to eat, and the hands at Juniper Camp are going hungry.") != std::string::npos,
               "the Chapter is told:\n" + ada.said());
        expect(g.camps().staff().count("npc_scout"), "he stays a while yet");
        hungry(w, "npc_scout", Society::KeptLeaveHunger + .5);
        for (int i = 0; i < 40; ++i)
            g.tick(.05);
        expect(!g.camps().staff().count("npc_scout") && w.entity("npc_scout")->leaderId.empty() &&
                   ada.said().find("leaves Juniper Camp: there was nothing to eat.") != std::string::npos,
               "unfed, he leaves:\n" + ada.said());
        int told = 0;
        for (std::size_t at = 0; (at = ada.said().find("are going hungry", at)) != std::string::npos; ++at)
            ++told;
        expect(told == 1, "told once that day: " + std::to_string(told));
    }
    std::remove(save.c_str());
}
} // namespace

int main()
{
    try
    {
        groundAlone();
        campingThroughTheGame("/tmp/ratw-camp-test-" + std::to_string(::getpid()) + ".json");
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
    std::cout << "camp tests: " << checks << " checks passed\n";
    return 0;
}
