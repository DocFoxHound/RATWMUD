// People (Docs/Design/50-player-card-friends-safety.md, Phase 1): the roleplay profile's rules (limits, cleaning, who
// sees which part, what residents are told), handles, status and walk-up; and through the game: a profile written and
// seen by another player as far as they may, a title only once introduced, status marks on the map, the Storyteller
// quill refused, played time, and a profile kept across a restart.
#include "RatwGame.h"
#include "RatwPeople.h"

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

void theRules()
{
    expect(people::clean("  Hello\x01 there\n  ", 80) == "Hello there", "control characters out, trimmed, newlines spaced");
    expect(people::clean("one\ntwo", 80, true) == "one\ntwo", "newlines kept where the field allows");
    expect(people::clean("abcdef", 3) == "abc" && people::clean("éééé", 2) == "éé", "cut by characters, never inside one");
    people::Profile p;
    std::string why;
    expect(people::applyFields(p, parsed(R"({"description": "A lean grey wolf.", "currently": "mending nets",
        "glances": [{"icon": "scar", "title": "A fresh scar", "line": "over one eye"}, {"icon": "smoke", "title": "Woodsmoke", "line": "", "sense": "scent"}],
        "title": "the Ferryman", "motto": "Across, and back.", "oocNotes": "Happy to RP any time.", "consent": {"injury": "ask", "death": "no"},
        "sliders": {"Cautious/Impulsive": 4}})"), why),
           "a profile is set: " + why);
    expect(p.revision == 1 && p.glances.size() == 2 && p.sliders.at("Cautious/Impulsive") == 4 && p.consent.at("death") == "no", "all of it, revision 1");
    expect(people::applyFields(p, parsed(R"({"currently": "mending nets"})"), why) && p.revision == 1, "no change, no new revision");
    expect(!people::applyFields(p, parsed(R"({"birthplace": "Ridgemere"})"), why), "no birthplace (the user, 2026-10-07)");
    expect(!people::applyFields(p, parsed(R"({"pronouns": "they/them"})"), why), "no pronouns: they follow the character's sex (the user)");
    expect(!people::applyFields(p, parsed(R"({"glances": [{"icon": "dragon", "title": "x"}]})"), why), "a glance needs a known icon");
    expect(!people::applyFields(p, parsed(R"({"glances": [{"icon": "scar", "title": "x", "sense": "taste"}]})"), why), "and a known sense");
    expect(!people::applyFields(p, parsed(R"({"consent": {"injury": "maybe"}})"), why), "lines and veils: yes, no or ask");
    expect(people::applyFields(p, parsed(R"({"sliders": {"Gentle/Brutal": 99}})"), why) && p.sliders.at("Gentle/Brutal") == 10, "a slider is -10..10");
    expect(!people::validStatus("storyteller", false, why) && people::validStatus("lfs", false, why) && people::validStatus("storyteller", true, why),
           "the quill only for approved storytellers (the user, 2026-10-07)");
    expect(!people::validHandle("ab", "", why) && !people::validHandle("Grey Fox!", "", why) && people::validHandle("Grey Fox", "", why),
           "a handle: 3 to 24 letters, digits, spaces, _ or -");
    expect(!people::validHandle("Wren", "wren", why), "never the sign-in name");
    // Who sees what.
    people::Viewer stranger;
    stranger.smells = false;
    auto card = people::cardFor(p, stranger);
    expect(card.has("description") && card.has("currently") && card.array("glances").size() == 1 && !card.has("title"),
           "a stranger out of nose's reach: the description, Currently and the scar, no title");
    expect(card.object("ooc").string("notes") == "Happy to RP any time.", "players see the OOC tab");
    people::Viewer close = stranger;
    close.smells = true;
    close.knowsName = true;
    card = people::cardFor(p, close);
    expect(card.array("glances").size() == 2 && card.string("title") == "the Ferryman" && card.string("motto") == "Across, and back.",
           "close enough to smell, and knowing the name: the woodsmoke, the title and motto");
    people::Viewer resident = stranger;
    resident.player = false;
    expect(!people::cardFor(p, resident).has("ooc"), "a resident never sees the OOC tab");
    const auto told = people::residentContext(p, resident);
    expect(told.find("A fresh scar") != std::string::npos && told.find("mending nets") != std::string::npos &&
               told.find("Ferryman") == std::string::npos && told.find("RP any time") == std::string::npos && told.find("Woodsmoke") == std::string::npos,
           "a resident is told what it perceives, never the title, the OOC tab or a scent it can't catch: " + told);
    p.mature = true;
    people::Viewer careful = stranger;
    careful.showMature = false;
    card = people::cardFor(p, careful);
    expect(card.boolean("folded") && !card.has("description") && !card.has("glances"), "a mature profile is folded for those who'd rather");
    const auto back = people::load(people::save(p));
    expect(people::save(back).string("description") == "A lean grey wolf." && back.mature && back.revision == p.revision && back.glances.size() == 2,
           "a profile round-trips");
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
};

std::string cmd(std::initializer_list<std::pair<const char*, json::Value>> fields)
{
    auto o = json::Value::object();
    for (const auto& [k, v] : fields)
        o.add(k, v);
    return json::dump(o);
}

game::Options options(const std::string& save = {})
{
    game::Options o;
    o.devIdentity = true;
    o.forkSnapshots = false;
    if (!save.empty())
        o.savePath = save;
    return o;
}

struct Two
{
    game::Game g;
    Client ada, bo;
    explicit Two(game::Options o) : g(o)
    {
        std::string problem;
        expect(g.start(problem), "starts: " + problem);
        ada.id = 1;
        bo.id = 2;
        g.connect(&ada);
        g.connect(&bo);
        g.command(&ada, cmd({{"type", "hello"}, {"id", "ada"}, {"name", "Ada"}}));
        g.command(&bo, cmd({{"type", "hello"}, {"id", "bo"}, {"name", "Bo"}}));
        auto* a = g.world().entity(ada.entityId);
        auto* b = g.world().entity(bo.entityId);
        b->cellId = a->cellId;
        b->position = {a->position.x + 1.2, a->position.y};
        tick(.5);
    }
    void tick(double seconds)
    {
        for (double t = 0; t < seconds; t += .05)
        {
            g.tick(.05);
            for (auto* c : {&ada, &bo})
                if (!c->snapshots.empty())
                    g.acknowledge(c, c->snapshots.back().number("revision"), false);
        }
    }
    // A command, and its reply (replies to a command that changes a character wait for the journal: doc 31).
    void command(Client& by, const std::string& text)
    {
        g.command(&by, text);
        g.settle();
    }
    const json::Value* look(Client& by, const std::string& at)
    {
        g.command(&by, cmd({{"type", "action"}, {"action", "inspect"}, {"target", at}}));
        return by.last("inspect");
    }
};

void throughTheGame()
{
    const std::string save = "/tmp/ratw-people-" + std::to_string(::getpid()) + ".json";
    {
        Two t(options(save));
        expect(t.ada.last("profile") && t.ada.last("profile")->object("rules").array("statuses").size() == 4, "entering, a wolf is sent its profile and the rules");
        t.command(t.ada, cmd({{"type", "profile"}, {"verb", "set"}, {"fields", parsed(R"({"description": "A lean grey wolf with salt in her fur.",
            "currently": "mending nets by the pier", "title": "the Ferryman", "oocNotes": "Walk up any time.",
            "glances": [{"icon": "scar", "title": "A fresh scar", "line": "over one eye"}, {"icon": "smoke", "title": "Woodsmoke", "line": "", "sense": "scent"}],
            "consent": {"injury": "ask"}})")}}));
        expect(t.ada.last("profile")->object("own").string("currently") == "mending nets by the pier",
               "her own profile comes back saved");
        // Bo, a stranger beside her: the description, Currently, both glances (he's close enough to smell), no title.
        auto* look = t.look(t.bo, t.ada.entityId);
        expect(look && look->string("description") == "A lean grey wolf with salt in her fur.", "the profile's description replaces the fixed line");
        const auto& card = look->object("profile");
        expect(card.string("currently") == "mending nets by the pier" && card.array("glances").size() == 2 && !card.has("title"),
               "a stranger sees what shows, not her title");
        expect(card.object("ooc").string("notes") == "Walk up any time.", "and, being a player, her OOC tab");
        t.g.world().entity(t.bo.entityId)->position.x += 6;     // Out of nose's reach.
        expect(t.look(t.bo, t.ada.entityId)->object("profile").array("glances").size() == 1, "further off, no woodsmoke");
        t.g.world().entity(t.bo.entityId)->position.x -= 6;
        // Once she introduces herself, her title.
        t.g.command(&t.ada, cmd({{"type", "chat"}, {"text", "I'm Ada."}}));
        t.tick(2.5);
        expect(t.look(t.bo, t.ada.entityId)->object("profile").string("title") == "the Ferryman", "introduced: her title");
        // A resident is told what it can see, and nothing of the OOC tab or title.
        std::string resident;
        for (const auto& [eid, e] : t.g.world().entities())
            if (e.npc && !e.transient && e.cellId == t.g.world().entity(t.ada.entityId)->cellId)
                resident = eid;
        if (!resident.empty())
        {
            auto* r = t.g.world().entity(resident);
            const auto* a = t.g.world().entity(t.ada.entityId);
            r->position = {a->position.x, a->position.y + 1};
            const auto told = t.g.profileContext(resident, t.ada.entityId);
            expect(told.find("mending nets") != std::string::npos && told.find("Ferryman") == std::string::npos &&
                       told.find("Walk up") == std::string::npos,
                   "a resident is told what it perceives only: " + told);
        }
        // Status: a mark by her label for others; the quill refused.
        t.command(t.ada, cmd({{"type", "profile"}, {"verb", "status"}, {"value", "lfs"}}));
        t.command(t.ada, cmd({{"type", "profile"}, {"verb", "walkup"}, {"on", true}}));
        t.tick(.5);
        const auto* her = t.bo.seen(t.ada.entityId);
        expect(her && her->string("rp") == "lfs" && her->boolean("walkup") && her->string("currently") == "mending nets by the pier",
               "Bo's map: Looking for a scene, walk-up, and her Currently");
        t.command(t.ada, cmd({{"type", "profile"}, {"verb", "status"}, {"value", "storyteller"}}));
        expect(t.ada.last("profile")->object("own").string("status") == "lfs", "the quill refused: not an approved storyteller");
        // Handles: unique; a dev identity's account is dev:<id>.
        t.command(t.ada, cmd({{"type", "profile"}, {"verb", "handle"}, {"handle", "Grey Fox"}}));
        expect(t.ada.last("profile")->object("account").string("handle") == "Grey Fox", "a handle chosen");
        t.command(t.bo, cmd({{"type", "profile"}, {"verb", "handle"}, {"handle", "grey fox"}}));
        expect(t.bo.last("profile")->object("account").string("handle").empty(), "taken, whatever the case");
        // Played time: a minute at the keys.
        t.g.command(&t.ada, cmd({{"type", "action"}, {"action", "look"}}));
        t.tick(61);
        t.command(t.ada, cmd({{"type", "profile"}, {"verb", "get"}}));
        expect(t.ada.last("profile")->object("account").number("playedMinutes") >= 1, "a minute played");
        t.g.save();
    }
    {
        Two t(options(save));
        const auto* own = t.ada.last("profile");
        expect(own && own->object("own").string("title") == "the Ferryman" && own->object("own").string("status") == "lfs" &&
                   own->object("account").string("handle") == "Grey Fox",
               "after a restart: her profile, status and handle");
    }
    std::remove(save.c_str());
}
} // namespace

int main()
{
    try
    {
        theRules();
        throughTheGame();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
    std::cout << "People tests passed: " << checks << " checks.\n";
    return 0;
}
