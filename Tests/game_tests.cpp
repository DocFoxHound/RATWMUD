// The portable game server (RatwGame.h), driven as clients drive it: the lobby, logins and accounts, commands,
// snapshots and events, NPC conversation, and a restart from the save.
#include "RatwGame.h"
#include "RatwMotionCore.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <iterator>
#include <unistd.h>
#include <stdexcept>
#include <string>
#include <thread>

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

// The ambient director (Phase 10): two residents who are friends, standing by a player, talk; the player hears it,
// a line at a time, in the authored words when there is no NPC Mind.
void residentsTalkWhereAPlayerCanHear()
{
    game::Options o;
    o.devIdentity = true;
    game::Game g(o);
    std::string problem;
    expect(g.start(problem), "the demo world starts: " + problem);
    Client ash;
    ash.id = 1;
    g.connect(&ash);
    g.command(&ash, cmd({{"type", "hello"}, {"id", "ash"}, {"name", "Ash"}}));
    auto& w = g.world();
    // Wait for two residents standing still near each other (at their work, say), and stand Ash beside them.
    std::string a, b;
    for (int i = 0; i < 240 && a.empty(); ++i)
    {
        run(g, ash, .5);
        for (const auto& [x, ex] : w.entities())
            for (const auto& [y, ey] : w.entities())
                if (a.empty() && x < y && ex.npc && ey.npc && w.society().resident(x) && w.society().resident(y) &&
                    ex.cellId == ey.cellId && ex.path.empty() && ey.path.empty() &&
                    std::hypot(ex.position.x - ey.position.x, ex.position.y - ey.position.y) <= 2.5 &&
                    w.society().resident(x)->task != "sleep" && w.society().resident(y)->task != "sleep")
                    a = x, b = y;
    }
    expect(!a.empty(), "two residents stand together");
    auto* me = w.entity("player-ash");
    me->cellId = w.entity(a)->cellId;
    me->position = {w.entity(a)->position.x, w.entity(a)->position.y + 1};
    for (const auto& [x, y] : {std::pair{a, b}, std::pair{b, a}})
        w.bonds().change(x, y, {30, 20, 60, 0, 0}, w.calendarDays());
    ash.events.clear();
    bool heardBoth = false;
    for (int i = 0; i < 60 && !heardBoth; ++i)
    {
        run(g, ash, .5);
        bool fromA = false, fromB = false;
        for (const auto& e : ash.events)
            if (e.string("type") == "roleplay")
            {
                fromA |= e.string("speaker") == w.entity(a)->name;
                fromB |= e.string("speaker") == w.entity(b)->name;
            }
        heardBoth = fromA && fromB;
    }
    expect(heardBoth, "Ash hears both of them:\n" + ash.said());
    expect(w.bonds().find(a, b)->familiarity > 60, "and the talk brought them a little closer");
}

// Cheaper voices (doc 28): what the world can answer, the game answers itself, in the NPC's tone, with real
// prices; the rest goes on to a model (authored lines here, with none configured); and every line is in the ledger.
void theGameAnswersWhatItKnows()
{
    const std::string ledger = "/tmp/ratw-voices-" + std::to_string(::getpid()) + ".jsonl";
    std::remove(ledger.c_str());
    game::Options o;
    o.devIdentity = true;
    o.voiceData = RATW_SOURCE_DIR "/Data/Voice";
    o.voiceLog = ledger;
    game::Game g(o);
    std::string problem;
    expect(g.start(problem), "the demo world starts: " + problem);
    Client ash;
    ash.id = 1;
    g.connect(&ash);
    g.command(&ash, cmd({{"type", "hello"}, {"id", "ash"}, {"name", "Ash"}}));
    auto& w = g.world();
    const auto* keeper = w.entity("npc_keeper");
    expect(keeper && w.society().merchant("npc_keeper"), "Rowan keeps shop");
    w.entity("player-ash")->cellId = keeper->cellId;
    w.entity("player-ash")->position = {keeper->position.x + 1, keeper->position.y};
    run(g, ash, .2);
    const auto say = [&](const std::string& words, const char* id) {
        run(g, ash, 2);                             // A moment between posts, as the chat rules ask.
        ash.events.clear();
        g.command(&ash, cmd({{"type", "chat"}, {"text", "\"" + words + "\""}, {"commandId", id}}));
        run(g, ash, .3);
        std::string reply;
        for (const auto& e : ash.events)
            if (e.string("type") == "roleplay" && e.string("speaker") == "Rowan")
                reply = e.string("text");
        return reply;
    };
    const auto price = w.society().quote("player-ash", "npc_keeper", "meal", 1, true).unitPrice;
    const auto sells = say("Rowan, what do you sell?", "v1");
    expect(sells.find("A meal is " + std::to_string(price) + " pennies") != std::string::npos,
           "Asked what he sells, Rowan gives his real price: " + sells);
    const auto again = say("Rowan, what do you sell?", "v2");
    expect(again.find(std::to_string(price)) != std::string::npos, "and again, as he said: " + again + " / " + ash.said());
    expect(!say("Hello, Rowan!", "v3").empty(), "A greeting is answered");
    const auto open = say("Rowan, I need to tell you about the strange wolf I saw on the road last night.", "v4");
    expect(!open.empty(), "Anything else still gets an answer (from a model, or authored lines without one)");
    g.disconnect(&ash);
    std::ifstream in(ledger);
    std::map<std::string, int> routes;
    for (std::string line; std::getline(in, line);)
    {
        json::Value v;
        std::string error;
        expect(json::parse(line, v, error) && v.string("kind") == "dialogue" && v.string("npc") == "npc_keeper" &&
                   !v.has("text"),
               "Ledger lines say who and how, never what: " + line);
        ++routes[v.string("route")];
    }
    expect(routes["game"] == 3 && routes["written"] == 1, "Three answered by the game, one left to a model (here, written)");
    std::remove(ledger.c_str());
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
    g.settle();                                // Passwords are checked off the game thread (doc 31).
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

// Passwords are worked on off the game thread (doc 31): signing in costs the game almost nothing, the answer comes
// in a later tick, a second request waits for the first, and a client gone before its answer is simply forgotten.
void signingInNeverHoldsTheGame()
{
    game::Options o;
    game::Game g(o);
    std::string problem;
    expect(g.start(problem), "starts: " + problem);
    Client a;
    a.id = 40;
    g.connect(&a);
    g.command(&a, cmd({{"type", "auth_register"}, {"username", "heron"}, {"password", "a long enough password"}}));
    g.settle();
    expect(a.last("lobby")->boolean("ok"), "an account to sign in to");
    g.command(&a, cmd({{"type", "auth_logout"}}));
    const auto lobbies = [](const Client& c) {
        std::size_t n = 0;
        for (const auto& e : c.events)
            n += e.string("type") == "lobby";
        return n;
    };
    const auto before = lobbies(a);
    const auto start = std::chrono::steady_clock::now();
    g.command(&a, cmd({{"type", "auth_login"}, {"username", "heron"}, {"password", "a long enough password"}}));
    const double took = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    expect(took < 20, "the command returns at once, the password left to the hasher: " + std::to_string(took) + " ms");
    expect(lobbies(a) == before && a.accountUsername.empty(), "with no answer yet");
    g.command(&a, cmd({{"type", "auth_login"}, {"username", "heron"}, {"password", "a long enough password"}}));
    expect(lobbies(a) == before + 1 && !a.last("lobby")->boolean("ok"), "a second request waits for the first");
    g.settle();
    expect(a.accountUsername == "heron" && a.last("lobby")->boolean("ok"), "then the answer: signed in");
    Client b;
    b.id = 41;
    g.connect(&b);
    g.command(&b, cmd({{"type", "auth_login"}, {"username", "heron"}, {"password", "the wrong password!!"}}));
    g.disconnect(&b);
    g.settle();
    expect(b.accountUsername.empty(), "a client gone before its answer is forgotten");
    Client c;
    c.id = 42;
    g.connect(&c);
    g.command(&c, cmd({{"type", "auth_login"}, {"username", "heron"}, {"password", "the wrong password!!"}}));
    g.settle();
    expect(!c.last("lobby")->boolean("ok") && c.accountUsername.empty(), "a wrong password is still refused");
    g.disconnect(&a);
    g.disconnect(&c);
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
        g.settle();                                // Replies wait for the journal (doc 31, Phase 2).
        auto appearance = json::Value::object();
        appearance.add("species", "arctic"); appearance.add("sex", "male"); appearance.add("stature", "tall");
        appearance.add("pattern", "solid"); appearance.add("baseColor", 1); appearance.add("gradientColor", 1);
        appearance.add("markingColor", 1); appearance.add("gradientAmount", .2); appearance.add("patternAmount", .2);
        g.command(&c, cmd({{"type", "character_create"}, {"name", "Moss"}, {"age", 30}, {"appearance", appearance}, {"commandId", "m1"}}));
        g.settle();
        id = c.last("lobby")->array("characters")[0].string("id");
        g.command(&c, cmd({{"type", "character_enter"}, {"id", id}}));
        g.settle();
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
    again.settle();                                // Passwords are checked off the game thread (doc 31).
    expect(c.accountUsername == "moss", "the account signs in again, its password verified from the save");
    again.command(&c, cmd({{"type", "character_enter"}, {"id", id}}));
    expect(c.entityId == id, "and plays on");
    again.disconnect(&c);
    std::remove(path.c_str());
}

// A player session, not a development one: the sky, the clock, the lamps and the calendar are not theirs to change,
// and a pace that isn't a whole step from 0 to 10 is refused.
void playersCannotRuleTheSky()
{
    game::Options o;
    o.devIdentity = true;
    game::Game g(o);
    std::string problem;
    expect(g.start(problem), "starts without dev tools: " + problem);
    Client ash;
    ash.id = 6;
    g.connect(&ash);
    g.command(&ash, cmd({{"type", "hello"}, {"id", "ash"}, {"name", "Ash"}}));
    expect(ash.entityId == "player-ash", "a player enters");
    const auto cellId = g.world().entity("player-ash")->cellId;
    const auto before = g.world().save();
    const auto hour = g.world().environmentAt(cellId).hour;
    ash.events.clear();
    for (const auto& [type, value] : std::initializer_list<std::pair<const char*, const char*>>{
             {"weather", "fog"}, {"weather", "seasonal"}, {"time", "night"}, {"lighting", "unlit"}, {"calendar", "year"}})
        g.command(&ash, cmd({{"type", type}, {"value", value}}));
    int refused = 0;
    for (const auto& e : ash.events)
        refused += e.string("text") == "Environment controls are available only in development sessions.";
    expect(refused == 5, "each is refused as a development control:\n" + ash.said());
    const auto after = g.world().save();
    expect(after.weather == before.weather && after.seasonalWeather == before.seasonalWeather, "the weather is as it was");
    expect(after.clockOffsetHours == before.clockOffsetHours && g.world().environmentAt(cellId).hour == hour, "the hour is as it was");
    expect(after.calendarDays == before.calendarDays, "the calendar is as it was");
    const auto& lamps = g.world().cell(cellId)->lighting;
    const auto& was = before.lighting.at(cellId);
    expect(lamps.artificial == was.artificial && lamps.daylightAccess == was.daylightAccess && lamps.tone == was.tone,
           "the lamps are as they were");
    // Pace.
    g.command(&ash, cmd({{"type", "pace"}, {"pace", 3}}));
    expect(g.world().entity("player-ash")->pace == 3, "a whole step is taken");
    for (const auto& bad : {json::Value::object(), json::Value(-1), json::Value(11), json::Value(4.5), json::Value(true),
                            json::Value("4"), json::Value(1e100), json::Value()})
    {
        ash.events.clear();
        g.command(&ash, cmd({{"type", "pace"}, {"pace", bad}}));
        expect(g.world().entity("player-ash")->pace == 3 && ash.said().find("Pace must be a whole step") != std::string::npos,
               "a pace of " + json::dump(bad) + " is refused and the pace kept");
    }
    ash.events.clear();
    g.command(&ash, cmd({{"type", "pace"}}));
    expect(g.world().entity("player-ash")->pace == 3 && !ash.events.empty(), "no pace at all is refused too");
    g.disconnect(&ash);
}

// What another wolf is sent about you: how you look and what you are doing, never your body's numbers.
void othersSeeNoPrivateStats()
{
    game::Options o;
    o.devIdentity = true;
    game::Game g(o);
    std::string problem;
    expect(g.start(problem), "starts: " + problem);
    Client ash, wren;
    ash.id = 7;
    wren.id = 8;
    g.connect(&ash);
    g.connect(&wren);
    g.command(&ash, cmd({{"type", "hello"}, {"id", "ash"}, {"name", "Ash"}}));
    g.command(&wren, cmd({{"type", "hello"}, {"id", "wren"}, {"name", "Wren"}}));
    auto* a = g.world().entity("player-ash");
    auto* w = g.world().entity("player-wren");
    expect(a && w, "both are in the world");
    w->cellId = a->cellId;
    w->position = {a->position.x + 1.5, a->position.y};
    a->age = 37;
    a->stamina = 61.5;
    a->dexterity = 77.25;
    a->strength = 58.5;
    a->sneakSkill = 41;
    a->hearingSkill = 23;
    a->scentSkill = 19;
    run(g, wren, .5);
    expect(!wren.snapshots.empty(), "Wren has a view");
    const auto& view = wren.snapshots.back();
    const json::Value* seen = nullptr;
    for (const auto& e : view.array("entities"))
        if (e.string("id") == "player-ash")
            seen = &e;
    expect(seen != nullptr, "Wren sees Ash");
    for (const auto* key : {"stamina", "dexterity", "effectiveDexterity", "age", "strength", "wisdom", "sneakSkill",
                            "hearingSkill", "scentSkill", "smell", "noseHealth", "hearing", "vision", "pace", "exhausted",
                            "cash", "socialXp"})
        expect(!seen->has(key), std::string("Wren is not told Ash's ") + key);
    expect(seen->string("lifeStage") == "adult", "only a life stage, not an age");
    const auto& self = view["self"];
    expect(self.has("stamina") && self.has("age") && self.has("sneakSkill"), "Wren's own numbers are hers to see");
    g.disconnect(&wren);
    g.disconnect(&ash);
}

std::string readFile(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void writeFile(const std::string& path, const std::string& text)
{
    std::ofstream(path, std::ios::binary | std::ios::trunc) << text;
}

// A save that can't be used is kept as it is: the server refuses to start on it, or (when it may play on without
// saving) never writes over it.
void unreadableSavesAreKept()
{
    const std::string path = "/tmp/ratw-game-test-kept-" + std::to_string(::getpid()) + ".json";
    const std::pair<const char*, std::string> saves[] = {
        {"an empty file", ""},
        {"a corrupt file", "{\"schema\":1,\"accounts\":"},
        {"a schema it doesn't know", "{\"schema\":2,\"accounts\":{},\"players\":[],\"npcs\":[]}"},
    };
    for (const auto& [what, text] : saves)
    {
        writeFile(path, text);
        {
            game::Options o;
            o.savePath = path;
            game::Game g(o);
            g.log = [](const char*, const std::string&) {};
            std::string problem;
            expect(!g.start(problem) && !g.storageReady(), std::string("a server that needs its save won't start on ") + what);
        }
        expect(readFile(path) == text, std::string("and leaves ") + what + " as it was");
        game::Options o;
        o.savePath = path;
        o.requireStorage = false;
        game::Game g(o);
        g.log = [](const char*, const std::string&) {};
        std::string problem;
        expect(g.start(problem) && !g.storageReady(), std::string("one that may play on starts, not saving, on ") + what);
        Client c;
        c.id = 9;
        g.connect(&c);
        g.command(&c, cmd({{"type", "auth_register"}, {"username", "nobody"}, {"password", "a long enough password"}}));
        expect(!c.last("lobby")->boolean("ok") && c.accountUsername.empty(), std::string("no account is made over ") + what);
        for (int i = 0; i < 400; ++i)
            g.tick(.05);                               // Past the autosave.
        g.save();
        g.disconnect(&c);
        expect(readFile(path) == text, std::string("and ") + what + " is still as it was, byte for byte");
    }
    std::remove(path.c_str());
}

// Accounts and characters must agree: an account owning a character the save doesn't have, or a made character
// nobody owns, stops the load.
// Saves that never stop the game (Docs/Design/31-responsiveness.md, Phase 2). A valuable command's reply waits for its
// journal record; a crash after it loses nothing, the journal being replayed over the last checkpoint; a snapshot
// taken by a forked copy of the server writes the save while the game plays on, and the records it covers go.
void theJournalKeepsWhatACrashWouldLose()
{
    const std::string path = "/tmp/ratw-game-test-journal-" + std::to_string(::getpid()) + ".json";
    const auto clean = [&] {
        std::remove(path.c_str());
        std::remove((path + ".journal").c_str());
    };
    clean();
    std::string id;
    const auto herbs = [&id](game::Game& g) {
        const auto* account = g.world().society().account(id);
        return account ? Society::stock(*account, "herbs") : -1;
    };
    int gathered = 0;
    {
        game::Options o;
        o.savePath = path;
        o.devIdentity = true;
        game::Game g(o);
        std::string problem;
        expect(g.start(problem), "starts with a file save: " + problem);
        expect(!readFile(path).empty(), "a world never saved is saved at once, for the journal to build on");
        Client c;
        c.id = 30;
        g.connect(&c);
        g.command(&c, cmd({{"type", "hello"}, {"id", "ash"}, {"name", "Ash"}}));
        g.settle();
        id = c.entityId;
        auto* ash = g.world().entity(c.entityId);
        expect(ash != nullptr, "Ash is in the world");
        ash->cellId = g.world().herbPatchCell();
        ash->position = g.world().herbPatchPosition();
        const int before = herbs(g);
        const auto events = c.events.size();
        g.command(&c, cmd({{"type", "gather"}}));
        gathered = herbs(g);
        expect(gathered > before, "herbs are gathered at once: " + std::to_string(before) + " -> " + std::to_string(gathered) +
                                      " " + c.said());
        g.settle();
        expect(c.events.size() > events, "and the reply is sent once the journal has it");
        const auto journal = readFile(path + ".journal");
        expect(journal.find("herbs") != std::string::npos, "the journal holds what changed");
        expect(readFile(path).find("\"herbs\":" + std::to_string(gathered)) == std::string::npos,
               "the save itself does not yet");
        // A crash: the game goes without saving.
    }
    {
        game::Options o;
        o.savePath = path;
        o.devIdentity = true;
        game::Game g(o);
        std::string problem;
        expect(g.start(problem), "starts again after the crash: " + problem);
        expect(herbs(g) == gathered, "the herbs are still gathered: the journal replayed over the checkpoint");
        // A snapshot from a forked copy: the save is written while the game goes on, and the journal is trimmed.
        Client c;
        c.id = 31;
        g.connect(&c);
        g.command(&c, cmd({{"type", "hello"}, {"id", "ash"}, {"name", "Ash"}}));
        g.settle();
        const auto saveBefore = readFile(path);
        bool written = false;
        for (int i = 0; i < 400 && !written; ++i)
        {
            g.tick(.05);
            if (i > 100)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            written = readFile(path) != saveBefore && readFile(path + ".journal").find("herbs") == std::string::npos;
        }
        expect(written, "a forked snapshot replaced the save and the journal records it covers went");
        expect(readFile(path).find("\"journal\"") != std::string::npos, "the save says how far into the journal it reaches");
        g.disconnect(&c);
    }
    {
        game::Options o;
        o.savePath = path;
        game::Game g(o);
        std::string problem;
        expect(g.start(problem) && herbs(g) == gathered, "and the snapshot alone brings it back: " + problem);
    }
    clean();
}

void mismatchedOwnersAreRefused()
{
    const std::string path = "/tmp/ratw-game-test-owners-" + std::to_string(::getpid()) + ".json";
    std::remove(path.c_str());
    {
        game::Options o;
        o.savePath = path;
        game::Game g(o);
        std::string problem;
        expect(g.start(problem), "starts with a file save: " + problem);
        Client c;
        c.id = 10;
        g.connect(&c);
        g.command(&c, cmd({{"type", "auth_register"}, {"username", "fern"}, {"password", "a long enough password"}}));
        g.settle();                                // Passwords are checked off the game thread (doc 31).
        auto appearance = json::Value::object();
        appearance.add("species", "red"); appearance.add("sex", "female"); appearance.add("stature", "short");
        appearance.add("pattern", "mantle"); appearance.add("baseColor", 2); appearance.add("gradientColor", 3);
        appearance.add("markingColor", 4); appearance.add("gradientAmount", .3); appearance.add("patternAmount", .6);
        g.command(&c, cmd({{"type", "character_create"}, {"name", "Fern"}, {"age", 20}, {"appearance", appearance}, {"commandId", "f1"}}));
        g.settle();                                // Replies wait for the journal (doc 31, Phase 2).
        expect(c.last("lobby")->array("characters").size() == 1, "a character is made");
        g.disconnect(&c);
        g.save();                                  // As the server does when it stops: the file then holds it all.
    }
    json::Value good;
    std::string error;
    expect(json::parse(readFile(path), good, error), "the save reads: " + error);
    expect(good.array("players").size() == 1, "with its one character");
    {
        game::Options o;
        o.savePath = path;
        game::Game g(o);
        std::string problem;
        expect(g.start(problem) && g.storageReady(), "the save as it was loads: " + problem);
    }
    auto orphan = good.array("players")[0];
    orphan.set("id", "wolf-00000000000000000000000000000abc");
    auto withoutCharacter = good, withStranger = good;
    withoutCharacter.set("players", json::Value::array());
    withStranger.find("players")->push(orphan);
    for (const auto& [what, document] : {std::pair<const char*, json::Value>{"an account's character missing", withoutCharacter},
                                         std::pair<const char*, json::Value>{"a character nobody owns", withStranger}})
    {
        const auto text = json::dump(document);
        writeFile(path, text);
        game::Options o;
        o.savePath = path;
        game::Game g(o);
        g.log = [](const char*, const std::string&) {};
        std::string problem;
        expect(!g.start(problem) && !g.storageReady(), std::string("a save with ") + what + " is refused");
        g.save();
        expect(readFile(path) == text, std::string("and kept as it was (") + what + ")");
    }
    std::remove(path.c_str());
}
} // namespace

// With the written scenes (doc 30): residents talk from them, a scene heard is remembered for the character, and the
// memory survives a save.
void residentsTalkFromWrittenScenes()
{
    game::Options o;
    o.devIdentity = true;
    o.voiceData = std::string(RATW_SOURCE_DIR) + "/Data/Voice";
    o.savePath = "/tmp/ratw-game-test-scenes-" + std::to_string(::getpid()) + ".json";
    std::remove(o.savePath.c_str());
    std::string problem;
    {
        game::Game g(o);
        expect(g.start(problem), "the demo world starts: " + problem);
        expect(g.sceneLibrarySize() >= 100, "the scenes are loaded (" + std::to_string(g.sceneLibrarySize()) + ")");
        Client ash;
        ash.id = 1;
        g.connect(&ash);
        g.command(&ash, cmd({{"type", "hello"}, {"id", "ash"}, {"name", "Ash"}}));
        auto& w = g.world();
        std::string a, b;
        for (int i = 0; i < 240 && a.empty(); ++i)
        {
            run(g, ash, .5);
            for (const auto& [x, ex] : w.entities())
                for (const auto& [y, ey] : w.entities())
                    if (a.empty() && x < y && ex.npc && ey.npc && w.society().resident(x) && w.society().resident(y) &&
                        ex.cellId == ey.cellId && ex.path.empty() && ey.path.empty() &&
                        std::hypot(ex.position.x - ey.position.x, ex.position.y - ey.position.y) <= 2.5 &&
                        w.society().resident(x)->task != "sleep" && w.society().resident(y)->task != "sleep")
                        a = x, b = y;
        }
        expect(!a.empty(), "two residents stand together");
        auto* me = w.entity("player-ash");
        me->cellId = w.entity(a)->cellId;
        me->position = {w.entity(a)->position.x, w.entity(a)->position.y + 1};
        ash.events.clear();
        for (int i = 0; i < 80 && g.scenesHeardBy("player-ash") == 0; ++i)
            run(g, ash, .5);
        expect(g.scenesHeardBy("player-ash") >= 1, "a written scene is spoken, even between two who barely know each other:\n" + ash.said());
        int lines = 0;
        for (const auto& e : ash.events)
            lines += e.string("type") == "roleplay";
        expect(lines >= 1, "and heard");
        g.disconnect(&ash);
        g.save();
    }
    game::Game again(o);
    expect(again.start(problem), "restarts: " + problem);
    expect(again.scenesHeardBy("player-ash") >= 1, "what Ash has heard is remembered across a restart");
    std::remove(o.savePath.c_str());
}

// Uploaded portraits (Docs/Design/29-client-polish.md, phase 9): sent in parts as raw pixels, encoded by the server,
// seen by their owner at once and by others only once approved; a report hides it again.
std::string base64(const std::string& in)
{
    static const char* d = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (std::size_t i = 0; i < in.size(); i += 3)
    {
        const unsigned n = (unsigned(std::uint8_t(in[i])) << 16) | (i + 1 < in.size() ? unsigned(std::uint8_t(in[i + 1])) << 8 : 0u) |
                           (i + 2 < in.size() ? unsigned(std::uint8_t(in[i + 2])) : 0u);
        out += d[(n >> 18) & 63];
        out += d[(n >> 12) & 63];
        out += i + 1 < in.size() ? d[(n >> 6) & 63] : '=';
        out += i + 2 < in.size() ? d[n & 63] : '=';
    }
    return out;
}

void uploadedPortraits()
{
    game::Options o;
    o.devIdentity = true;
    game::Game g(o);
    std::string problem;
    expect(g.start(problem), "the demo world starts: " + problem);
    Client ash, wren;
    ash.id = 1;
    wren.id = 2;
    g.connect(&ash);
    g.connect(&wren);
    g.command(&ash, cmd({{"type", "hello"}, {"id", "ash"}, {"name", "Ash"}}));
    g.command(&wren, cmd({{"type", "hello"}, {"id", "wren"}, {"name", "Wren"}}));
    auto* w = g.world().entity("player-wren");
    const auto* a = g.world().entity("player-ash");
    w->cellId = a->cellId;
    w->position = {a->position.x + 1, a->position.y};
    std::string pixels(std::size_t(256) * 256 * 4, '\0');
    for (std::size_t i = 0; i < pixels.size(); ++i)
        pixels[i] = char(i % 4 == 3 ? 255 : (i * 7) % 251);
    const auto upload = [&](Client& who, const std::string& character, const std::string& data, int parts) {
        const std::size_t size = (data.size() + parts - 1) / parts;
        for (int part = 0; part < parts; ++part)
            g.command(&who, cmd({{"type", "artwork_upload"}, {"characterId", character}, {"uploadId", "u1"}, {"part", part}, {"parts", parts},
                                 {"data", base64(data.substr(std::size_t(part) * size, size))}}));
    };
    upload(wren, "player-ash", pixels, 8);
    expect(wren.last("artworkError") != nullptr, "nobody uploads a portrait for another's character");
    upload(ash, "player-ash", pixels.substr(0, 1000), 1);
    expect(ash.last("artworkError") && ash.last("artworkError")->string("text").find("256") != std::string::npos, "a portrait is 256 pixels square");
    g.command(&ash, cmd({{"type", "artwork_upload"}, {"characterId", "player-ash"}, {"uploadId", "u2"}, {"part", 1}, {"parts", 8},
                         {"data", base64(pixels.substr(0, 1000))}}));
    expect(ash.last("artworkError") && ash.last("artworkError")->string("text").find("first part") != std::string::npos,
           "an upload starts from its first part");
    upload(ash, "player-ash", pixels, 8);
    const auto* first = ash.last("artworkUploaded");
    expect(first && first->string("status") == "pending", "eight parts make a portrait, waiting for approval");
    const std::string replaced = first->string("id");
    upload(ash, "player-ash", pixels, 8);
    const auto* done = ash.last("artworkUploaded");
    expect(done && done->string("id") != replaced && g.artworks().at(replaced).status == "rejected",
           "a newer upload replaces one still waiting");
    const std::string id = done->string("id");
    run(g, ash, .5);
    run(g, wren, .5);
    expect(ash.snapshots.back()["self"].string("artwork") == id, "its owner sees it at once");
    bool hidden = true;
    for (const auto& e : wren.snapshots.back().array("entities"))
        if (e.string("id") == "player-ash")
            hidden = !e.has("artwork");
    expect(hidden, "others don't, until it's approved");
    g.command(&wren, cmd({{"type", "artwork_get"}, {"id", id}}));
    expect(wren.last("artwork") && wren.last("artwork")->boolean("denied"), "nor can they fetch it");
    g.command(&ash, cmd({{"type", "artwork_get"}, {"id", id}}));
    expect(ash.last("artwork") && ash.last("artwork")->string("png").rfind("iVBORw0KGgo", 0) == 0, "the owner gets a PNG, made by the server");
    expect(g.reviewArtwork(id, "approve", "").ok, "a DM approves it");
    run(g, wren, .5);
    bool shown = false;
    for (const auto& e : wren.snapshots.back().array("entities"))
        if (e.string("id") == "player-ash")
            shown = e.string("artwork") == id;
    expect(shown, "and now everyone sees it");
    g.command(&wren, cmd({{"type", "artwork_report"}, {"id", id}, {"reason", "not a wolf"}}));
    expect(wren.last("artworkReported") && g.artworks().at(id).status == "pending" && g.artworks().at(id).reported,
           "a report sends it back to the DM, hidden again");
    expect(!g.reviewArtwork(id, "maybe", "").ok && !g.reviewArtwork("art-nothing", "approve", "").ok, "only real decisions on real portraits");
    for (int i = 0; i < 10; ++i)
        upload(ash, "player-ash", pixels, 8);
    g.command(&ash, cmd({{"type", "artwork_upload"}, {"characterId", "player-ash"}, {"uploadId", "u3"}, {"part", 0}, {"parts", 8},
                         {"data", base64(pixels.substr(0, 1000))}}));
    expect(g.artworks().size() == 10 && ash.last("artworkError")->string("text").find("one day") != std::string::npos,
           "ten uploads a day, no more");
    g.disconnect(&ash);
    g.disconnect(&wren);
}

// Talk targets (Docs/Design/29-client-polish.md, phase 4): only those spoken to answer, in turn, and the player is told
// when a chosen wolf can't hear.
void talkTargets()
{
    game::Options o;
    o.devIdentity = true;
    game::Game g(o);
    std::string problem;
    expect(g.start(problem), "the demo world starts: " + problem);
    Client ash;
    ash.id = 1;
    g.connect(&ash);
    g.command(&ash, cmd({{"type", "hello"}, {"id", "ash"}, {"name", "Ash"}}));
    // Two residents in one place, the player between them.
    std::vector<std::string> pair;
    std::string cell;
    for (const auto& [id, e] : g.world().entities())
        if (e.npc && !e.transient && !e.dead && (cell.empty() || e.cellId == cell) && pair.size() < 2)
        {
            cell = e.cellId;
            pair.push_back(id);
        }
    expect(pair.size() == 2, "two residents share a place in the demo world");
    auto* a = g.world().entity(pair[0]);
    auto* b = g.world().entity(pair[1]);
    for (auto* e : {a, b})
    {
        g.world().stop(e->id);
        e->cellId = cell;
    }
    a->position = {6, 6};
    b->position = {8, 6};
    auto* me = g.world().entity("player-ash");
    me->cellId = cell;
    me->position = {7, 6};
    run(g, ash, .2);
    const auto spoke = [&](const std::string& name) {
        int n = 0;
        for (const auto& e : ash.events)
            n += e.string("type") == "roleplay" && e.string("speaker") == name;
        return n;
    };
    const auto chat = [&](const std::string& text, std::vector<std::string> targets, const std::string& commandId) {
        auto list = json::Value::array();
        for (const auto& t : targets)
            list.push(t);
        ash.events.clear();
        g.command(&ash, cmd({{"type", "chat"}, {"text", "\"" + text + "\""}, {"targets", list}, {"commandId", commandId}}));
        run(g, ash, 1.2);
    };
    chat("Good day to you.", {a->id}, "t1");
    expect(spoke(a->name) == 1 && spoke(b->name) == 0, "only the chosen wolf answers:\n" + ash.said());
    const json::Value* mine = nullptr;
    for (const auto& e : ash.events)
        if (e.string("type") == "roleplay" && e.string("speaker") == "Ash")
            mine = &e;
    expect(mine && mine->array("to").size() == 1 && mine->array("to")[0].asString() == a->name, "the player's words say whom they were for");
    chat("And what do you both make of it?", {a->id, b->id}, "t2");
    expect(spoke(a->name) == 1 && spoke(b->name) == 1, "two chosen wolves both answer:\n" + ash.said());
    std::size_t first = 0, second = 0;
    for (std::size_t i = 0; i < ash.events.size(); ++i)
        if (ash.events[i].string("type") == "roleplay")
        {
            if (ash.events[i].string("speaker") == a->name)
                first = i;
            if (ash.events[i].string("speaker") == b->name)
                second = i;
        }
    expect(first < second, "one after the other, in the order chosen");
    // Naming someone still reaches them, alongside the chosen.
    chat("Tell me, " + b->name + ", is it always this quiet?", {a->id}, "t3");
    expect(spoke(a->name) == 1 && spoke(b->name) == 1, "a wolf named answers too");
    // A chosen wolf too far away to hear: the player is told.
    a->position = {6 + 200, 6};
    if (const auto* c = g.world().cell(cell); c && c->width > 210)
    {
        chat("Hello?", {a->id}, "t4");
        expect(spoke(a->name) == 0 && ash.said().find("too far away to hear you") != std::string::npos, "out of earshot, and told so");
    }
    a->position = {6, 6};
    run(g, ash, .2);
    // Choosing someone says nothing for the player: the client adds them to its talk targets.
    ash.events.clear();
    g.command(&ash, cmd({{"type", "action"}, {"action", "talk"}, {"target", a->id}}));
    expect(ash.last("talkTarget") && ash.last("talkTarget")->string("id") == a->id, "talk chooses a target");
    expect(spoke("Ash") == 0, "without saying anything");
    g.disconnect(&ash);
}

int main()
{
    try
    {
        aDevelopmentSession();
        residentsTalkWhereAPlayerCanHear();
        talkTargets();
        residentsTalkFromWrittenScenes();
        uploadedPortraits();
        theGameAnswersWhatItKnows();
        accountsAndARestart();
        signingInNeverHoldsTheGame();
        aRestartFromAFile();
        playersCannotRuleTheSky();
        othersSeeNoPrivateStats();
        unreadableSavesAreKept();
        mismatchedOwnersAreRefused();
        theJournalKeepsWhatACrashWouldLose();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Game tests passed: " << checks << " checks.\n";
    return 0;
}
