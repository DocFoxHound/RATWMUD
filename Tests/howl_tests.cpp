// The gathering howl and chorus (Docs/Design/51-scenes-and-stars.md, Phase 6): a howl heard far off as a direction and
// a distance, never a place or a name; a deaf wolf, or one who blocked the howler, hears nothing; the cooldown; a howl
// near joins a chorus, one mark updated with its count; those who howled together grow closer, and it is a moment of a
// scene they share; and not in a fight, or with something in the mouth.
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
    o.tiesOptional = true;                      // (Ties: doc 52, tested in newcomer_tests.)
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


// All three outdoors (the test world's one place outside, 40 by 28): Ada at the west, Bo 20 tiles east of her, Cy 12
// tiles south.
std::string outdoors(World3& w)
{
    auto& world = w.g.world();
    std::string place;
    for (const auto& [id, c] : world.cells())
        if (c.outdoors && (place.empty() || c.width * c.height > world.cell(place)->width * world.cell(place)->height))
            place = id;
    expect(!place.empty(), "a place outdoors");
    const double x = 8.5, y = 12.5;
    const auto ok = [](const Result& r) { return r.ok; };
    expect(ok(world.teleport(w.adaId, place, x, y)) && ok(world.teleport(w.bo1, place, x + 20, y)) && ok(world.teleport(w.cyId, place, x, y + 12)),
           "all three there");
    w.tick(.5);
    return place;
}
void howl(World3& w, Client& c) { w.send(c, "howl", {}); }

void heardFarOff()
{
    World3 w(options());
    outdoors(w);
    w.g.world().entity(w.cyId)->hearing = 0;        // (Cy hears nothing.)
    howl(w, w.ada);
    expect(w.ada.said("You howl."), "Ada howls");
    const auto* heard = w.bo.last("howl");
    expect(heard && heard->string("band") == "near" && heard->number("wolves") == 1 && heard->boolean("canJoin"),
           "Bo hears it near, and could join: " + (heard ? json::dump(*heard) : std::string()));
    const double bearing = heard->number("bearing");
    expect(std::abs(bearing - 270) <= 12.5, "from the west, within the jitter: " + std::to_string(bearing));
    expect(!heard->has("name") && !heard->has("x") && !heard->has("cell") && !heard->has("from"), "no name, no place");
    expect(w.bo.said("A howl rises to the west, near by."), "and a line: " + std::string(w.bo.last("system") ? w.bo.last("system")->string("text") : ""));
    expect(!w.cy.last("howl"), "deaf Cy hears nothing");
    // Residents near turn toward it, and one or two say something (authored lines; no model).
    int residents = 0;
    for (const auto& [id, e] : w.g.world().entities())
        residents += e.npc && !e.dead && e.cellId == w.g.world().entity(w.adaId)->cellId;
    if (residents > 0)
    {
        bool spoke = false;
        for (const auto& e : w.bo.events)
            if (e.string("type") == "roleplay")
                for (const char* line : {"Hear that? Someone's calling.", "Howling, at this hour? Something's afoot.", "That's a wolf calling for company.",
                                         "A howl, out there in the dark.", "Someone's calling under the moon."})
                    spoke |= e.string("text").find(line) != std::string::npos;
        expect(spoke, "a resident near says something of it");
    }
    howl(w, w.ada);
    expect(w.ada.said("Your voice needs a rest"), "a howl, then a rest");
    // Not with something in the mouth.
    w.g.world().entity(w.bo1)->mouth = "sword";
    howl(w, w.bo);
    expect(w.bo.said("Not with something in your mouth."), "not with a sword in the jaws");
}

void aChorus()
{
    World3 w(options());
    outdoors(w);
    // Ada and Bo in a scene together, so the chorus is a moment of it.
    w.send(w.ada, "action", {{"action", "invite"}, {"target", w.bo1}});
    w.send(w.bo, "party", {{"verb", "accept"}});
    w.send(w.ada, "chat", {{"text", "\"Shall we call the others? The light is going.\""}, {"channel", "party"}});
    w.tick(1.0);
    howl(w, w.ada);
    const auto first = w.cy.last("howl")->string("id");
    howl(w, w.bo);
    expect(w.bo.said("You join the howl."), "Bo joins it");
    const auto* chorus = w.cy.last("howl");
    expect(chorus->string("id") == first && chorus->number("wolves") == 2, "Cy: one mark, now two wolves");
    expect(w.cy.said("More wolves join the howl"), "and told so");
    const auto* before = w.g.world().bonds().find(w.adaId, w.bo1);
    const double familiarity = before ? before->familiarity : 0, affinity = before ? before->affinity : 0;
    w.tick(13.0);                                   // (The chorus closes: eight seconds, and four for Bo.)
    const auto* after = w.g.world().bonds().find(w.adaId, w.bo1);
    expect(after && after->familiarity > familiarity && after->affinity > affinity, "howling together, they grow closer");
    bool moment = false;
    for (const auto& sid : w.g.ledger().scenesOf(w.adaId))
        for (const auto& m : w.g.ledger().sessions.at(sid).moments)
            moment |= m.kind == "chorus";
    expect(moment, "a moment of the scene they share");
}

void blockedHearsNothing()
{
    World3 w(options());
    outdoors(w);
    w.send(w.cy, "safety", {{"verb", "block"}, {"target", w.adaId}});
    howl(w, w.ada);
    expect(!w.cy.last("howl") && w.bo.last("howl"), "Cy, who blocked Ada, hears nothing of her howl; Bo does");
}
} // namespace

int main()
{
    try
    {
        heardFarOff();
        aChorus();
        blockedHearsNothing();
    }
    catch (const std::exception& e)
    {
        std::cerr << "howl_tests failed after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
    std::cout << "howl_tests passed (" << checks << " checks)\n";
    return 0;
}
