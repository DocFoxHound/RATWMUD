// A Chapter's ground (Core/RatwEstates.h; Docs/Design/32-parties-chapters-factions.md, Part 5): leases, rent, grace
// and eviction, the rented hall the Company level needs; then through the game: what the world lets (rooms above
// inns), leasing from the treasury, a locked door, a guest let in, the notice board, the stores, rent at the week's
// end, eviction, and the Company reached.
#include "RatwEstates.h"
#include "RatwGame.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <cmath>
#include <iostream>
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

json::Value parsed(const std::string& text)
{
    json::Value v;
    std::string error;
    expect(json::parse(text, v, error), "JSON: " + error);
    return v;
}

void leasesAlone()
{
    using namespace estate;
    Estates e;
    e.define({"loft", "The Loft", "hall", "npc_keeper", "", 30, 2});
    e.markAuthored("loft");
    expect(!e.open("cellar", "lodge", 0).ok, "only a place to let");
    expect(e.open("loft", "lodge", 1).ok && e.lease("loft")->paidTo == 1 + WeekDays, "a lease, a week paid");
    expect(!e.open("loft", "other", 1).ok, "one Chapter at a time");
    expect(e.due(5).empty() && e.due(8).size() == 1, "rent falls due a week on");
    e.paid("loft", 8);
    expect(e.lease("loft")->paidTo == 15 && e.lease("loft")->state == "active", "paid: another week");
    expect(!e.heldLongEnough("lodge", 14.9) && e.heldLongEnough("lodge", 15), "held a fortnight");
    expect(!e.unpaid("loft", 15) && e.lease("loft")->state == "grace", "unpaid: a week's grace");
    expect(!e.heldLongEnough("lodge", 16), "not while in grace");
    expect(e.unpaid("loft", 15 + GraceDays) && !e.lease("loft"), "then evicted");
    e.open("loft", "lodge", 30);
    e.lease("loft")->guests.insert("cy");
    e.lease("loft")->notices.push_back({"ada", "Meet at dusk.", 30});
    Estates f;
    f.load(parsed(json::dump(e.save())));
    expect(f.property("loft") && f.lease("loft") && f.lease("loft")->guests.count("cy") && f.lease("loft")->notices.size() == 1,
           "saved and read, with what the DM made");
}

// Atlas's records (doc 32): `let` places to let and `joinable` residents, read from a world file, and refused when wrong.
void authoredRecords()
{
    namespace fs = std::filesystem;
    const fs::path source = RATW_SOURCE_DIR "/Data/Worlds/Greyfen";
    const auto load = [&](const std::string& extra, World& w) {
        const fs::path dir = fs::temp_directory_path() / ("ratw-let-" + std::to_string(::getpid()));
        fs::remove_all(dir);
        fs::copy(source, dir, fs::copy_options::recursive);
        std::ofstream(dir / "world.ratw", std::ios::app) << extra;
        const auto loaded = w.loadWorldFile((dir / "world.ratw").string());
        fs::remove_all(dir);
        return loaded;
    };
    World w;
    const auto ok = load("let \"shop\" \"hall\" \"wren\" 30 2\njoinable \"harrow\"\n", w);
    expect(ok.ok, "a world with a place to let and a joinable resident loads: " + ok.message);
    expect(w.lettings().count("shop") && w.lettings().at("shop").landlord == "wren" && w.lettings().at("shop").rent == 30,
           "the place to let is read");
    expect(w.society().spec("harrow") && w.society().spec("harrow")->joinable && !w.society().spec("wren")->joinable,
           "Harrow may travel with a party; Wren may not");
    World bad;
    expect(!load("let \"shop\" \"hall\" \"nobody\" 30 2\n", bad).ok, "an unknown landlord is refused");
    World bad2;
    expect(!load("let \"shop\" \"castle\" \"wren\" 30 2\n", bad2).ok, "an unknown kind is refused");
    World bad3;
    expect(!load("joinable \"nobody\"\n", bad3).ok, "an unknown resident is refused");
    World bad4;
    expect(!load("joinable \"harrow\"\njoinable \"harrow\"\n", bad4).ok, "once only");
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

void rentingThroughTheGame()
{
    game::Options o;
    o.devIdentity = true;
    o.hiddenNames = false;
    o.forkSnapshots = false;
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
    const auto lodge = ch.agree(cy.entityId, 0).message;
    w.society().openAccount("chapter:" + lodge);
    // Cy leaves the Lodge, to be the one outside.
    ch.leave(cy.entityId, 1);
    // The loft above the demo tavern is to let, from its keeper.
    g.estates().define({"loft", "The Loft", "hall", "npc_keeper", "", 30, 2});
    g.estates().markAuthored("loft");
    auto* a = w.entity(ada.entityId);
    a->cellId = "loft";
    a->position = {4.5, 8.5};
    tick(.5);
    expect(ada.snapshots.back()["self"]["place"].string("name") == "The Loft" && ada.snapshots.back()["self"]["place"].number("rent") == 30,
           "Ada sees the loft is to let");
    g.command(&ada, cmd({{"type", "chapter"}, {"verb", "lease"}}));
    expect(ada.said().find("takes a Lodge") != std::string::npos, "a Gathering can't rent yet:\n" + ada.said());
    ch.byId(lodge)->level = 2;
    g.command(&ada, cmd({{"type", "chapter"}, {"verb", "lease"}}));
    expect(ada.said().find("it hasn't them") != std::string::npos, "an empty treasury can't pay");
    w.society().shift("treasury", "chapter:" + lodge, "", 0, 60, "test: dues");
    const auto keeper = w.society().account("npc_keeper")->cash;
    g.command(&ada, cmd({{"type", "chapter"}, {"verb", "lease"}}));
    expect(ada.said().find("You take the lease on The Loft") != std::string::npos, "leased:\n" + ada.said());
    expect(w.society().account("npc_keeper")->cash == keeper + 30 && w.society().account("chapter:" + lodge)->cash == 30,
           "the first week paid, treasury to keeper");
    // The door is locked to Cy, who isn't of the Lodge, until she is let in.
    auto* c = w.entity(cy.entityId);
    c->cellId = "tavern";
    c->position = {28.5, 5.2};
    tick(.3);
    g.command(&cy, cmd({{"type", "action"}, {"action", "enter"}, {"target", "stairs_up"}}));
    tick(.5);
    expect(w.entity(cy.entityId)->cellId == "tavern" && cy.said().find("The door is locked") != std::string::npos,
           "locked to Cy:\n" + cy.said());
    auto* b = w.entity(bo.entityId);
    b->cellId = "tavern";
    b->position = {28.5, 5.2};
    tick(.3);
    g.command(&bo, cmd({{"type", "action"}, {"action", "enter"}, {"target", "stairs_up"}}));
    tick(.5);
    expect(w.entity(bo.entityId)->cellId == "loft", "Bo, of the Lodge, goes up");
    c->cellId = "loft";                               // (Let in by hand to be offered as a guest from inside.)
    c->position = {5.5, 8.5};
    tick(.3);
    g.command(&ada, cmd({{"type", "action"}, {"action", "let in"}, {"target", cy.entityId}}));
    c->cellId = "tavern";
    c->position = {28.5, 5.2};
    tick(.3);
    g.command(&cy, cmd({{"type", "action"}, {"action", "enter"}, {"target", "stairs_up"}}));
    tick(.5);
    expect(w.entity(cy.entityId)->cellId == "loft", "let in as a guest, Cy goes up");
    // The notice board and the stores.
    g.command(&bo, cmd({{"type", "chapter"}, {"verb", "notice"}, {"text", "Meet here at dusk."}}));
    w.society().shift("treasury", bo.entityId, "meal", 1, 0, "test: a meal");
    g.command(&bo, cmd({{"type", "chapter"}, {"verb", "store"}, {"item", "meal"}, {"quantity", 1}}));
    tick(2.5);
    const auto place = ada.snapshots.back()["self"]["place"];
    expect(place.boolean("mine") && place.array("notices").size() == 1 && place["stores"].number("meal") == 1,
           "the board and the stores: " + json::dump(place));
    // A week on, rent is paid from the treasury; another, and it can't be; past the grace, evicted.
    w.advanceCalendar(7);
    tick(6);
    expect(w.society().account("chapter:" + lodge)->cash == 0 && ada.said().find("a week's rent on The Loft") != std::string::npos,
           "the week's rent paid:\n" + ada.said());
    expect(ch.byId(lodge)->hallHeldTwoWeeks == false, "a week isn't a fortnight");
    w.advanceCalendar(7);
    tick(6);
    expect(ada.said().find("can't pay it. A week's grace") != std::string::npos, "unpaid: warned");
    w.advanceCalendar(7);
    tick(6);
    expect(!g.estates().lease("loft") && ada.said().find("Evicted from The Loft") != std::string::npos, "evicted:\n" + ada.said());
    // Held a fortnight, paid up, with the renown and the members: a Company.
    w.society().shift("treasury", "chapter:" + lodge, "", 0, 200, "test: dues");
    a->cellId = "loft";
    tick(.3);
    g.command(&ada, cmd({{"type", "chapter"}, {"verb", "lease"}}));
    ch.byId(lodge)->renown = 1500;
    ch.byId(lodge)->storiesTold = 3;
    for (const char* who : {"di", "ed", "fa", "gu", "hy", "io"})
    {
        ch.invite(ada.entityId, who, 0);
        ch.accept(who, 0);
    }
    w.advanceCalendar(14);
    tick(6);
    // (Activity is measured in real seconds: each has just taken part.)
    for (const auto& [m, member] : ch.byId(lodge)->members)
        ch.touch(m, std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count());
    tick(6);
    expect(ch.byId(lodge)->hallHeldTwoWeeks && ch.byId(lodge)->level == 3, "a fortnight in the loft: a Company (level " +
                                                                               std::to_string(ch.byId(lodge)->level) + ")");
}
} // namespace

int main()
{
    try
    {
        leasesAlone();
        authoredRecords();
        rentingThroughTheGame();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
    std::cout << "estate tests: " << checks << " checks passed\n";
    return 0;
}
