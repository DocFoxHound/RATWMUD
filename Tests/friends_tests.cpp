// Friends and private messages (Docs/Design/50-player-card-friends-safety.md, Phase 3): a request needs the other's
// accept; the list shows a friend's character only when they share it, and a sharing friend's handle shows under their
// label while strangers never see one; private messages reach a friend anywhere and a stranger never, wait for an
// offline friend (50 at most, gone after 14 days) and arrive when they next come; blocks end friendships and refuse
// messages; a DM's silence stops them; and all of it is kept across a restart.
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

void requests()
{
    World3 w(options());
    w.friends(w.ada, {{"verb", "request"}, {"handle", "Bobbin"}});
    expect(w.ada.said("Choose a handle first"), "no request before one has a handle of one's own");
    w.handles();
    w.friends(w.ada, {{"verb", "request"}, {"handle", "Nobody Here"}});
    expect(w.ada.said("No one by that handle is taking friend requests."), "an unknown handle: refused");
    w.friends(w.ada, {{"verb", "request"}, {"handle", "adder"}});
    expect(w.ada.said("Not yourself."), "not oneself (handles match without case)");
    w.friends(w.ada, {{"verb", "request"}, {"handle", "bobbin"}});
    expect(w.ada.said("Friend request sent to Bobbin."), "a request by handle, any case");
    expect(w.ada.asked("Bobbin", "outgoing"), "Ada sees it waiting");
    expect(w.bo.asked("Adder"), "Bo sees Adder asking");
    expect(w.bo.last("friends")->string("toast") == "Adder asked to be your friend.", "and is told, as a toast");
    expect(!w.g.areFriends(w.adaId, w.bo1), "not friends until Bo accepts");
    w.message(w.ada, "Bobbin", "hello?");
    expect(!w.bo.message("hello?") && w.ada.said("Private messages go to friends only."), "no private messages before");
    w.friends(w.bo, {{"verb", "accept"}, {"handle", "Adder"}});
    expect(w.g.areFriends(w.adaId, w.bo1) && w.g.areFriends(w.bo2, w.adaId), "friends, by account: Bo's other wolf too");
    expect(w.ada.last("friends")->string("toast") == "Bobbin accepted your friend request.", "Ada hears of it");
    const auto* bob = w.ada.friendRow("Bobbin");
    expect(bob && bob->boolean("online") && bob->string("character") == "Bo One" && bob->boolean("shares"),
           "Ada's list: Bobbin, here, playing Bo One (sharing is on by default)");
    expect(!w.ada.asked("Bobbin", "outgoing") && !w.bo.asked("Adder"), "the request is gone");
    // Asking back is accepting; declining tells no one; a request withdrawn goes.
    w.friends(w.cy, {{"verb", "request"}, {"handle", "Adder"}});
    w.friends(w.ada, {{"verb", "request"}, {"handle", "Cypress"}});
    expect(w.g.areFriends(w.adaId, w.cyId), "asking back is accepting");
    w.friends(w.ada, {{"verb", "remove"}, {"handle", "Cypress"}});
    expect(!w.g.areFriends(w.adaId, w.cyId) && !w.cy.friendRow("Adder"), "removed, from both lists");
    w.friends(w.cy, {{"verb", "request"}, {"handle", "Adder"}});
    const auto before = w.cy.events.size();
    w.friends(w.ada, {{"verb", "decline"}, {"handle", "Cypress"}});
    expect(!w.cy.asked("Adder", "outgoing"), "declined: it leaves Cy's list");
    bool toldCy = false;
    for (auto i = before; i < w.cy.events.size(); ++i)
        toldCy |= w.cy.events[i].string("text").find("ecline") != std::string::npos || w.cy.events[i].has("toast");
    expect(!toldCy, "and Cy isn't told she was declined");
    w.friends(w.cy, {{"verb", "request"}, {"target", w.adaId}});
    expect(w.ada.asked("Cypress"), "a request in person, from a card");
    for (const auto& row : w.ada.last("friends")->array("incoming"))
        if (row.string("handle") == "Cypress")
            expect(row.string("wolf") == "Cy", "it says which wolf asked: " + row.string("wolf"));
    w.friends(w.cy, {{"verb", "cancel"}, {"handle", "Adder"}});
    expect(!w.ada.asked("Cypress"), "withdrawn");
}

void sharingAndHandles()
{
    World3 w(options());
    w.handles();
    w.befriend(w.ada, w.bo, "Bobbin", "Adder");
    w.tick(1.0);
    const auto* bo = w.ada.seen(w.bo1);
    expect(bo && bo->string("handle") == "Bobbin", "a sharing friend's handle under their label on Ada's map");
    const auto* toCy = w.cy.seen(w.bo1);
    expect(toCy && !toCy->has("handle"), "a stranger never sees a handle");
    const auto* adaToCy = w.cy.seen(w.adaId);
    expect(adaToCy && !adaToCy->has("handle"), "nor Ada's");
    // Bo stops sharing with Ada: his character leaves her list, and his handle leaves the map.
    w.friends(w.bo, {{"verb", "share"}, {"handle", "Adder"}, {"on", false}});
    w.tick(1.0);
    const auto* row = w.ada.friendRow("Bobbin");
    expect(row && row->boolean("online") && !row->has("character"), "not shared: online, but which wolf isn't said");
    bo = w.ada.seen(w.bo1);
    expect(bo && !bo->has("handle"), "and no handle under his label");
    expect(w.bo.friendRow("Adder") && !w.bo.friendRow("Adder")->boolean("shares"), "Bo's own switch shows off");
    w.friends(w.bo, {{"verb", "share"}, {"handle", "Adder"}, {"on", true}});
    w.tick(1.0);
    expect(w.ada.seen(w.bo1) && w.ada.seen(w.bo1)->string("handle") == "Bobbin", "shared again, it comes back");
    // Leaving and coming back: online changes reach the friend, with a toast on arrival.
    w.leave(w.bo);
    row = w.ada.friendRow("Bobbin");
    expect(row && !row->boolean("online") && !row->has("character"), "gone: shown away");
    w.enter(w.bo, w.bo2);
    row = w.ada.friendRow("Bobbin");
    expect(row && row->boolean("online") && row->string("character") == "Bo Two", "back on his other wolf, shared");
    expect(w.ada.last("friends")->string("toast") == "Bobbin is here.", "Ada is told he's here");
    w.send(w.ada, "profile", {{"verb", "settings"}, {"settings", parsed(R"({"toasts": false})")}});
    w.leave(w.bo);
    w.enter(w.bo, w.bo1);
    expect(!w.ada.last("friends")->has("toast"), "with toasts off, no toast");
}

void privateMessages()
{
    World3 w(options());
    w.handles();
    w.befriend(w.ada, w.bo, "Bobbin", "Adder");
    // (Delivered by connection, not by place: a friend anywhere gets it, and the wolf beside them doesn't.)
    w.message(w.ada, "Bobbin", "Are you coming to the moot?");
    const auto* got = w.bo.message("Are you coming to the moot?");
    expect(got && got->string("speaker") == "Adder" && got->string("with") == "Adder", "it arrives, named by handle");
    expect(!w.cy.message("moot") && !w.cy.said("moot"), "and no one else reads it");
    bool echoed = false;
    for (const auto& e : w.ada.events)
        echoed |= e.string("channel") == "private" && e.boolean("outgoing") && e.string("with") == "Bobbin";
    expect(echoed, "Ada gets her own copy, to Bobbin");
    w.message(w.cy, "Bobbin", "psst");
    expect(!w.bo.message("psst") && w.cy.said("Private messages go to friends only."), "a stranger's never arrives");
    // Too long; and a friend who has turned private messages off.
    w.message(w.ada, "Bobbin", std::string(2001, 'a'));
    expect(w.ada.said("at most 2000 characters"), "2000 characters at most");
    w.send(w.bo, "profile", {{"verb", "settings"}, {"settings", parsed(R"({"messages": false})")}});
    w.message(w.ada, "Bobbin", "still there?");
    expect(!w.bo.message("still there?") && w.ada.said("Bobbin isn't taking private messages."), "turned off: refused");
    w.send(w.bo, "profile", {{"verb", "settings"}, {"settings", parsed(R"({"messages": true})")}});
    // A friend's mute: their messages don't arrive, and they aren't told.
    w.send(w.bo, "safety", {{"verb", "mute"}, {"target", w.adaId}});
    w.message(w.ada, "Bobbin", "hello hello");
    expect(!w.bo.message("hello hello") && !w.ada.said("muted"), "muted: not delivered, and Ada isn't told");
    w.send(w.bo, "safety", {{"verb", "unmute"}, {"target", w.adaId}});
    // A DM's silence stops them.
    w.befriend(w.cy, w.bo, "Bobbin", "Cypress");
    w.send(w.cy, "safety", {{"verb", "report"}, {"target", w.bo1}, {"category", "spam"}});
    w.g.decideReport(w.g.reportsKept().begin()->first, "uphold", "silence", 1, "dm");
    w.message(w.bo, "Adder", "can't you hear me");
    expect(!w.ada.message("can't you hear me") && w.bo.said("silenced"), "silenced: no private messages either");
}

void reportsAndBlocks()
{
    World3 w(options());
    w.handles();
    w.befriend(w.ada, w.bo, "Bobbin", "Adder");
    w.message(w.bo, "Adder", "you are the worst");
    const auto* line = w.ada.message("you are the worst");
    expect(line != nullptr, "delivered");
    // A report by the line's number: its author found on the server, the private message as evidence.
    w.send(w.ada, "safety", {{"verb", "report"}, {"line", line->number("sequence")}, {"category", "harassment"}, {"block", true}});
    expect(w.g.reportsKept().size() == 1, "reported");
    const auto& r = w.g.reportsKept().begin()->second;
    expect(r.reportedAccount == "bob" && !r.evidence.empty() && r.evidence[0].channel == "private" &&
               r.evidence[0].text == "you are the worst",
           "the private message is the evidence");
    expect(w.ada.last("safety")->array("marks").size() == 1 && w.ada.last("safety")->array("marks")[0].string("label") == "Bobbin",
           "blocked too, listed by handle (all she knew of him there)");
    expect(!w.g.areFriends(w.adaId, w.bo1) && !w.bo.friendRow("Adder"), "a block ends the friendship");
    w.message(w.bo, "Adder", "hey");
    expect(!w.ada.message("hey") && w.bo.said("Private messages go to friends only."), "and refuses messages");
    w.friends(w.bo, {{"verb", "request"}, {"handle", "Adder"}});
    expect(w.bo.said("No one by that handle is taking friend requests.") && !w.ada.asked("Bobbin"),
           "a blocked account's request gets an unknown handle's refusal");
    w.friends(w.ada, {{"verb", "request"}, {"handle", "Bobbin"}});
    expect(!w.bo.asked("Adder"), "nor can the blocker ask, until she unblocks");
}

void offlineAndKept()
{
    const std::string save = "/tmp/ratw-friends-" + std::to_string(::getpid()) + ".json";
    std::remove(save.c_str());
    {
        World3 w(options(save));
        w.handles();
        w.befriend(w.ada, w.bo, "Bobbin", "Adder");
        w.friends(w.cy, {{"verb", "request"}, {"handle", "Adder"}});
        w.friends(w.bo, {{"verb", "share"}, {"handle", "Adder"}, {"on", false}});
        w.leave(w.bo);
        w.message(w.ada, "Bobbin", "See you at dawn by the ford.");
        expect(w.ada.said("Bobbin is away; it will reach them when they are next here."), "away: kept, and Ada is told");
        expect(!w.bo.message("dawn"), "not delivered yet");
        w.enter(w.bo, w.bo2);
        const auto* got = w.bo.message("See you at dawn by the ford.");
        expect(got && got->boolean("kept") && got->string("speaker") == "Adder" && got->number("at") > 0,
               "delivered when he comes, marked as kept, with when it was sent");
        expect(w.bo.said("1 private message came while you were away"), "and he's told how many");
        w.leave(w.bo);
        w.leave(w.bo);
        // Fifty at most for an away friend.
        for (int i = 0; i < 50; ++i)
            w.message(w.ada, "Bobbin", "note " + std::to_string(i));
        w.message(w.ada, "Bobbin", "one too many");
        expect(w.ada.said("Bobbin's messages are full until they are next here."), "the 51st: refused");
        w.g.save();
    }
    {
        // Across a restart: the friendship, its sharing, the request waiting, and the 50 kept.
        World3 w(options(save), false);
        expect(w.g.areFriends(w.adaId, w.bo1), "friends after a restart");
        expect(w.ada.friendRow("Bobbin") && !w.ada.friendRow("Bobbin")->has("character"), "Bo's sharing off, kept");
        expect(w.ada.asked("Cypress"), "Cy's request kept");
        int kept = 0;
        for (const auto& e : w.bo.events)
            kept += e.string("channel") == "private" && e.boolean("kept");
        expect(kept == 50, "the 50 kept, delivered on coming: " + std::to_string(kept));
        w.leave(w.bo);
        w.message(w.ada, "Bobbin", "an old one");
        w.g.save();
    }
    {
        // Fourteen days on: an old message and an old request go unread.
        std::ifstream in(save);
        std::stringstream text;
        text << in.rdbuf();
        auto doc = parsed(text.str());
        auto& people = doc.object("people");
        expect(people.array("inbox").size() == 1 && people.array("requests").size() == 1, "one message and one request saved");
        auto inbox = people.array("inbox");
        auto requests = people.array("requests");
        json::Value oldInbox = json::Value::array(), oldRequests = json::Value::array();
        for (auto m : inbox)
        {
            m.set("at", m.number("at") - 15 * 86400);
            oldInbox.push(m);
        }
        for (auto q : requests)
        {
            q.set("at", q.number("at") - 15 * 86400);
            oldRequests.push(q);
        }
        auto edited = people;
        edited.set("inbox", oldInbox);
        edited.set("requests", oldRequests);
        doc.set("people", edited);
        std::ofstream(save) << json::dump(doc);
    }
    {
        World3 w(options(save), false);
        expect(!w.bo.message("an old one"), "a message past 14 days never arrives");
        expect(!w.ada.asked("Cypress"), "a request past 14 days is gone");
    }
    std::remove(save.c_str());
}
} // namespace

int main()
{
    try
    {
        requests();
        sharingAndHandles();
        privateMessages();
        reportsAndBlocks();
        offlineAndKept();
    }
    catch (const std::exception& e)
    {
        std::cerr << "friends_tests failed after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
    std::cout << "friends_tests passed (" << checks << " checks)\n";
    return 0;
}
