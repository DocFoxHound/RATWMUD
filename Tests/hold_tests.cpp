// Halls and Holds (Docs/Design/32-parties-chapters-factions.md, Phase 9), through the game: a treaty proposed and agreed
// by a Dungeon Master, its tithe, building on the faction's land by its terms, a levy answered, the treaty broken by an
// unpaid tithe, fortifying with the faction's sworn friendship, the Hold's claim and the Hold level, recognition as a
// minor House by the faction's own rule, a resident sworn to the Chapter and settled at the Hold, and a restart.
#include "RatwGame.h"

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
    const json::Value* sees(const std::string& id) const
    {
        for (const auto& e : snapshots.back().array("entities"))
            if (e.string("id") == id)
                return &e;
        return nullptr;
    }
};

std::string cmd(std::initializer_list<std::pair<const char*, json::Value>> fields)
{
    auto o = json::Value::object();
    for (const auto& [k, v] : fields)
        o.add(k, v);
    return json::dump(o);
}

double unixNow()
{
    return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
}

void holdsThroughTheGame(const std::string& save)
{
    std::remove(save.c_str());
    game::Options o;
    o.devIdentity = true;
    o.hiddenNames = false;
    o.forkSnapshots = false;
    o.savePath = save;
    std::string lodge, steward, friendNpc;
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
        ch.byId(lodge)->level = 3;
        w.society().openAccount("chapter:" + lodge);
        w.society().shift("treasury", "chapter:" + lodge, "", 0, 600, "test: dues");
        // The Crown, its steward in the tavern, and Juniper Yard its land.
        g.factions().define({"crown", "The Crown", "#f0e6c8", "city"});
        auto* a = w.entity(ada.entityId);
        // (The friend must be an ordinary resident: the cook, keeper and forager stay at their work.)
        for (const auto& [id, e] : w.entities())
            if (e.npc && !e.transient && w.society().resident(id) && id != "npc_scout")
            {
                if (w.society().resident(id)->role == "resident" && friendNpc.empty())
                    friendNpc = id;
                else if (steward.empty() && e.cellId == a->cellId)
                    steward = id;
            }
        expect(!steward.empty() && !friendNpc.empty() && steward != friendNpc, "two residents");
        for (const auto& [id, e] : w.entities())
            if (e.npc)
                w.entity(id)->leaderId = "test-frozen";
        g.factions().setMember(steward, "crown", "steward", true);
        w.cell("exterior")->factionClaims = {"crown"};
        w.entity(steward)->position = {a->position.x, a->position.y + 1};
        tick(2.5);
        // A treaty: proposed to the steward, agreed by a DM.
        g.command(&ada, cmd({{"type", "action"}, {"action", "propose a treaty"}, {"target", steward}}));
        expect(g.factions().treaties().size() == 1 && g.factions().treaties()[0].state == "pending", "a treaty waits:\n" + ada.said());
        const auto treaty = g.factions().treaties()[0].id;
        // Without it, the Crown's land is closed to a camp.
        a->cellId = "exterior";
        a->position = {30.5, 20.5};
        tick(6);
        g.command(&ada, cmd({{"type", "chapter"}, {"verb", "camp"}, {"name", "Crown Camp"}}));
        expect(ada.said().find("one with a treaty") != std::string::npos, "the Crown's land needs its trust or a treaty:\n" + ada.said());
        expect(g.decideTreaty(treaty, true, "by a Dungeon Master").ok, "a DM agrees it");
        const auto before = w.society().account("chapter:" + lodge)->cash;
        tick(6);
        expect(w.society().account("chapter:" + lodge)->cash == before - 20, "the first week's tithe paid");
        g.command(&ada, cmd({{"type", "chapter"}, {"verb", "camp"}, {"name", "Crown Camp"}}));
        expect(g.camps().sitesOf(lodge).size() == 1, "by the treaty's terms, a camp on the Crown's land:\n" + ada.said());
        const auto site = g.camps().sitesOf(lodge).front()->id;
        // The treaty's levy: called this week, answered by Bo keeping watch where the steward is.
        tick(6);
        expect(!g.factions().levies().empty() && ada.said().find("calls on the Chapter") != std::string::npos, "a levy is called:\n" + ada.said());
        g.factions().levies().back().needed = 20;      // (Twenty seconds instead of twenty minutes.)
        auto* b = w.entity(bo.entityId);
        b->cellId = g.factions().levies().back().cell;
        const double standing = g.factions().earned("crown", lodge);
        tick(30);
        expect(g.factions().levies().back().state == "answered" && g.factions().earned("crown", lodge) == standing + 5, "answered: +5");
        // The tithe goes unpaid a week on: the treaty is broken.
        if (const auto left = w.society().account("chapter:" + lodge)->cash; left > 0)
            w.society().shift("chapter:" + lodge, "treasury", "", 0, left, "test: spent");
        w.advanceCalendar(7);
        tick(6);
        expect(g.factions().treaty(treaty)->state == "ended" && ada.said().find("the treaty with The Crown is broken") != std::string::npos,
               "broken:\n" + ada.said());
        // Fortifying the Crown's land takes its sworn friendship.
        ch.byId(lodge)->level = 4;
        w.society().shift("treasury", "chapter:" + lodge, "", 0, 300, "test: dues");
        a->position = {31.5, 22.5};
        g.command(&ada, cmd({{"type", "chapter"}, {"verb", "plan"}, {"kind", "palisade"}}));
        expect(ada.said().find("takes its sworn friendship, or a treaty") != std::string::npos, "not yet sworn:\n" + ada.said());
        g.factions().change("crown", lodge, 90, "test", w.calendarDays(), 0, false);
        g.command(&ada, cmd({{"type", "chapter"}, {"verb", "plan"}, {"kind", "palisade"}}));
        expect(g.camps().structureAt("exterior", 31, 22), "sworn friends may fortify:\n" + ada.said());
        // A Hold: the fortified camp, then stone (built by hand here), the Chapter's claim and level V.
        ch.byId(lodge)->level = 5;
        auto& camps = g.camps();
        for (int i = 0; i < 8; ++i)
            camps.work(camps.plan(site, "palisade", 22 + i, 15).message, 10);
        for (auto [kind, x, y] : {std::tuple{"gate", 30, 15}, std::tuple{"hall", 31, 20}, std::tuple{"keep", 32, 18}, std::tuple{"gatehouse", 33, 15}})
            camps.work(camps.plan(site, kind, x, y).message, 100);
        for (int i = 0; i < 8; ++i)
            camps.work(camps.plan(site, "wall", 22 + i, 25).message, 20);
        ch.byId(lodge)->level = 4;                    // (Earned again by the gates.)
        ch.byId(lodge)->renown = 10000;
        ch.byId(lodge)->storiesTold = 10;
        for (const char* who : {"di", "ed", "fa", "gu", "hy", "io", "ju", "ka", "lu", "mo", "nu", "op", "pa"})
        {
            ch.invite(ada.entityId, who, 0);
            ch.accept(who, 0);
        }
        for (const auto& [m, member] : ch.byId(lodge)->members)
            ch.touch(m, unixNow());
        tick(12);
        expect(ch.byId(lodge)->claimCell == "exterior" && ada.said().find("The Hold stands: the Chapter claims Juniper Yard") != std::string::npos,
               "the Hold's claim:\n" + ada.said());
        expect(ch.byId(lodge)->level == 5, "a Hold (level " + std::to_string(ch.byId(lodge)->level) + ")");
        // Recognised as a minor House: asked of the steward, decided by the Crown's own rule a day later.
        a->cellId = w.entity(steward)->cellId;
        a->position = {w.entity(steward)->position.x, w.entity(steward)->position.y - 1};
        tick(.5);
        g.command(&ada, cmd({{"type", "action"}, {"action", "ask to be recognised as a House"}, {"target", steward}}));
        w.advanceCalendar(1.1);
        tick(6);
        expect(ch.byId(lodge)->houseOf == "crown" && ada.said().find("recognises Ashen Lodge as a minor House (by its own rule)") != std::string::npos,
               "a minor House:\n" + ada.said());
        // A resident sworn for life, from a deep bond; and settled at the Hold.
        w.entity(friendNpc)->cellId = a->cellId;
        w.entity(friendNpc)->position = {a->position.x + 1, a->position.y};
        w.bonds().change(friendNpc, ada.entityId, {85, 75, 60, 0, 20}, w.calendarDays());
        tick(.5);
        g.command(&ada, cmd({{"type", "action"}, {"action", "ask to swear to the Chapter"}, {"target", friendNpc}}));
        expect(ch.byId(lodge)->sworn.count(friendNpc), "sworn:\n" + ada.said());
        tick(2.5);
        b->cellId = a->cellId;
        b->position = {a->position.x, a->position.y + 2};
        tick(.5);
        expect(bo.sees(friendNpc) && bo.sees(friendNpc)->string("rel") == "chapter", "a sworn resident wears the Chapter's colours");
        expect(g.dialogueContext(friendNpc, ada.entityId, "hello", true).activity.find("sworn for life to the Chapter Ashen Lodge, a minor House") !=
                   std::string::npos,
               "and knows it");
        w.entity(friendNpc)->leaderId.clear();
        g.command(&ada, cmd({{"type", "action"}, {"action", "settle at the Hold"}, {"target", friendNpc}}));
        expect(ada.said().find("They set out to make their home at the Hold") != std::string::npos ||
                   ada.said().find("They can't go:") != std::string::npos,
               "asked to settle at the Hold:\n" + ada.said());
        tick(2.5);
        const auto view = ada.snapshots.back()["self"]["chapter"]["hold"];
        expect(view.string("house") == "a minor House of The Crown" && view.string("claims") == "Juniper Yard" && view.array("sworn").size() == 1,
               "the Chapter window shows it: " + json::dump(view));
        g.save();
    }
    {
        game::Game g(o);
        std::string problem;
        expect(g.start(problem), "restarts: " + problem);
        const auto* c = g.chapters().byId(lodge);
        expect(c && c->houseOf == "crown" && c->claimCell == "exterior" && c->sworn.count(friendNpc) && c->level == 5,
               "the House, its claim and its sworn survive a restart");
        expect(!g.factions().treaties().empty(), "and its treaties");
    }
    std::remove(save.c_str());
}
} // namespace

int main()
{
    try
    {
        holdsThroughTheGame("/tmp/ratw-hold-test-" + std::to_string(::getpid()) + ".json");
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
    std::cout << "hold tests: " << checks << " checks passed\n";
    return 0;
}
