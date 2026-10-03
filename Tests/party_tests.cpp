// Parties (Core/RatwParty.h; Docs/Design/32-parties-chapters-factions.md, Part 2): the rules alone, then through the
// game as clients drive it: invitations, the party in each snapshot, who shows as a party mate or hostile, the party's
// two chats, being called into a party mate's fight (Docs/Design/33-combat.md), and a restart.
#include "RatwGame.h"
#include "RatwParty.h"

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

// ------------------------------------------------------------------ The rules alone

void formingAndLeaving()
{
    party::Parties p;
    expect(!p.invite("ada", "ada", 0).ok, "nobody invites themself");
    expect(p.invite("ada", "bo", 0).ok, "anyone in no party may invite");
    expect(p.inviteFor("bo", 10) && p.inviteFor("bo", 10)->from == "ada", "the invitation waits for Bo");
    expect(!p.of("ada"), "and no party exists until it is accepted");
    expect(!p.invite("cy", "bo", 5).ok, "a second invitation can't crowd out the first");
    expect(p.accept("bo", 20).ok, "Bo accepts");
    const auto* party = p.of("ada");
    expect(party && party->leader == "ada" && party->members.size() == 2 && p.together("ada", "bo"), "Ada leads a party of two");
    expect(p.mates("bo") == std::vector<std::string>{"ada"}, "Bo's party mate is Ada");
    expect(!p.invite("bo", "cy", 21).ok, "only the leader invites");
    expect(p.invite("ada", "cy", 21).ok && !p.accept("cy", 21 + party::InviteSeconds + 1).ok, "an invitation lapses after a minute");
    expect(p.invite("ada", "cy", 100).ok && p.decline("cy").ok && !p.accept("cy", 101).ok, "a declined one is gone");
    for (const char* who : {"cy", "di", "ed", "fa"})
    {
        expect(p.invite("ada", who, 200).ok, std::string("Ada invites ") + who);
        expect(p.accept(who, 201).ok, std::string(who) + " joins");
    }
    expect(p.of("ada")->members.size() == party::MaxPlayers, "six players: full");
    expect(!p.invite("ada", "gus", 202).ok, "a full party invites nobody");
    expect(!p.invite("gus", "bo", 202).ok, "someone in a party can't be invited to another");
    expect(p.lead("ada", "bo").ok && p.of("ada")->leader == "bo", "the lead can be handed on");
    expect(!p.remove("ada", "cy").ok, "only the leader sends anyone away");
    expect(p.remove("bo", "cy").ok && !p.of("cy"), "the leader sends Cy away");
    expect(p.leave("bo").ok && p.of("ada")->leader == "ada", "the leader leaving hands the lead to the next to have joined");
    expect(p.leave("di").ok && p.leave("ed").ok && p.of("ada"), "a party of two goes on");
    expect(p.leave("fa").ok && !p.of("ada"), "a party of one is no party");
    expect(p.invite("ada", "bo", 300).ok && p.accept("bo", 300).ok && p.disband("ada").ok && !p.of("bo"), "a leader disbands it");
}

void placesKeptAndLost()
{
    party::Parties p;
    p.invite("ada", "bo", 0);
    p.accept("bo", 0);
    p.invite("ada", "cy", 0);
    p.accept("cy", 0);
    bool boHere = true, cyHere = true;
    const auto online = [&](const std::string& who) { return who == "ada" || (who == "bo" && boHere) || (who == "cy" && cyHere); };
    p.tick(1, online);
    boHere = false;
    p.tick(10, online);
    expect(p.together("ada", "bo") && p.offlineSince("bo") == 10, "Bo leaves the world and keeps his place");
    expect(p.tick(10 + party::HoldSeconds - 1, online).empty() && p.together("ada", "bo"), "for ten minutes");
    const auto lost = p.tick(10 + party::HoldSeconds + 1, online);
    expect(lost.size() == 1 && lost[0].first == "bo" && !p.of("bo") && p.together("ada", "cy"), "then loses it");
    // Everyone gone: the party waits, then goes.
    party::Parties q;
    q.invite("ada", "bo", 0);
    q.accept("bo", 0);
    const auto nobody = [](const std::string&) { return false; };
    q.tick(100, nobody);
    expect(q.together("ada", "bo"), "a party with nobody in the world waits");
    q.tick(100 + party::HoldSeconds + 1, nobody);
    expect(!q.of("ada") && !q.of("bo"), "and goes after ten minutes");
}

void savedAndRead()
{
    party::Parties p;
    p.invite("ada", "bo", 0);
    p.accept("bo", 0);
    p.invite("ada", "cy", 0);
    p.accept("cy", 0);
    p.lead("ada", "cy");
    p.setAutoJoin("bo", false);
    p.tick(50, [](const std::string& who) { return who != "bo"; });
    party::Parties q;
    q.load(parsed(json::dump(p.save())));
    expect(q.of("bo") && q.of("bo")->leader == "cy" && q.of("bo")->members.size() == 3, "a party survives its save");
    expect(q.offlineSince("bo") == 50 && !q.autoJoin("bo") && q.autoJoin("ada"), "with who is away, and settings");
    expect(q.invite("cy", "di", 60).ok && q.accept("di", 60).ok && q.of("di")->id != "", "and goes on as before");
    party::Parties r;
    r.load(parsed("{\"parties\":[{\"id\":\"party-1\",\"leader\":\"x\",\"members\":[{\"id\":\"ada\"}]}]}"));
    expect(!r.of("ada"), "a saved party of one is dropped");
}

// ------------------------------------------------------------------ Through the game

struct Client final : game::Connection
{
    std::vector<json::Value> events, snapshots;
    sections::Cache cache;
    void event(const std::string& text) override
    {
        json::Value v;
        std::string error;
        expect(json::parse(text, v, error), "events are JSON: " + error);
        events.push_back(v);
    }
    void snapshot(const std::string& text) override
    {
        json::Value v;
        std::string error;
        expect(json::parse(text, v, error), "snapshots are JSON: " + error);
        expect(sections::fill(v, cache), "a snapshot can be filled from what the client holds");
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
    const json::Value& self() const { return snapshots.back()["self"]; }
    const json::Value* sees(const std::string& id) const
    {
        for (const auto& e : snapshots.back().array("entities"))
            if (e.string("id") == id)
                return &e;
        return nullptr;
    }
    bool offered(const std::string& id, const std::string& action) const
    {
        if (const auto* e = sees(id))
            for (const auto& a : e->array("actions"))
                if (a.asString() == action)
                    return true;
        return false;
    }
};

std::string cmd(std::initializer_list<std::pair<const char*, json::Value>> fields)
{
    auto o = json::Value::object();
    for (const auto& [k, v] : fields)
        o.add(k, v);
    return json::dump(o);
}

void run(game::Game& g, std::initializer_list<Client*> clients, double seconds)
{
    for (double t = 0; t < seconds; t += .05)
    {
        g.tick(.05);
        for (auto* c : clients)
            if (!c->snapshots.empty())
                g.acknowledge(c, c->snapshots.back().number("revision"), false);
    }
}

struct Three
{
    game::Game g;
    Client ada, bo, cy;
    explicit Three(game::Options o) : g(o)
    {
        std::string problem;
        expect(g.start(problem), "the demo world starts: " + problem);
        ada.id = 1;
        bo.id = 2;
        cy.id = 3;
        for (auto* c : {&ada, &bo, &cy})
            g.connect(c);
        g.command(&ada, cmd({{"type", "hello"}, {"id", "ada"}, {"name", "Ada"}}));
        g.command(&bo, cmd({{"type", "hello"}, {"id", "bo"}, {"name", "Bo"}}));
        g.command(&cy, cmd({{"type", "hello"}, {"id", "cy"}, {"name", "Cy"}}));
        auto* a = g.world().entity(ada.entityId);
        for (auto* c : {&bo, &cy})
        {
            auto* e = g.world().entity(c->entityId);
            e->cellId = a->cellId;
        }
        place(bo, 1.2, 0);
        place(cy, -2, 0);
        run(g, {&ada, &bo, &cy}, .5);
    }
    void place(Client& c, double dx, double dy)
    {
        const auto* a = g.world().entity(ada.entityId);
        g.world().entity(c.entityId)->position = {a->position.x + dx, a->position.y + dy};
    }
    void tick(double seconds) { run(g, {&ada, &bo, &cy}, seconds); }
    void form()
    {
        g.command(&ada, cmd({{"type", "action"}, {"action", "invite"}, {"target", bo.entityId}}));
        g.command(&bo, cmd({{"type", "party"}, {"verb", "accept"}}));
        tick(.3);
    }
};

game::Options devOptions()
{
    game::Options o;
    o.devIdentity = true;
    return o;
}

void invitationsAndTheView()
{
    Three t(devOptions());
    expect(t.ada.offered(t.bo.entityId, "invite"), "a player in sight can be invited");
    t.g.command(&t.ada, cmd({{"type", "action"}, {"action", "invite"}, {"target", t.bo.entityId}}));
    t.tick(.3);
    expect(t.bo.said().find("Ada invites you to join their party") != std::string::npos, "Bo is told:\n" + t.bo.said());
    const auto& invite = t.bo.self()["party"]["invite"];
    expect(invite.string("from") == t.ada.entityId && invite.string("name") == "Ada" && invite.number("seconds") > 50,
           "and his snapshot carries the invitation");
    t.g.command(&t.bo, cmd({{"type", "party"}, {"verb", "accept"}}));
    t.tick(.3);
    const auto& party = t.ada.self()["party"];
    expect(party.string("leader") == t.ada.entityId && party.array("members").size() == 2, "Ada's snapshot shows a party of two");
    const auto& bo = party.array("members")[1];
    expect(bo.string("name") == "Bo" && bo.boolean("online") && bo.string("cell") == t.g.world().entity(t.bo.entityId)->cellId &&
               std::abs(bo.number("x") - t.g.world().entity(t.bo.entityId)->position.x) < .2,
           "with where Bo is, for her minimap");
    expect(t.ada.said().find("Bo joins the party") != std::string::npos, "Ada is told he joined");
    expect(t.ada.sees(t.bo.entityId)->string("rel") == "party" && t.bo.sees(t.ada.entityId)->string("rel") == "party",
           "each shows as the other's party mate");
    expect(t.ada.sees(t.cy.entityId)->string("rel").empty(), "Cy is nobody to them");
    expect(!t.ada.offered(t.bo.entityId, "invite") && t.ada.offered(t.cy.entityId, "invite") && !t.bo.offered(t.cy.entityId, "invite"),
           "a party mate isn't offered an invitation, and only the leader may invite");
    t.g.command(&t.bo, cmd({{"type", "party"}, {"verb", "leave"}}));
    t.tick(.3);
    expect(t.ada.self()["party"].isNull() && t.ada.sees(t.bo.entityId)->string("rel").empty(), "Bo leaves: no party is left");
    expect(t.ada.said().find("Bo leaves the party") != std::string::npos, "and Ada is told");
}

void theTwoChats()
{
    Three t(devOptions());
    t.g.command(&t.cy, cmd({{"type", "chat"}, {"channel", "partyooc"}, {"text", "anyone?"}}));
    expect(t.cy.said().find("You are not in a party") != std::string::npos, "party chat needs a party");
    t.form();
    // In character, to the party: heard as speech is heard. Cy, close by, overhears; Bo is told it was to the party.
    for (auto* c : {&t.ada, &t.bo, &t.cy})
        c->events.clear();
    t.g.command(&t.ada, cmd({{"type", "chat"}, {"channel", "party"}, {"text", "\"We leave at dusk.\""}}));
    const auto heard = [](const Client& c, const std::string& words) -> const json::Value* {
        for (const auto& e : c.events)
            if (e.string("type") == "roleplay" && e.string("text").find(words) != std::string::npos)
                return &e;
        return nullptr;
    };
    expect(heard(t.bo, "dusk") && heard(t.bo, "dusk")->boolean("party"), "Bo hears it, as said to the party");
    expect(heard(t.ada, "dusk") && heard(t.ada, "dusk")->boolean("party"), "Ada sees her own line marked so");
    expect(heard(t.cy, "dusk") && !heard(t.cy, "dusk")->boolean("party"), "Cy overhears it as plain speech");
    // Far away, Bo hears nothing in character, but the party's OOC chat reaches him anywhere.
    t.place(t.bo, 200, 0);
    auto* b = t.g.world().entity(t.bo.entityId);
    for (const auto& [id, cell] : t.g.world().cells())
        if (id != b->cellId && cell.width > 4)
        {
            b->cellId = id;
            b->position = {2, 2};
            break;
        }
    t.tick(.6);
    for (auto* c : {&t.ada, &t.bo, &t.cy})
        c->events.clear();
    t.g.command(&t.ada, cmd({{"type", "chat"}, {"channel", "party"}, {"text", "\"Where are you?\""}}));
    expect(!heard(t.bo, "Where") && heard(t.cy, "Where"), "in character, a party mate out of earshot hears nothing");
    t.tick(.6);
    t.g.command(&t.ada, cmd({{"type", "chat"}, {"channel", "partyooc"}, {"text", "brb, meet at the gate"}}));
    const auto ooc = [](const Client& c) {
        for (const auto& e : c.events)
            if (e.string("type") == "ooc" && e.string("channel") == "partyooc" && e.string("text") == "brb, meet at the gate")
                return true;
        return false;
    };
    expect(ooc(t.bo) && ooc(t.ada) && !ooc(t.cy), "party OOC reaches the party wherever they are, and nobody else");
}

void calledIntoAPartyMatesFight()
{
    // Ada and Bo are a party; Cy challenges Ada and she accepts. Bo, watching, is called in on Ada's side after five
    // seconds; Cy shows red to both, and stays so after the fight.
    Three t(devOptions());
    t.form();
    t.place(t.cy, 0, 1.2);
    t.tick(.3);
    t.g.command(&t.cy, cmd({{"type", "action"}, {"action", "challenge"}, {"target", t.ada.entityId}}));
    t.g.command(&t.ada, cmd({{"type", "action"}, {"action", "accept"}}));
    t.tick(.5);
    expect(t.g.world().inBattle(t.ada.entityId) && t.g.world().inBattle(t.cy.entityId), "Ada and Cy fight");
    expect(!t.g.world().inBattle(t.bo.entityId), "Bo isn't in it yet");
    expect(t.bo.self()["party"].has("pull") && t.bo.self()["party"]["pull"].string("name") == "Ada", "he is being called in:\n" + t.bo.said());
    expect(t.bo.sees(t.cy.entityId) && t.bo.sees(t.cy.entityId)->string("rel") == "hostile" &&
               t.bo.sees(t.cy.entityId)->string("why") == "fighting your party",
           "Cy is red to Bo: fighting his party");
    t.tick(party::PullSeconds + .5);
    const auto* b = t.g.world().battleOf(t.bo.entityId);
    expect(b && b->fighter(t.bo.entityId) && b->fighter(t.bo.entityId)->side == b->fighter(t.ada.entityId)->side,
           "after five seconds Bo is in, on Ada's side:\n" + t.bo.said());
    expect(t.bo.said().find("You join Ada's fight") != std::string::npos, "and is told so");
}

void stayingOut()
{
    Three t(devOptions());
    t.form();
    t.place(t.cy, 0, 1.2);
    t.tick(.3);
    t.g.command(&t.cy, cmd({{"type", "action"}, {"action", "challenge"}, {"target", t.ada.entityId}}));
    t.g.command(&t.ada, cmd({{"type", "action"}, {"action", "accept"}}));
    t.tick(.5);
    t.g.command(&t.bo, cmd({{"type", "party"}, {"verb", "stayout"}}));
    t.tick(party::PullSeconds + 1);
    expect(!t.g.world().inBattle(t.bo.entityId) && !t.bo.self()["party"].has("pull"), "Bo stays out, and isn't called again");
    // Never pulled in at all, by his own setting.
    Three u(devOptions());
    u.form();
    u.g.command(&u.bo, cmd({{"type", "party"}, {"verb", "autojoin"}, {"on", false}}));
    u.place(u.cy, 0, 1.2);
    u.tick(.3);
    u.g.command(&u.cy, cmd({{"type", "action"}, {"action", "challenge"}, {"target", u.ada.entityId}}));
    u.g.command(&u.ada, cmd({{"type", "action"}, {"action", "accept"}}));
    u.tick(party::PullSeconds + 1);
    expect(!u.g.world().inBattle(u.bo.entityId) && u.bo.self()["party"].boolean("autoJoin", true) == false,
           "with auto-join off, a party mate's fight never calls him");
}

void aRestartKeepsTheParty()
{
    auto o = devOptions();
    o.forkSnapshots = false;
    o.savePath = "/tmp/ratw-party-test-" + std::to_string(::getpid()) + ".json";
    std::remove(o.savePath.c_str());
    {
        Three t(o);
        t.form();
        t.g.save();
    }
    {
        game::Game g(o);
        std::string problem;
        expect(g.start(problem), "restarts: " + problem);
        Client ada, bo;
        ada.id = 1;
        bo.id = 2;
        g.connect(&ada);
        g.connect(&bo);
        g.command(&ada, cmd({{"type", "hello"}, {"id", "ada"}, {"name", "Ada"}}));
        g.command(&bo, cmd({{"type", "hello"}, {"id", "bo"}, {"name", "Bo"}}));
        run(g, {&ada, &bo}, .5);
        const auto& party = ada.self()["party"];
        expect(party.array("members").size() == 2 && party.string("leader") == ada.entityId, "the party is still there after a restart");
    }
    std::remove(o.savePath.c_str());
}
} // namespace

int main()
{
    try
    {
        formingAndLeaving();
        placesKeptAndLost();
        savedAndRead();
        invitationsAndTheView();
        theTwoChats();
        calledIntoAPartyMatesFight();
        stayingOut();
        aRestartKeepsTheParty();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
    std::cout << "party tests: " << checks << " checks passed\n";
    return 0;
}
