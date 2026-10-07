// Circles (Docs/Design/50-player-card-friends-safety.md, Phase 5): making one (a handle, a unique name, 10 an account);
// invitations by handle, from its keeper and officers only, never between a blocked pair; officers' and the keeper's
// rights; the roster showing a member's wolf only where they share it with the circle; its chat reaching every member
// in the world, wherever, and never one who muted or blocked the speaker; planned nights in order, past ones dropped;
// a keeper who leaves hands it on; the last to leave ends it; and all of it kept across a restart.
#include "RatwGame.h"

#include <chrono>
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


double unixNow()
{
    return double(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

// The circles one player sees, by name.
const json::Value* circleNamed(const Client& c, const std::string& name)
{
    if (const auto* e = c.last("circles"))
        for (const auto& circle : e->array("circles"))
            if (circle.string("name") == name)
                return &circle;
    return nullptr;
}
const json::Value* memberOf(const json::Value* circle, const std::string& handle)
{
    if (circle)
        for (const auto& m : circle->array("members"))
            if (m.string("handle") == handle)
                return &m;
    return nullptr;
}
bool invitedTo(const Client& c, const std::string& name)
{
    if (const auto* e = c.last("circles"))
        for (const auto& i : e->array("invites"))
            if (i.string("name") == name)
                return true;
    return false;
}
// A circle line received (not one's own copy) with this text in it.
const json::Value* circleLine(const Client& c, const std::string& text)
{
    for (const auto& e : c.events)
        if (e.string("type") == "ooc" && e.string("channel") == "circle" && !e.boolean("outgoing") &&
            e.string("text").find(text) != std::string::npos)
            return &e;
    return nullptr;
}

void circle(World3& w, Client& c, std::initializer_list<std::pair<const char*, json::Value>> fields) { w.send(c, "circle", fields); }
std::string idOf(const Client& c, const std::string& name)
{
    const auto* found = circleNamed(c, name);
    return found ? found->string("id") : std::string();
}
void say(World3& w, Client& c, const std::string& id, const std::string& text)
{
    w.send(c, "chat", {{"channel", "circle"}, {"circle", id}, {"text", text}});
    w.tick(1.0);
}

void making()
{
    World3 w(options());
    circle(w, w.ada, {{"verb", "create"}, {"name", "Moot Night"}});
    expect(w.ada.said("Choose a handle first"), "a handle first");
    w.handles();
    circle(w, w.ada, {{"verb", "create"}, {"name", ""}});
    expect(w.ada.said("A circle needs a name."), "a name");
    circle(w, w.ada, {{"verb", "create"}, {"name", std::string(33, 'x')}});
    expect(w.ada.said("at most 32 letters"), "32 letters at most");
    circle(w, w.ada, {{"verb", "create"}, {"name", "Moot Night"}});
    const auto* moot = circleNamed(w.ada, "Moot Night");
    expect(moot && moot->string("role") == "keeper" && moot->array("members").size() == 1, "made, with Ada its keeper");
    circle(w, w.bo, {{"verb", "create"}, {"name", "moot night"}});
    expect(w.bo.said("There is a circle by that name already."), "names are unique, any case");
    for (int i = 0; i < 9; ++i)
        circle(w, w.ada, {{"verb", "create"}, {"name", "Circle " + std::to_string(i)}});
    circle(w, w.ada, {{"verb", "create"}, {"name", "One Too Many"}});
    expect(w.ada.said("You are in 10 circles already.") && !circleNamed(w.ada, "One Too Many"), "10 an account");
}

void invitingAndRanks()
{
    World3 w(options());
    w.handles();
    circle(w, w.ada, {{"verb", "create"}, {"name", "Moot Night"}});
    const auto id = idOf(w.ada, "Moot Night");
    circle(w, w.ada, {{"verb", "invite"}, {"circle", id}, {"handle", "Nobody"}});
    expect(w.ada.said("No one by that handle can be invited."), "an unknown handle");
    circle(w, w.ada, {{"verb", "invite"}, {"circle", id}, {"handle", "bobbin"}});
    expect(invitedTo(w.bo, "Moot Night") && w.bo.said("Adder invites you to the circle Moot Night"), "Bo is invited, and told");
    expect(circleNamed(w.ada, "Moot Night")->array("invited").size() == 1, "Ada sees it waiting");
    circle(w, w.bo, {{"verb", "accept"}, {"circle", id}});
    expect(memberOf(circleNamed(w.ada, "Moot Night"), "Bobbin") && circleNamed(w.bo, "Moot Night"), "Bo joins; both rosters show it");
    // A member can't invite, remove or plan; the keeper names an officer, who can.
    circle(w, w.bo, {{"verb", "invite"}, {"circle", id}, {"handle", "Cypress"}});
    expect(w.bo.said("Only its keeper and officers invite.") && !invitedTo(w.cy, "Moot Night"), "a member can't invite");
    circle(w, w.bo, {{"verb", "night"}, {"circle", id}, {"at", unixNow() + 3600}, {"place", "the Wharf"}});
    expect(w.bo.said("Only its keeper and officers plan nights."), "nor plan nights");
    circle(w, w.bo, {{"verb", "officer"}, {"circle", id}, {"handle", "Adder"}, {"on", true}});
    expect(w.bo.said("Only its keeper names officers."), "nor name officers");
    circle(w, w.ada, {{"verb", "officer"}, {"circle", id}, {"handle", "Bobbin"}, {"on", true}});
    expect(memberOf(circleNamed(w.bo, "Moot Night"), "Bobbin")->string("role") == "officer", "Bo is an officer");
    circle(w, w.bo, {{"verb", "invite"}, {"circle", id}, {"handle", "Cypress"}});
    circle(w, w.cy, {{"verb", "decline"}, {"circle", id}});
    expect(!invitedTo(w.cy, "Moot Night") && !memberOf(circleNamed(w.ada, "Moot Night"), "Cypress"), "Cy declines");
    circle(w, w.bo, {{"verb", "invite"}, {"circle", id}, {"handle", "Cypress"}});
    circle(w, w.cy, {{"verb", "accept"}, {"circle", id}});
    circle(w, w.bo, {{"verb", "remove"}, {"circle", id}, {"handle", "Adder"}});
    expect(w.bo.said("You can't remove them.") && memberOf(circleNamed(w.bo, "Moot Night"), "Adder"), "an officer can't remove the keeper");
    circle(w, w.bo, {{"verb", "remove"}, {"circle", id}, {"handle", "Cypress"}});
    expect(!memberOf(circleNamed(w.ada, "Moot Night"), "Cypress") && !circleNamed(w.cy, "Moot Night"), "but can remove a member");
    // Blocks: no invitation between a blocked pair, either way, with an unknown handle's refusal.
    w.send(w.cy, "safety", {{"verb", "block"}, {"target", w.bo1}});
    circle(w, w.bo, {{"verb", "invite"}, {"circle", id}, {"handle", "Cypress"}});
    expect(w.bo.said("No one by that handle can be invited.") && !invitedTo(w.cy, "Moot Night"), "Cy has blocked Bo: he can't invite her");
}

void rosterAndChat()
{
    World3 w(options());
    w.handles();
    circle(w, w.ada, {{"verb", "create"}, {"name", "Moot Night"}});
    const auto id = idOf(w.ada, "Moot Night");
    for (auto* c : {&w.bo, &w.cy})
    {
        circle(w, w.ada, {{"verb", "invite"}, {"circle", id}, {"handle", c == &w.bo ? "Bobbin" : "Cypress"}});
        circle(w, *c, {{"verb", "accept"}, {"circle", id}});
    }
    // The roster: here or away, and a wolf only where its player shares it with the circle (off at first).
    auto* bo = memberOf(circleNamed(w.ada, "Moot Night"), "Bobbin");
    expect(bo && bo->boolean("online") && !bo->has("character"), "Bo here, his wolf not shown");
    circle(w, w.bo, {{"verb", "share"}, {"circle", id}, {"on", true}});
    bo = memberOf(circleNamed(w.ada, "Moot Night"), "Bobbin");
    expect(bo && bo->string("character") == "Bo One", "shared with the circle: Bo One");
    // Its chat: every member in the world, by handle; one's own copy.
    say(w, w.ada, id, "Moot at the Wharf tonight?");
    const auto* line = circleLine(w.bo, "Moot at the Wharf");
    expect(line && line->string("speaker") == "Adder" && line->string("circleName") == "Moot Night", "Bo reads it, by handle");
    expect(circleLine(w.cy, "Moot at the Wharf"), "and Cy");
    bool own = false;
    for (const auto& e : w.ada.events)
        own |= e.string("channel") == "circle" && e.boolean("outgoing") && e.string("text") == "Moot at the Wharf tonight?";
    expect(own, "Ada gets her own copy");
    // A blocked member's lines never reach the blocker; the rest still read them.
    w.send(w.cy, "safety", {{"verb", "block"}, {"target", w.bo1}});
    say(w, w.bo, id, "I'll bring the cider.");
    expect(!circleLine(w.cy, "bring the cider") && circleLine(w.ada, "bring the cider"), "blocked: not to Cy, still to Ada");
    expect(!w.bo.said("blocked") && !w.bo.said("Blocked"), "and Bo isn't told");
    // Not a member: no line, either way.
    circle(w, w.cy, {{"verb", "leave"}, {"circle", id}});
    say(w, w.ada, id, "Cy left?");
    expect(!circleLine(w.cy, "Cy left?"), "one who left reads nothing");
    w.send(w.cy, "chat", {{"channel", "circle"}, {"circle", id}, {"text", "hello?"}});
    expect(w.cy.said("You aren't in that circle.") && !circleLine(w.ada, "hello?"), "nor writes");
    // Away: a member out of the world gets nothing (circles keep no inbox) and shows away.
    w.leave(w.bo);
    expect(!memberOf(circleNamed(w.ada, "Moot Night"), "Bobbin")->boolean("online"), "Bo away on the roster");
}

void nightsAndLeaving()
{
    const std::string save = "/tmp/ratw-circles-" + std::to_string(::getpid()) + ".json";
    std::remove(save.c_str());
    std::string id;
    {
        World3 w(options(save));
        w.handles();
        circle(w, w.ada, {{"verb", "create"}, {"name", "Moot Night"}});
        id = idOf(w.ada, "Moot Night");
        const double t = unixNow();
        circle(w, w.ada, {{"verb", "night"}, {"circle", id}, {"at", t + 7200}, {"place", "the Wharf"}, {"line", "Bring a story."}});
        circle(w, w.ada, {{"verb", "night"}, {"circle", id}, {"at", t + 3600}, {"place", "the mill"}});
        circle(w, w.ada, {{"verb", "night"}, {"circle", id}, {"at", t - 3600}, {"place", "yesterday"}});
        expect(w.ada.said("Choose a time within the coming year."), "not in the past");
        circle(w, w.ada, {{"verb", "night"}, {"circle", id}, {"at", t + 3600}, {"place", ""}});
        expect(w.ada.said("Say where."), "a place");
        const auto nights = circleNamed(w.ada, "Moot Night")->array("nights");
        expect(nights.size() == 2 && nights[0].string("place") == "the mill" && nights[1].string("line") == "Bring a story." &&
                   nights[1].string("by") == "Adder",
               "two nights, soonest first");
        circle(w, w.ada, {{"verb", "unnight"}, {"circle", id}, {"night", nights[0].string("id")}});
        expect(circleNamed(w.ada, "Moot Night")->array("nights").size() == 1, "one taken off");
        for (auto* c : {&w.bo, &w.cy})
        {
            circle(w, w.ada, {{"verb", "invite"}, {"circle", id}, {"handle", c == &w.bo ? "Bobbin" : "Cypress"}});
            circle(w, *c, {{"verb", "accept"}, {"circle", id}});
        }
        circle(w, w.ada, {{"verb", "officer"}, {"circle", id}, {"handle", "Cypress"}, {"on", true}});
        circle(w, w.cy, {{"verb", "share"}, {"circle", id}, {"on", true}});
        w.g.save();
    }
    {
        // Across a restart: members, ranks, sharing, the night.
        World3 w(options(save), false);
        const auto* moot = circleNamed(w.ada, "Moot Night");
        expect(moot && moot->array("members").size() == 3 && moot->array("nights").size() == 1, "kept: " + (moot ? json::dump(*moot) : std::string()));
        expect(memberOf(moot, "Cypress")->string("role") == "officer" && memberOf(moot, "Cypress")->string("character") == "Cy",
               "Cy an officer, sharing her wolf");
        // The keeper leaves: the officer keeps it. The last to leave ends it.
        circle(w, w.ada, {{"verb", "leave"}, {"circle", id}});
        expect(!circleNamed(w.ada, "Moot Night") && memberOf(circleNamed(w.cy, "Moot Night"), "Cypress")->string("role") == "keeper",
               "Ada leaves; Cy keeps it");
        circle(w, w.bo, {{"verb", "disband"}, {"circle", id}});
        expect(w.bo.said("Only its keeper ends a circle."), "only the keeper ends it");
        circle(w, w.bo, {{"verb", "leave"}, {"circle", id}});
        circle(w, w.cy, {{"verb", "leave"}, {"circle", id}});
        expect(!circleNamed(w.cy, "Moot Night"), "the last to leave ends it");
        circle(w, w.cy, {{"verb", "create"}, {"name", "Moot Night"}});
        expect(circleNamed(w.cy, "Moot Night"), "and its name is free again");
        const auto again = idOf(w.cy, "Moot Night");
        circle(w, w.cy, {{"verb", "night"}, {"circle", again}, {"at", unixNow() + 60}, {"place", "the Wharf"}});
        w.g.save();
    }
    {
        // A night a day past drops off.
        std::ifstream in(save);
        std::stringstream text;
        text << in.rdbuf();
        auto doc = parsed(text.str());
        auto people = doc.object("people");
        auto circles = json::Value::array();
        for (auto c : people.array("circles"))
        {
            auto nights = json::Value::array();
            for (auto n : c.array("nights"))
            {
                n.set("at", n.number("at") - 3 * 86400);
                nights.push(n);
            }
            c.set("nights", nights);
            circles.push(c);
        }
        people.set("circles", circles);
        doc.set("people", people);
        std::ofstream(save) << json::dump(doc);
    }
    {
        World3 w(options(save), false);
        w.tick(.5);
        expect(circleNamed(w.cy, "Moot Night") && circleNamed(w.cy, "Moot Night")->array("nights").empty(), "a night past is gone");
    }
    std::remove(save.c_str());
}
} // namespace

int main()
{
    try
    {
        making();
        invitingAndRanks();
        rosterAndChat();
        nightsAndLeaving();
    }
    catch (const std::exception& e)
    {
        std::cerr << "circles_tests failed after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
    std::cout << "circles_tests passed (" << checks << " checks)\n";
    return 0;
}
