// Scenes' openness, joining and knocking through the game (Docs/Design/51-scenes-and-stars.md, Phase 3): a party's scene
// starts Private and nobody nearby sees it; opened, a wolf in earshot sees it as "2 wolves · open" (and, pointing at
// it, who is in it, by the names it knows) and joins with one click, and their next line counts; a Knock scene tells its wolves who knocks, and one let in counts at
// once; a member who blocked someone keeps them out of it, without saying who.
#include "RatwGame.h"
#include "battle_play.h"

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
    expect(near.size() == 1 && near[0].string("id") == sid && near[0].number("wolves") == 2 && near[0].string("openness") == "open",
           "Cy sees it: 2 wolves, open: " + json::dump(social(w.cy)));
    // Who is in it, for the window on hover (the user, 2026-10-07): each by name as Cy knows them, with a look to draw.
    const auto& who = near[0].array("who");
    expect(who.size() == 2 && who[0].object("appearance").has("species") && !who[0].string("name").empty(), "who is in it, to point at");
    bool namedRight = true;
    for (const auto& wolf : who)
        namedRight &= wolf.string("name") == (wolf.string("id") == w.adaId ? "Ada" : "Bo One");
    expect(namedRight, "named as Cy knows them");
    expect(social(w.ada).array("scenes")[0].string("openness") == "open", "its wolves see it open");
    w.send(w.cy, "social", {{"verb", "join"}, {"session", sid}});
    expect(w.cy.said("You join the scene"), "Cy joins");
    say(w, w.cy, "\"Mind if I sit with you? I came in out of the rain.\"");
    expect(w.g.ledger().sessions.at(sid).members.count(w.cyId), "her next line counts in it");
}

const json::Value* line(Client& c, const std::string& text);

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
    const auto* first = line(w.cy, "no sign of stopping");
    expect(first && first->object("scene").boolean("mine"), "and her own copy of it is her scene's (doc 51, Phase 4)");
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
// The last roleplay line a player received with this text in it.
const json::Value* line(Client& c, const std::string& text)
{
    for (auto it = c.events.rbegin(); it != c.events.rend(); ++it)
        if (it->string("type") == "roleplay" && it->string("text").find(text) != std::string::npos)
            return &*it;
    return nullptr;
}

// Doc 51, Phase 4: each line says its scene, as far as the listener may know it; open scenes show on the map.
void seeingScenes()
{
    World3 w(options());
    const auto sid = partyScene(w);
    // Private: its wolves see their own scene's lines; Cy's copy carries nothing.
    w.send(w.ada, "chat", {{"text", "\"Keep your voice down, the innkeeper is listening.\""}, {"channel", "party"}});
    w.tick(1.0);
    const auto* mine = line(w.bo, "innkeeper is listening");
    expect(mine && mine->object("scene").boolean("mine") && mine->object("scene").string("colour").rfind("#", 0) == 0,
           "a member's copy: their own scene, with its colour");
    const auto* theirs = line(w.cy, "innkeeper is listening");
    expect(theirs && !theirs->has("scene"), "a private scene's line to an onlooker: ordinary talk");
    w.tick(2.5);
    expect(social(w.cy).array("openNear").empty(), "a private scene isn't on the map");
    // Open: an onlooker's copy names the door and the place, never the scene's id.
    w.send(w.ada, "social", {{"verb", "openness"}, {"session", sid}, {"value", "open"}});
    ::usleep(2100000);
    w.send(w.ada, "chat", {{"text", "\"Come and sit, anyone who likes; the fire is warm.\""}, {"channel", "party"}});
    w.tick(1.0);
    const auto* open = line(w.cy, "the fire is warm");
    expect(open && open->object("scene").string("openness") == "open" && !open->object("scene").string("place").empty() &&
               !open->object("scene").boolean("mine") && !open->object("scene").has("id"),
           "an open scene's line to an onlooker: open, and where");
    expect(open->object("scene").string("colour") == line(w.bo, "the fire is warm")->object("scene").string("colour"),
           "the same colour for everyone");
    w.tick(2.5);
    const auto& map = social(w.cy).array("openNear");
    expect(map.size() == 1 && map[0].number("wolves") == 2 && map[0].string("openness") == "open" && !map[0].has("who"),
           "an open scene on Cy's map: how many wolves, not who: " + json::dump(social(w.cy).array("openNear")));
    expect(social(w.ada).array("openNear").empty(), "never one's own scene");
    // Knock: on the map only for one with a friend in it.
    w.g.ledger().sessions.at(sid).openness = "knock";
    w.tick(2.5);
    expect(social(w.cy).array("openNear").empty(), "a knock scene with no friend in it isn't on Cy's map");
    w.handles();
    w.friends(w.cy, {{"verb", "request"}, {"handle", "Adder"}});
    w.friends(w.ada, {{"verb", "accept"}, {"handle", "Cypress"}});
    w.tick(2.5);
    const auto& withFriend = social(w.cy).array("openNear");
    expect(withFriend.size() == 1 && withFriend[0].string("openness") == "knock" && withFriend[0].boolean("friend"),
           "with her friend Ada in it, it is");
}
// Doc 51, Phase 5: a fight that breaks out in a scene is part of it (the user, 2026-10-07); the scene's card says what
// happened, in the names each viewer knows.
void endCards()
{
    auto o = options();
    o.hiddenNames = true;
    World3 w(o);
    const auto sid = partyScene(w);
    w.send(w.ada, "social", {{"verb", "openness"}, {"session", sid}, {"value", "open"}});
    w.send(w.cy, "social", {{"verb", "join"}, {"session", sid}});
    const auto line = [&](Client& c, const std::string& text, const char* channel = "") {
        ::usleep(2100000);
        w.send(c, "chat", {{"text", text}, {"channel", channel}});
        w.tick(1.0);
    };
    line(w.cy, "\"Mind if I sit with you both? I came in out of the rain, and the fire looks warmer than the road I left behind.\"");
    line(w.ada, "\"Sit, and welcome; we were only talking about the ford, and how the river took the lower bridge in the night, so that everyone coming from the south must now go the long way round by the mill.\"", "party");
    line(w.cy, "\"I'm Cy. I crossed at the old mill myself, though the miller's grey dog followed me most of the way to the crossroads.\"");
    line(w.bo, "\"That dog follows everyone. It thinks the whole valley is its own, and nobody has ever had the heart to tell it otherwise.\"", "party");
    // A fight breaks out between Ada and Bo: it is part of their scene, which stays alive while it lasts.
    auto& world = w.g.world();
    expect(world.attack(w.adaId, w.bo1).ok && world.answerChallenge(w.bo1, true).ok, "Ada and Bo fight");
    w.tick(1.0);
    std::string fight;
    for (const auto& [id, sc] : w.g.ledger().sessions)
        if (SocialLedger::isFight(sc) && sc.ended == 0)
            fight = id;
    expect(!fight.empty() && w.g.ledger().sessions.at(fight).parent == sid, "the fight's scene belongs to the scene it broke out in");
    bool momentKept = false;
    for (const auto& m : w.g.ledger().sessions.at(sid).moments)
        momentKept |= m.kind == "fight" && m.actor == fight;
    expect(momentKept, "and is one of its moments");
    test::takeGround(world, w.adaId);              // (Past the positioning phase: doc 40.)
    for (int i = 0; i < 400 && !test::acting(world.battleOf(w.adaId), w.adaId); ++i)
        w.tick(.05);
    const auto offered = world.offerTruce(w.adaId);
    expect(offered.ok, "a truce offered: " + offered.message);
    const auto agreed = world.answerTruce(w.bo1, true);
    expect(agreed.ok, "and agreed: " + agreed.message);
    w.tick(1.0);
    expect(w.g.ledger().sessions.at(fight).ended > 0 && !w.g.ledger().sessions.at(fight).log.empty(),
           "the fight is over; its log, blow by blow, kept with its scene");
    expect(w.g.ledger().sessions.at(sid).ended == 0, "the scene it broke out in goes on");
    // Cy, who watched, may read it, in the names she knows.
    w.send(w.cy, "social", {{"verb", "fightlog"}, {"session", fight}});
    const auto* log = w.cy.last("fightLog");
    expect(log && log->string("session") == fight && !log->array("lines").empty(), "Cy reads the fight's log");
    // The scene ends: its card.
    w.send(w.ada, "action", {{"action", "session_end"}, {"target", ""}});
    w.tick(2.5);
    const auto& ended = social(w.ada).object("ended");
    expect(ended.string("id") == sid && !ended.string("place").empty() && ended.array("with").size() == 2,
           "Ada's card: where, and who was in it: " + json::dump(ended));
    std::string said;
    for (const auto& m : ended.array("moments"))
        said += m.string("text") + "\n";
    expect(said.find("Cy joined the scene.") != std::string::npos, "Cy joined, by the name Ada learnt:\n" + said);
    expect(said.find("Cy told you") != std::string::npos && said.find("their name.") != std::string::npos, "Cy told her name");
    expect(said.find("A fight broke out") != std::string::npos, "the fight, part of it");
    expect(said.find("You and Cy shared a scene for the first time.") != std::string::npos, "her first scene with Cy");
    bool fightLinked = false;
    for (const auto& m : ended.array("moments"))
        fightLinked |= m.string("kind") == "fight" && m.string("fight") == fight;
    expect(fightLinked, "with the fight's log to read from the card");
    expect(said.find("words") == std::string::npos && said.find("turns") == std::string::npos, "and no word or turn counts");
}
} // namespace

int main()
{
    try
    {
        joining();
        knocking();
        blocksKeepOut();
        seeingScenes();
        endCards();
    }
    catch (const std::exception& e)
    {
        std::cerr << "scene_doors_tests failed after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
    std::cout << "scene_doors_tests passed (" << checks << " checks)\n";
    return 0;
}
