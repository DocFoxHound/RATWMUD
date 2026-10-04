// Names and introductions (Core/RatwNames.h; Docs/Design/32-parties-chapters-factions.md, 1.5): the rules alone, then
// through the game as clients drive it: strangers known by their look, introductions heard in speech, aliases, a
// resident giving their name (or keeping it), the game's messages veiled, and a restart.
#include "RatwGame.h"
#include "RatwNames.h"

#include <algorithm>
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

void hearingAnIntroduction()
{
    const std::vector<std::string> ash{"Ash", "Kestrel", "Old Tam"};
    const auto said = [&](const std::string& text) { return names::introducedName(text, ash); };
    expect(said("\"I'm Ash.\"") == "Ash", "I'm Ash");
    expect(said("Well met. I am Ash, of the north road.") == "Ash", "I am Ash");
    expect(said("My name is Kestrel") == "Kestrel", "my name is Kestrel");
    expect(said("You can call me Kestrel, if you like.") == "Kestrel", "you can call me Kestrel");
    expect(said("They call me Old Tam.") == "Old Tam", "a name of two words");
    expect(said("I\xe2\x80\x99m Ash") == "Ash", "a curly apostrophe");
    expect(said("ash, at your service") == "Ash", "a name opening the line, at your service");
    expect(said("Ash, of the Ashen Lodge.") == "Ash", "a name opening the line, of somewhere");
    expect(said("I'm tired.").empty(), "\"I'm tired\" introduces nobody");
    expect(said("Have you seen Ash?").empty(), "naming someone isn't introducing oneself");
    expect(said("Ash is here, I'm sure.").empty(), "nor is talking about oneself");
    expect(said("I am ashamed.").empty(), "a whole word only");
    expect(names::introducedName("I'm Wren.", {"Ash"}).empty(), "only one's own names count");
}

void aliasesAndLooks()
{
    expect(names::aliasProblem("Kestrel", "Ash", {}).empty(), "an alias");
    expect(names::aliasProblem("Old Tam", "Ash", {}).empty(), "of two words");
    expect(!names::aliasProblem("ash", "Ash", {}).empty(), "not one's own name");
    expect(!names::aliasProblem("Kestrel", "Ash", {"kestrel"}).empty(), "nor one already taken");
    expect(!names::aliasProblem("K", "Ash", {}).empty() && !names::aliasProblem("Kes7rel", "Ash", {}).empty() &&
               !names::aliasProblem("-Kes", "Ash", {}).empty(),
           "letters only, 2 to 24");
    expect(!names::aliasProblem("Wren", "Ash", {"A", "B", "C"}).empty(), "at most three");
    Appearance a;
    a.stature = "tall";
    a.coat = "#a0663f";
    a.markings.push_back({"socks", "#f4f2ec", 1});
    expect(names::describe(a, 30) == "a tall russet wolf with white socks", "a look: " + names::describe(a, 30));
    Appearance b;
    b.baseColor = 0;
    b.build = "heavy";
    expect(names::describe(b, 70) == "a heavy old cream wolf", "another: " + names::describe(b, 70));
    b.build = "";
    expect(names::describe(b, 70) == "an old cream wolf", "an, before a vowel: " + names::describe(b, 70));
    b.markings.push_back({"blaze", "#2a2b2c", .9});
    expect(names::describe(b, 9) == "a young cream wolf with a black blaze", "young, a blaze: " + names::describe(b, 9));
    const std::map<std::string, std::string> labels{{"Rowan", "the innkeeper"}, {"Ash", "a grey wolf"}};
    expect(names::veil("Rowan comes at you. You go for Ash.", labels) == "The innkeeper comes at you. You go for a grey wolf.",
           "names veiled, capitalised at the start of a sentence");
    expect(names::veil("Ash's purse is light; Ash Hollow is near.", labels) == "A grey wolf's purse is light; Ash Hollow is near.",
           "a possessive is still them; a longer proper name is left alone");
    expect(names::veil("Rowanberry", labels) == "Rowanberry", "whole words only");
    // Residents by their post: a trade's name, or what they do (playtest, October 4: "a carries loads for hire").
    expect(names::byTrade("innkeeper", false) == "the innkeeper" && names::byTrade("guard", true) == "a guard", "a trade");
    expect(names::byTrade("carries loads for hire", true) == "a wolf who carries loads for hire", "what they do");
    expect(names::byTrade("playing in the plaza", true) == "a wolf playing in the plaza", "what they're doing");
    expect(names::byTrade("on patrol", true) == "a wolf on patrol" && names::byTrade("holding the main gate", false) ==
               "the wolf holding the main gate", "where they are, what they hold");
    expect(names::byTrade("glass blower", false) == "the glass blower" && names::byTrade("lord's steward", false) == "the lord's steward",
           "a trade that only looks like a verb");
    // A written kind of wolf in the coat's own colour.
    Appearance sandy;
    sandy.baseColor = 7;
    expect(names::fitCoat("A small, quick grey timber wolf with a scar.", sandy) == "A small, quick sandy timber wolf with a scar.",
           "the coat wins: " + names::fitCoat("A small, quick grey timber wolf with a scar.", sandy));
    expect(names::fitCoat("A charcoal wolf in a harness.", sandy) == "A charcoal wolf in a harness.", "no kind written: left alone");
}

void whoKnowsWhom()
{
    names::Acquaintances k;
    expect(k.learn("ada", "bo", "Kestrel", "introduced", 3), "Ada learns Bo's name");
    expect(!k.learn("ada", "bo", "Kestrel", "introduced", 4), "the same again is nothing new");
    expect(k.learn("ada", "bo", "Bo", "introduced", 5) && k.nameFor("ada", "bo") == "Bo", "a second name: the newest is used");
    expect(k.find("ada", "bo")->names.size() == 2 && k.find("ada", "bo")->day == 3, "both are kept, and when they met");
    expect(!k.knows("bo", "ada"), "it is one-sided");
    k.learn("cy", "bo", "Kestrel", "introduced", 6);
    names::Acquaintances l;
    l.load(parsed(json::dump(k.save())));
    expect(l.nameFor("ada", "bo") == "Bo" && l.nameFor("cy", "bo") == "Kestrel" && l.size() == 2, "saved and read");
    l.forget("bo");
    expect(!l.knows("ada", "bo") && l.size() == 0, "forgotten when gone");
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
        expect(sections::fill(v, cache), "a snapshot can be filled from what the client holds");
        snapshots.push_back(v);
    }
    void motion(const json::Value&) override {}
    bool allowsLocalCredentials() const override { return true; }
    std::string said() const
    {
        std::string all;
        for (const auto& e : events)
            all += e.string("speaker") + ": " + e.string("text") + "\n";
        return all;
    }
    const json::Value* sees(const std::string& id) const
    {
        for (const auto& e : snapshots.back().array("entities"))
            if (e.string("id") == id)
                return &e;
        return nullptr;
    }
    std::string calls(const std::string& id) const { return sees(id) ? sees(id)->string("name") : std::string("(not seen)"); }
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
            g.world().entity(c->entityId)->cellId = a->cellId;
        place(bo, 1.2, 0);
        place(cy, -1.5, 0);
        tick(.5);
    }
    void place(Client& c, double dx, double dy)
    {
        const auto* a = g.world().entity(ada.entityId);
        g.world().entity(c.entityId)->position = {a->position.x + dx, a->position.y + dy};
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
    void say(Client& c, const std::string& text, const std::vector<std::string>& targets = {})
    {
        auto o = json::Value::object();
        o.add("type", "chat");
        o.add("text", text);
        auto list = json::Value::array();
        for (const auto& t : targets)
            list.push(t);
        o.add("targets", list);
        g.command(&c, json::dump(o));
        tick(.6);                                  // (A moment between posts.)
    }
};

game::Options options()
{
    game::Options o;
    o.devIdentity = true;
    o.voiceData = std::string(RATW_SOURCE_DIR) + "/Data/Voice";
    return o;
}

void strangersAndIntroductions()
{
    Three t(options());
    const auto& bo = *t.g.world().entity(t.bo.entityId);
    const auto look = names::capitalised(names::describe(bo.appearance, bo.age));
    expect(t.ada.calls(t.bo.entityId) == look || t.ada.calls(t.bo.entityId).rfind(look, 0) == 0,
           "Ada sees Bo as he looks: " + t.ada.calls(t.bo.entityId));
    expect(t.ada.sees(t.bo.entityId)->boolean("known", true) == false, "and is told she doesn't know him");
    expect(t.ada.offered(t.bo.entityId, "introduce"), "she can introduce herself to him");
    // Bo speaks before he introduces himself: heard, but not by name.
    t.say(t.bo, "\"Fine weather for it.\"");
    bool named = false;
    for (const auto& e : t.ada.events)
        named |= e.string("type") == "roleplay" && e.string("speaker") == "Bo";
    expect(!named, "his words aren't put to his name:\n" + t.ada.said());
    // He introduces himself.
    t.ada.events.clear();
    t.bo.events.clear();
    t.say(t.bo, "\"Well met. I'm Bo.\"");
    expect(t.ada.calls(t.bo.entityId) == "Bo" && !t.ada.sees(t.bo.entityId)->has("known"), "now Ada knows him as Bo");
    expect(t.ada.said().find(" is Bo.") != std::string::npos, "and is told so:\n" + t.ada.said());
    expect(t.bo.said().find("You introduced yourself as Bo to") != std::string::npos, "Bo gets a receipt:\n" + t.bo.said());
    expect(t.cy.calls(t.bo.entityId) == "Bo", "Cy was close enough to hear it too");
    // An alias: Bo takes "Kestrel", and gives it too.
    t.g.command(&t.bo, cmd({{"type", "names"}, {"verb", "add"}, {"name", "Kestrel"}}));
    t.tick(.3);
    const auto& aliases = t.bo.snapshots.back()["self"]["names"].array("aliases");
    expect(aliases.size() == 1 && aliases[0].asString() == "Kestrel", "Bo goes by Kestrel as well");
    t.g.command(&t.bo, cmd({{"type", "names"}, {"verb", "add"}, {"name", "Bo"}}));
    expect(t.bo.said().find("That is your own name") != std::string::npos, "not by his own name twice");
    t.say(t.bo, "\"Though some call me Kestrel.\"");
    expect(t.ada.calls(t.bo.entityId) == "Kestrel", "a second name: the newest is used");
    // Ada introduces herself to Cy alone, by a whisper.
    t.place(t.bo, 9, 0);
    t.tick(.3);
    auto o = json::Value::object();
    o.add("type", "chat");
    o.add("volume", "whisper");
    o.add("text", "\"I'm Ada.\"");
    t.g.command(&t.ada, json::dump(o));
    t.tick(.6);
    expect(t.cy.calls(t.ada.entityId) == "Ada", "Cy, close by, hears Ada's whisper");
    expect(t.bo.calls(t.ada.entityId) != "Ada", "Bo, further off, doesn't: " + t.bo.calls(t.ada.entityId));
}

void theGamesMessagesAreVeiled()
{
    Three t(options());
    t.g.command(&t.ada, cmd({{"type", "action"}, {"action", "invite"}, {"target", t.bo.entityId}}));
    t.tick(.3);
    expect(t.bo.said().find("Ada invites you") == std::string::npos && t.bo.said().find("invites you to join their party") != std::string::npos,
           "the invitation names her as she looks:\n" + t.bo.said());
    const auto party = t.bo.snapshots.back()["self"]["party"];
    expect(party["invite"].string("name") != "Ada", "and so does the Party panel");
}

void residentsAndTheirNames()
{
    Three t(options());
    auto& w = t.g.world();
    // A resident beside Ada, asked her name.
    std::string npc;
    for (const auto& [id, e] : w.entities())
        if (e.npc && !e.transient && w.society().resident(id) && e.cellId == w.entity(t.ada.entityId)->cellId)
        {
            npc = id;
            break;
        }
    expect(!npc.empty(), "a resident in the room");
    for (const auto& [id, e] : w.entities())
        if (e.npc)
            w.entity(id)->leaderId = "test-frozen";
    auto* r = w.entity(npc);
    r->position = {w.entity(t.ada.entityId)->position.x + 1, w.entity(t.ada.entityId)->position.y + 1};
    t.tick(.3);
    expect(t.ada.calls(npc) != r->name, "a resident is a stranger at first: " + t.ada.calls(npc));
    // She introduces herself: the resident knows her name now, and tells his in return.
    t.say(t.ada, "\"Good day. I'm Ada.\"", {npc});
    t.tick(1);
    expect(t.g.dialogueContext(npc, t.ada.entityId, "hello", true).playerName == "Ada", "the resident knows her name");
    expect(t.ada.calls(npc) == r->name, "and has given his: " + t.ada.calls(npc) + "\n" + t.ada.said());
    // Bo, distrusted, asks: the resident keeps their name.
    w.bonds().change(npc, t.bo.entityId, {0, -60, 0, 0, 0}, w.calendarDays());
    t.place(t.bo, 1, 1.4);
    t.tick(.3);
    t.say(t.bo, "\"What is your name?\"", {npc});
    t.tick(1);
    expect(t.bo.calls(npc) != r->name, "a resident who distrusts Bo keeps their name from him:\n" + t.bo.said());
    expect(t.g.dialogueContext(npc, t.bo.entityId, "hello", true).playerName != "Bo", "nor does the resident know his");
    // Cy asks plainly, and is told.
    t.place(t.cy, -1, 1.4);
    t.tick(.3);
    t.say(t.cy, "\"What is your name?\"", {npc});
    t.tick(1);
    expect(t.cy.calls(npc) == r->name, "Cy asks, and is told:\n" + t.cy.said());
}

void aRestartKeepsWhoKnowsWhom()
{
    auto o = options();
    o.forkSnapshots = false;
    o.savePath = "/tmp/ratw-names-test-" + std::to_string(::getpid()) + ".json";
    std::remove(o.savePath.c_str());
    {
        Three t(o);
        t.g.command(&t.bo, cmd({{"type", "names"}, {"verb", "add"}, {"name", "Kestrel"}}));
        t.say(t.bo, "\"Call me Kestrel.\"");
        expect(t.ada.calls(t.bo.entityId) == "Kestrel", "Ada knows Bo as Kestrel");
        t.g.save();
    }
    {
        Three t(o);
        expect(t.ada.calls(t.bo.entityId) == "Kestrel", "still, after a restart");
        expect(t.cy.calls(t.ada.entityId) != "Ada", "and nobody learned more than they heard");
        expect(t.bo.snapshots.back()["self"]["names"].array("aliases").size() == 1, "Bo's alias is kept");
    }
    std::remove(o.savePath.c_str());
}

void namesShownWhenNotHidden()
{
    auto o = options();
    o.hiddenNames = false;
    Three t(o);
    expect(t.ada.calls(t.bo.entityId) == "Bo" && !t.ada.offered(t.bo.entityId, "introduce"), "with names not hidden, Bo is Bo");
}
} // namespace

int main()
{
    try
    {
        hearingAnIntroduction();
        aliasesAndLooks();
        whoKnowsWhom();
        strangersAndIntroductions();
        theGamesMessagesAreVeiled();
        residentsAndTheirNames();
        aRestartKeepsWhoKnowsWhom();
        namesShownWhenNotHidden();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
    std::cout << "names tests: " << checks << " checks passed\n";
    return 0;
}
