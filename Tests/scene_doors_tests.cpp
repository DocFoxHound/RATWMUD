// Scenes' openness, joining and knocking through the game (Docs/Design/51-scenes-and-stars.md, Phase 3): a party's scene
// starts Private and nobody nearby sees it; opened, a wolf in earshot sees it as "2 wolves · open" (never names) and
// joins with one click, and their next line counts; a Knock scene tells its wolves who knocks, and one let in counts at
// once; a member who blocked someone keeps them out of it, without saying who.
#include "RatwGame.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
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
    const json::Value* last(const std::string& type) const
    {
        for (auto it = events.rbegin(); it != events.rend(); ++it)
            if (it->string("type") == type)
                return &*it;
        return nullptr;
    }
    const json::Value* seen(const std::string& id) const
    {
        if (snapshots.empty())
            return nullptr;
        for (const auto& e : snapshots.back().array("entities"))
            if (e.string("id") == id)
                return &e;
        return nullptr;
    }
    // Private messages received (not one's own copies) with this text in them.
    const json::Value* message(const std::string& text) const
    {
        for (const auto& e : events)
            if (e.string("type") == "ooc" && e.string("channel") == "private" && !e.boolean("outgoing") &&
                e.string("text").find(text) != std::string::npos)
                return &e;
        return nullptr;
    }
    bool said(const std::string& text) const
    {
        for (const auto& e : events)
            if (e.string("text").find(text) != std::string::npos)
                return true;
        return false;
    }
    // A friend on this player's list, by handle.
    const json::Value* friendRow(const std::string& handle) const
    {
        if (const auto* f = last("friends"))
            for (const auto& row : f->array("friends"))
                if (row.string("handle") == handle)
                    return &row;
        return nullptr;
    }
    bool asked(const std::string& handle, const char* list = "incoming") const
    {
        if (const auto* f = last("friends"))
            for (const auto& row : f->array(list))
                if (row.string("handle") == handle)
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

json::Value appearance()
{
    return parsed(R"({"species": "timber", "sex": "female", "stature": "average", "pattern": "solid", "baseColor": 3, "gradientColor": 1,
                      "markingColor": 5, "gradientAmount": 0.5, "patternAmount": 0.5})");
}

game::Options options(const std::string& save = {})
{
    game::Options o;
    o.hiddenNames = false;
    o.forkSnapshots = false;
    o.oneWolfPerAccount = true;
    if (!save.empty())
        o.savePath = save;
    return o;
}

// Three accounts: ada (Ada), bob (Bo One, Bo Two) and cyd (Cy), each with a handle unless told not to.
struct World3
{
    game::Game g;
    Client ada, bo, cy;
    std::string adaId, bo1, bo2, cyId;
    explicit World3(game::Options o, bool fresh = true) : g(o)
    {
        std::string problem;
        expect(g.start(problem), "starts: " + problem);
        int n = 1;
        for (auto* c : {&ada, &bo, &cy})
        {
            c->id = n++;
            g.connect(c);
        }
        const auto make = [&](Client& c, const char* user, std::initializer_list<const char*> names) {
            g.command(&c, cmd({{"type", fresh ? "auth_register" : "auth_login"}, {"username", user}, {"password", "a long enough password"}}));
            g.settle();
            int i = 0;
            if (fresh)
                for (const char* name : names)
                    g.command(&c, cmd({{"type", "character_create"}, {"name", name}, {"age", 24}, {"appearance", appearance()},
                                       {"commandId", std::string(user) + std::to_string(i++)}}));
            g.settle();
            std::vector<std::string> ids;
            expect(c.last("lobby") != nullptr, std::string("a lobby for ") + user);
            for (const auto& ch : c.last("lobby")->array("characters"))
                ids.push_back(ch.string("id"));
            expect(ids.size() == names.size(), std::string("characters for ") + user + ": " + c.last("lobby")->string("message"));
            return ids;
        };
        adaId = make(ada, "ada", {"Ada"})[0];
        const auto bos = make(bo, "bob", {"Bo One", "Bo Two"});
        bo1 = bos[0];
        bo2 = bos[1];
        cyId = make(cy, "cyd", {"Cy"})[0];
        enter(ada, adaId);
        enter(bo, bo1);
        enter(cy, cyId);
    }
    void enter(Client& c, const std::string& id)
    {
        g.command(&c, cmd({{"type", "character_enter"}, {"id", id}}));
        g.settle();
        auto* e = g.world().entity(id);
        auto* a = g.world().entity(adaId);
        expect(e != nullptr, "entered: " + id);
        if (a && e != a)
        {
            e->cellId = a->cellId;
            e->position = {a->position.x + 1.2 * double(&c == &cy ? 2 : 1), a->position.y};
        }
        tick(.3);
    }
    void leave(Client& c)
    {
        g.command(&c, cmd({{"type", "character_leave"}}));
        g.settle();
    }
    void tick(double seconds)
    {
        for (double t = 0; t < seconds; t += .05)
        {
            g.tick(.05);
            for (auto* c : {&ada, &bo, &cy})
                if (!c->snapshots.empty())
                    g.acknowledge(c, c->snapshots.back().number("revision"), false);
        }
    }
    void send(Client& c, const std::string& type, std::initializer_list<std::pair<const char*, json::Value>> fields)
    {
        auto o = json::Value::object();
        o.add("type", type);
        for (const auto& [k, v] : fields)
            o.add(k, v);
        g.command(&c, json::dump(o));
        g.settle();
    }
    void friends(Client& c, std::initializer_list<std::pair<const char*, json::Value>> fields) { send(c, "friends", fields); }
    void message(Client& c, const std::string& to, const std::string& text)
    {
        send(c, "chat", {{"channel", "private"}, {"to", to}, {"text", text}});
        tick(1.0);                                  // (Past chat's rate limit before the next line.)
    }
    void handles()
    {
        send(ada, "profile", {{"verb", "handle"}, {"handle", "Adder"}});
        send(bo, "profile", {{"verb", "handle"}, {"handle", "Bobbin"}});
        send(cy, "profile", {{"verb", "handle"}, {"handle", "Cypress"}});
    }
    void befriend(Client& a, Client& b, const char* bHandle, const char* aHandle)
    {
        friends(a, {{"verb", "request"}, {"handle", bHandle}});
        friends(b, {{"verb", "accept"}, {"handle", aHandle}});
    }
};


// A party scene between Ada and Bo: its id.
std::string partyScene(World3& w)
{
    w.send(w.ada, "action", {{"action", "invite"}, {"target", w.bo1}});
    w.send(w.bo, "party", {{"verb", "accept"}});
    w.tick(.3);
    w.send(w.ada, "chat", {{"text", "\"The river rose in the night and took the lower bridge with it.\""}, {"channel", "party"}});
    w.tick(1.0);
    w.send(w.bo, "chat", {{"text", "\"Then we'll go round by the old mill, if the dog lets us.\""}, {"channel", "party"}});
    w.tick(1.0);
    for (const auto& sid : w.g.ledger().scenesOf(w.adaId))
        return sid;
    return {};
}
const json::Value& social(Client& c) { return c.snapshots.back().object("self").object("social"); }
void say(World3& w, Client& c, const std::string& text)
{
    w.send(c, "chat", {{"text", text}, {"channel", ""}});
    w.tick(1.0);
}

void joining()
{
    World3 w(options());
    const auto sid = partyScene(w);
    expect(!sid.empty() && w.g.ledger().sessions.at(sid).openness == "private", "a party's scene, private");
    w.tick(2.5);
    expect(social(w.cy).array("nearby").empty(), "Cy, beside it, doesn't see a private scene");
    w.send(w.ada, "social", {{"verb", "openness"}, {"session", sid}, {"value", "open"}});
    expect(w.ada.said("The scene is open") && w.bo.said("opened the scene."), "Ada opens it; Bo is told");
    w.tick(2.5);
    const auto& near = social(w.cy).array("nearby");
    expect(near.size() == 1 && near[0].string("id") == sid && near[0].number("wolves") == 2 && near[0].string("openness") == "open" &&
               !near[0].has("with") && !near[0].has("names"),
           "Cy sees it: 2 wolves, open, no names: " + json::dump(social(w.cy)));
    expect(social(w.ada).array("scenes")[0].string("openness") == "open", "its wolves see it open");
    w.send(w.cy, "social", {{"verb", "join"}, {"session", sid}});
    expect(w.cy.said("You join the scene"), "Cy joins");
    say(w, w.cy, "\"Mind if I sit with you? I came in out of the rain.\"");
    expect(w.g.ledger().sessions.at(sid).members.count(w.cyId), "her next line counts in it");
}

void knocking()
{
    World3 w(options());
    const auto sid = partyScene(w);
    w.send(w.ada, "social", {{"verb", "openness"}, {"session", sid}, {"value", "knock"}});
    w.tick(2.5);
    expect(social(w.cy).array("nearby").size() == 1 && social(w.cy).array("nearby")[0].string("openness") == "knock", "Cy sees a Knock scene");
    w.send(w.cy, "social", {{"verb", "join"}, {"session", sid}});
    expect(w.cy.said("Knock to join that scene."), "no Join on it");
    say(w, w.cy, "\"Is anyone sitting here?\"");
    expect(!w.g.ledger().sessions.at(sid).members.count(w.cyId), "speaking near it doesn't put her in it");
    w.send(w.cy, "social", {{"verb", "knock"}, {"session", sid}});
    expect(w.cy.said("You knock."), "she knocks");
    const auto* knock = w.ada.last("knock");
    expect(knock && knock->string("session") == sid && !knock->string("from").empty() && w.bo.last("knock"), "both its wolves hear the knock");
    expect(w.ada.said("is knocking"), "with a line where they write");
    w.tick(2.5);
    const auto& knocks = social(w.ada).array("scenes")[0].array("knocks");
    expect(knocks.size() == 1 && knocks[0].string("id") == w.cyId, "Ada's scene line shows the knock");
    expect(social(w.cy).array("nearby")[0].boolean("knocked"), "Cy's shows she has knocked");
    w.send(w.ada, "social", {{"verb", "admit"}, {"session", sid}, {"who", w.cyId}});
    expect(w.cy.said("You're let in") && w.bo.said(" in."), "let in: Cy and Bo are told");
    ::usleep(2100000);                              // (The ledger takes a wolf's line every two real seconds.)
    say(w, w.cy, "\"Thank you. The rain shows no sign of stopping.\"");
    expect(w.g.ledger().sessions.at(sid).members.count(w.cyId), "her next line counts");
}

void blocksKeepOut()
{
    World3 w(options());
    const auto sid = partyScene(w);
    w.send(w.bo, "safety", {{"verb", "block"}, {"target", w.cyId}});
    w.send(w.ada, "social", {{"verb", "openness"}, {"session", sid}, {"value", "open"}});
    w.tick(2.5);
    expect(social(w.cy).array("nearby").empty(), "a scene with one who blocked her isn't offered to Cy");
    w.send(w.cy, "social", {{"verb", "join"}, {"session", sid}});
    expect(w.cy.said("You can't join that scene.") && !w.cy.said("block"), "joining is refused, without saying who");
    w.g.ledger().sessions.at(sid).openness = "knock";
    w.send(w.cy, "social", {{"verb", "knock"}, {"session", sid}});
    expect(w.cy.said("No answer.") && !w.ada.last("knock"), "a knock goes unheard");
}
} // namespace

int main()
{
    try
    {
        joining();
        knocking();
        blocksKeepOut();
    }
    catch (const std::exception& e)
    {
        std::cerr << "scene_doors_tests failed after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
    std::cout << "scene_doors_tests passed (" << checks << " checks)\n";
    return 0;
}
