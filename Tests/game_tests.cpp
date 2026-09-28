// The portable game server (RatwGame.h), driven as clients drive it: the lobby, logins and accounts, commands,
// snapshots and events, NPC conversation, and a restart from the save.
#include "RatwGame.h"
#include "RatwMotionCore.h"

#include <cstdio>
#include <iostream>
#include <unistd.h>
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

struct Client final : game::Connection
{
    std::vector<json::Value> events, snapshots;
    std::size_t motions = 0;
    json::Value lastMotion;
    sections::Cache cache;
    bool local = true;
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
        const bool whole = sections::fill(v, cache);   // As a client puts back what it holds.
        expect(whole, "a snapshot can be filled from what the client holds");
        snapshots.push_back(v);
    }
    void motion(const json::Value& frame) override
    {
        ++motions;
        lastMotion = frame;
    }
    bool allowsLocalCredentials() const override { return local; }
    const json::Value* last(const std::string& type) const
    {
        for (auto it = events.rbegin(); it != events.rend(); ++it)
            if (it->string("type") == type)
                return &*it;
        return nullptr;
    }
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

void run(game::Game& g, Client& c, double seconds)
{
    for (double t = 0; t < seconds; t += .05)
    {
        g.tick(.05);
        if (!c.snapshots.empty())
            g.acknowledge(&c, c.snapshots.back().number("revision"), false);
    }
}

void aDevelopmentSession()
{
    game::Options o;
    o.devIdentity = true;
    o.devTools = true;
    game::Game g(o);
    std::string problem;
    expect(g.start(problem), "the demo world starts: " + problem);
    Client ash;
    ash.id = 1;
    g.connect(&ash);
    expect(ash.last("lobby") && ash.last("lobby")->string("stage") == "login", "a new client is shown the lobby");
    g.command(&ash, cmd({{"type", "hello"}, {"id", "ash"}, {"name", "Ash"}}));
    expect(ash.last("entered") && ash.entityId == "player-ash", "a development identity enters");
    expect(!ash.snapshots.empty(), "and gets a snapshot at once");
    const auto& first = ash.snapshots.back();
    expect(first["self"].string("id") == "player-ash" && first["cell"].array("rows").size() > 0 && !first.array("entities").empty(),
           "with itself, its cell and who is about");
    bool talkable = false;
    for (const auto& e : first.array("entities"))
        for (const auto& a : e.array("actions"))
            talkable |= a.asString() == "talk";
    expect(talkable, "NPCs it can talk to");
    // Walking: motion frames every tick, snapshots five times a second, most of them deltas.
    const auto startX = g.world().entity("player-ash")->position.x;
    g.command(&ash, cmd({{"type", "move"}, {"x", 1}, {"y", 0}}));
    run(g, ash, 1.5);
    expect(g.world().entity("player-ash")->position.x > startX + .5, "a move command moves them");
    expect(ash.motions >= 25 && ash.lastMotion.string("motionSession") == ash.motionSession, "motion frames every tick");
    expect(!motion::unpack(motion::pack(ash.lastMotion)).isNull(), "that travel in binary");
    expect(ash.snapshots.size() >= 6, "snapshots five times a second (" + std::to_string(ash.snapshots.size()) + ")");
    g.command(&ash, cmd({{"type", "stop"}}));
    // Chat: heard by the speaker; an NPC named in it answers (authored lines, with no Mind configured).
    const auto keeper = g.world().entity("npc_keeper");
    expect(keeper != nullptr, "the keeper is here");
    g.world().entity("player-ash")->cellId = keeper->cellId;
    g.world().entity("player-ash")->position = {keeper->position.x + 1, keeper->position.y};
    run(g, ash, .2);
    ash.events.clear();
    g.command(&ash, cmd({{"type", "chat"}, {"text", "\"Hello, " + keeper->name + ". Do you remember me?\""}, {"commandId", "c1"}}));
    expect(ash.last("chatAccepted") != nullptr, "the post is accepted");
    int roleplay = 0;
    for (const auto& e : ash.events)
        roleplay += e.string("type") == "roleplay";
    expect(roleplay >= 2, "Ash hears their own words and the keeper's answer:\n" + ash.said());
    g.command(&ash, cmd({{"type", "chat"}, {"text", "again"}, {"commandId", "c1"}}));
    expect(ash.events.back().string("type") == "chatAccepted", "a repeated command gets its first answer again");
    // Dev tools: the weather.
    g.command(&ash, cmd({{"type", "weather"}, {"value", "fog"}}));
    expect(ash.said().find("Weather updated.") != std::string::npos, "dev tools change the weather");
    g.disconnect(&ash);
    expect(g.world().entity("player-ash") == nullptr && g.characters().count("player-ash"), "leaving keeps the character, offline");
}

void accountsAndARestart()
{
    game::Options o;
    game::Game g(o);
    std::string problem;
    expect(g.start(problem), "starts: " + problem);
    Client wren;
    wren.id = 2;
    g.connect(&wren);
    g.command(&wren, cmd({{"type", "hello"}, {"id", "wren"}}));
    expect(wren.last("lobby") && !wren.last("lobby")->boolean("ok"), "development identities are refused unless enabled");
    g.command(&wren, cmd({{"type", "auth_register"}, {"username", "Wren"}, {"password", "a long enough password"}}));
    expect(wren.last("lobby")->boolean("ok") && wren.accountUsername == "wren", "an account registers");
    auto appearance = json::Value::object();
    appearance.add("species", "timber"); appearance.add("sex", "female"); appearance.add("stature", "average");
    appearance.add("pattern", "saddle"); appearance.add("baseColor", 3); appearance.add("gradientColor", 1);
    appearance.add("markingColor", 5); appearance.add("gradientAmount", .5); appearance.add("patternAmount", .5);
    g.command(&wren, cmd({{"type", "character_create"}, {"name", "Wren Reed"}, {"age", 24}, {"appearance", appearance}, {"commandId", "make-1"}}));
    const auto* lobby = wren.last("lobby");
    expect(lobby->boolean("ok") && lobby->array("characters").size() == 1, "a character is made: " + lobby->string("message"));
    const std::string id = lobby->array("characters")[0].string("id");
    g.command(&wren, cmd({{"type", "character_create"}, {"name", "Wren Reed"}, {"age", 24}, {"appearance", appearance}, {"commandId", "make-1"}}));
    expect(wren.last("lobby")->array("characters").size() == 1, "the same request again makes no second");
    g.command(&wren, cmd({{"type", "character_enter"}, {"id", id}}));
    expect(wren.entityId == id, "and enters the world");
    g.command(&wren, cmd({{"type", "chat"}, {"text", "/sit"}}));
    run(g, wren, 1);
    g.command(&wren, cmd({{"type", "character_leave"}}));
    // The remote peer: no passwords over an unencrypted line.
    Client far;
    far.id = 3;
    far.local = false;
    g.connect(&far);
    g.command(&far, cmd({{"type", "auth_login"}, {"username", "wren"}, {"password", "a long enough password"}}));
    expect(!far.last("lobby")->boolean("ok") && far.accountUsername.empty(), "remote peers may not sign in");
    g.disconnect(&far);
    g.disconnect(&wren);
    g.save();
    expect(g.characters().count(id) && g.storageReady(), "saved, with the character offline");
}

void aRestartFromAFile()
{
    const std::string path = "/tmp/ratw-game-test-" + std::to_string(::getpid()) + ".json";
    std::remove(path.c_str());
    std::string id;
    {
        game::Options o;
        o.savePath = path;
        game::Game g(o);
        std::string problem;
        expect(g.start(problem), "starts with a file save: " + problem);
        Client c;
        c.id = 4;
        g.connect(&c);
        g.command(&c, cmd({{"type", "auth_register"}, {"username", "moss"}, {"password", "another long password"}}));
        auto appearance = json::Value::object();
        appearance.add("species", "arctic"); appearance.add("sex", "male"); appearance.add("stature", "tall");
        appearance.add("pattern", "solid"); appearance.add("baseColor", 1); appearance.add("gradientColor", 1);
        appearance.add("markingColor", 1); appearance.add("gradientAmount", .2); appearance.add("patternAmount", .2);
        g.command(&c, cmd({{"type", "character_create"}, {"name", "Moss"}, {"age", 30}, {"appearance", appearance}, {"commandId", "m1"}}));
        id = c.last("lobby")->array("characters")[0].string("id");
        g.command(&c, cmd({{"type", "character_enter"}, {"id", id}}));
        g.world().entity(id)->position.x += 2;
        g.disconnect(&c);
    }
    game::Options o;
    o.savePath = path;
    game::Game again(o);
    std::string problem;
    expect(again.start(problem), "a second server starts from the file: " + problem);
    expect(again.characters().count(id), "the character is still there");
    Client c;
    c.id = 5;
    again.connect(&c);
    again.command(&c, cmd({{"type", "auth_login"}, {"username", "moss"}, {"password", "another long password"}}));
    expect(c.accountUsername == "moss", "the account signs in again, its password verified from the save");
    again.command(&c, cmd({{"type", "character_enter"}, {"id", id}}));
    expect(c.entityId == id, "and plays on");
    again.disconnect(&c);
    std::remove(path.c_str());
}
} // namespace

int main()
{
    try
    {
        aDevelopmentSession();
        accountsAndARestart();
        aRestartFromAFile();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Game tests passed: " << checks << " checks.\n";
    return 0;
}
