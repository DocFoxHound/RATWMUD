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
    Camps d;
    d.load(parsed(json::dump(c.save())));
    expect(d.sitesOf("lodge").size() == 1 && d.structuresOf(site).size() == c.structuresOf(site).size() && d.fortified("lodge"),
           "saved and read");
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
        g.save();
    }
    {
        game::Game g(o);
        std::string problem;
        expect(g.start(problem), "restarts: " + problem);
        expect(g.camps().sitesOf(lodge).size() == 1 && g.camps().structuresOf(site).size() == 4 && g.camps().staff().count("npc_scout"),
               "the camp, its structures and its staff survive a restart");
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
