// Factions in play (Core/RatwFactions.h; Docs/Design/32-parties-chapters-factions.md, Part 4): standing and its bands,
// weekly caps, ripples and drift, members' burdens, expulsion and the news of it; then through the game: an official's
// report, a crime that weighs, telling of an expulsion, a tithe, a mission done, a merchant who won't serve, a faction
// at war shown red, and what the Mind is told.
#include "RatwFactions.h"
#include "RatwGame.h"

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

// ------------------------------------------------------------------ The rules alone

void standingAndBurden()
{
    using namespace faction;
    expect(std::string(band(80)) == "Sworn" && std::string(band(20)) == "Known well" && std::string(band(0)) == "Neutral" &&
               std::string(band(-20)) == "Distrusted" && std::string(band(-50)) == "Hostile" && std::string(band(-90)) == "Enemy",
           "bands");
    Factions f;
    f.define({"crown", "The Crown", "#ffffff", "city"});
    f.define({"church", "The Church", "#eeeeee", "npc"});
    f.define({"syndicate", "The Syndicate", "#333333", "npc"});
    f.setRelation("church", "crown", {60, "allied"});
    f.setRelation("syndicate", "crown", {-60, "hostile"});
    expect(f.change("crown", "lodge", 8, "mission", 1) == 8, "a mission");
    expect(std::abs(f.earned("church", "lodge") - 2) < 1e-9 && std::abs(f.earned("syndicate", "lodge") + 2) < 1e-9,
           "word spreads: the Crown's ally a quarter with, its enemy a quarter against");
    expect(f.change("crown", "lodge", 1, "tithe", 1, 4) == 1 && f.change("crown", "lodge", 5, "tithe", 2, 4) == 3 &&
               f.change("crown", "lodge", 1, "tithe", 3, 4) == 0,
           "a tithe: at most 4 a week");
    expect(f.change("crown", "lodge", 1, "tithe", 7.5, 4) == 1, "a new week");
    // Burdens: a member's crime weighs on the Chapter, as the faction knows it.
    f.addBurden("crown", "ada", 6, "inc-1", 2);
    f.addBurden("crown", "ada", 6, "inc-1", 2);
    expect(f.burden("crown", "ada") == 6, "one incident weighs once");
    const double before = f.effective("crown", "lodge", {"ada", "bo"});
    expect(std::abs(before - (f.earned("crown", "lodge") - 6)) < 1e-9, "the burden of a member counts against the Chapter");
    // Sent away: it still counts until the faction hears.
    f.expelled("lodge", "ada", 3);
    expect(std::abs(f.effective("crown", "lodge", {"bo"}) - before) < 1e-9, "sent away, but not yet heard of: still counted");
    f.hear("crown", "lodge", 3.1);
    expect(std::abs(f.effective("crown", "lodge", {"bo"}) - f.earned("crown", "lodge")) < 1e-9, "told: it leaves with her");
    expect(f.stillCounted("church", "lodge").size() == 1, "others haven't heard yet");
    f.spread(3 + Factions::ExpulsionNewsDays + .1);
    expect(f.stillCounted("church", "lodge").empty(), "word reaches everyone in a day and a half");
    // Scrutiny: a Trusted Chapter's members weigh double.
    f.change("church", "lodge", 45, "mission", 4, 0, false);
    f.addBurden("church", "bo", 3, "inc-2", 4);
    expect(std::abs(f.effective("church", "lodge", {"bo"}) - (f.earned("church", "lodge") - 6)) < 1e-9, "trusted: double weight");
    // Drift and fading, each game week.
    const double crown = f.earned("crown", "lodge");
    f.drift(4);
    f.drift(4 + 7 * 3);
    expect(std::abs(f.earned("crown", "lodge") - (crown - 3)) < 1e-9 && f.burden("crown", "ada") == 3, "a week each: drift and fading");
    Factions g;
    g.define({"crown", "The Crown", "#ffffff", "city"});
    g.load(parsed(json::dump(f.save())));
    expect(std::abs(g.earned("crown", "lodge") - f.earned("crown", "lodge")) < 1e-9 && g.burden("crown", "ada") == 3, "saved and read");
}

// ------------------------------------------------------------------ Through the game

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
    const json::Value* last(const std::string& type) const
    {
        for (auto it = events.rbegin(); it != events.rend(); ++it)
            if (it->string("type") == type)
                return &*it;
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

void factionsThroughTheGame()
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
    // A faction, the Watch, whose sergeant is the resident Moss stands for here; the Ashen Lodge of Ada, Bo and Cy.
    g.factions().define({"watch", "The Greyfen Watch", "#9aa9c0", "city"});
    std::string sergeant, victim;
    auto* a = w.entity(ada.entityId);
    for (const auto& [id, e] : w.entities())
        if (e.npc && !e.transient && w.society().resident(id) && e.cellId == a->cellId && id != "npc_scout")
            (sergeant.empty() ? sergeant : victim) = victim.empty() ? id : victim;
    expect(!sergeant.empty() && !victim.empty() && sergeant != victim, "two residents");
    g.factions().setMember(sergeant, "watch", "sergeant", true);
    g.factions().setMember(victim, "watch", "constable", true);
    for (const auto& [id, e] : w.entities())
        if (e.npc)
            w.entity(id)->leaderId = "test-frozen";
    for (auto* c : {&bo, &cy})
    {
        auto* e = w.entity(c->entityId);
        e->cellId = a->cellId;
        e->position = {a->position.x + 1, a->position.y};
    }
    w.entity(sergeant)->position = {a->position.x, a->position.y + 1};
    w.entity(victim)->position = {a->position.x + 1, a->position.y + 1};
    auto& ch = g.chapters();
    ch.propose(ada.entityId, {bo.entityId, cy.entityId}, "Ashen Lodge", "#5b8bd9", "", "scene-x", 0);
    ch.agree(bo.entityId, 0);
    const auto lodge = ch.agree(cy.entityId, 0).message;
    expect(!lodge.empty(), "the Lodge is founded");
    tick(2.5);
    // An official: standing, a report (paid, at Neutral), a tithe.
    expect([&] {
        for (const auto& act : ada.sees(sergeant)->array("actions"))
            if (act.asString() == "ask about our standing")
                return true;
        return false;
    }(),
           "the sergeant can be asked about the Lodge's standing");
    g.command(&ada, cmd({{"type", "action"}, {"action", "ask about our standing"}, {"target", sergeant}}));
    expect(ada.said().find("Information isn't free") != std::string::npos, "at Neutral, a report costs:\n" + ada.said());
    g.command(&ada, cmd({{"type", "action"}, {"action", "pay for a report (5p)"}, {"target", sergeant}}));
    expect(ada.said().find("The Greyfen Watch holds Ashen Lodge to be neutral. Nothing of yours weighs with us.") != std::string::npos,
           "a report:\n" + ada.said());
    g.command(&bo, cmd({{"type", "action"}, {"action", "give a tithe (20p)"}, {"target", sergeant}}));
    expect(g.factions().earned("watch", lodge) == 1, "a tithe: +1");
    // Ada robs a constable in plain sight: it weighs on the Lodge, and the report says what was seen.
    a->dexterity = 1;
    for (int i = 0; i < 8 && g.factions().burden("watch", ada.entityId) <= 0; ++i)
    {
        g.command(&ada, cmd({{"type", "action"}, {"action", "steal"}, {"target", victim}}));
        tick(5);
    }
    expect(g.factions().burden("watch", ada.entityId) > 0, "her crime weighs with the Watch");
    tick(2.5);
    expect(json::dump(cy.snapshots.back()["self"]["chapter"]["standings"]).find("weighs on your name") != std::string::npos ||
               g.factions().burden("watch", ada.entityId) < 14,
           "the Chapter is told something weighs, never who");
    bo.events.clear();
    w.society().shift("treasury", bo.entityId, "", 0, 30, "test: purse");   // (His tithe emptied it.)
    g.command(&bo, cmd({{"type", "action"}, {"action", "pay for a report (5p)"}, {"target", sergeant}}));
    expect(bo.said().find("theft") != std::string::npos && bo.said().find("the one seen was") != std::string::npos,
           "the report tells what was seen:\n" + bo.said() + json::dump(bo.events.empty() ? json::Value() : bo.events.back()) +
               " cash " + std::to_string(w.society().account(bo.entityId)->cash));
    // Ada is sent away: until the Watch hears, it still weighs.
    const double with = g.factions().effective("watch", lodge, {ada.entityId, bo.entityId, cy.entityId});
    ch.setRank(ada.entityId, bo.entityId, chapter::RankHead, 0);
    g.command(&bo, cmd({{"type", "chapter"}, {"verb", "remove"}, {"target", ada.entityId}}));
    expect(!ch.of(ada.entityId), "Ada is sent away");
    tick(2.5);
    expect(std::abs(g.factions().effective("watch", lodge, {bo.entityId, cy.entityId}) - with) < 1e-9, "still weighing, unheard of");
    g.command(&bo, cmd({{"type", "action"}, {"action", "tell of an expulsion"}, {"target", sergeant}}));
    expect(g.factions().effective("watch", lodge, {bo.entityId, cy.entityId}) > with, "told: her burden leaves with her");
    // A mission: a Lodge the Watch knows well takes it, and does it.
    ch.byId(lodge)->level = 2;
    g.factions().change("watch", lodge, 20, "test", w.calendarDays(), 0, false);
    faction::Mission m;
    m.id = g.factions().nextMissionId();
    m.faction = "watch";
    m.kind = "deliver";
    m.item = "meal";
    m.quantity = 1;
    m.official = sergeant;
    m.coins = 8;
    m.standing = 4;
    m.renown = 5;
    m.expires = w.calendarDays() + 1;
    g.factions().missions().push_back(m);
    g.command(&bo, cmd({{"type", "action"}, {"action", "ask for missions"}, {"target", sergeant}}));
    expect(bo.last("missions") && !bo.last("missions")->array("missions").empty(), "the board");
    g.command(&bo, cmd({{"type", "faction"}, {"verb", "take"}, {"mission", m.id}}));
    w.society().shift("treasury", bo.entityId, "meal", 1, 0, "test: a meal");
    const double earned = g.factions().earned("watch", lodge);
    const int renown = ch.byId(lodge)->renown;
    tick(.3);
    g.command(&bo, cmd({{"type", "action"}, {"action", "deliver " + m.id}, {"target", sergeant}}));
    expect(g.factions().earned("watch", lodge) == earned + 4 && ch.byId(lodge)->renown == renown + 5, "done: standing and renown");
    expect(bo.said().find("The mission is done") != std::string::npos, "and Bo is told");
    // A merchant of a hostile faction won't serve; a faction at war shows red.
    std::string merchant;
    for (const auto& [id, e] : w.entities())
        if (e.npc && w.society().merchant(id))
            merchant = id;
    if (!merchant.empty())
    {
        g.factions().setMember(merchant, "watch", "", true);
        g.factions().change("watch", lodge, -100, "test", w.calendarDays(), 0, false);
        g.command(&bo, cmd({{"type", "trade"}, {"target", merchant}, {"item", "meal"}, {"quantity", 1}, {"buy", true}}));
        expect(bo.said().find("I don't serve the Ashen Lodge's sort") != std::string::npos, "turned away:\n" + bo.said());
    }
    g.factions().setStance("watch", lodge, "war");
    tick(.5);
    expect(bo.sees(sergeant) && bo.sees(sergeant)->string("rel") == "hostile" &&
               bo.sees(sergeant)->string("why").find("at war with your Chapter") != std::string::npos,
           "at war: the Watch shows red");
    expect(g.dialogueContext(sergeant, bo.entityId, "hello", true).activity.find("regards Ashen Lodge") != std::string::npos,
           "the sergeant's Mind knows how the Watch regards the Lodge");
}
} // namespace

int main()
{
    try
    {
        standingAndBurden();
        factionsThroughTheGame();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
    std::cout << "faction tests: " << checks << " checks passed\n";
    return 0;
}
