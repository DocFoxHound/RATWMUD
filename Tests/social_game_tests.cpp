// The individual social game (Docs/Design/32-parties-chapters-factions.md, Part 1): actions at half weight, a party's
// own scene, Gold Stars, Stories and Story Stars in the ledger (Core/RatwSocialCore.h); then through the game: the
// scene a player sees, a star given, regard and a private note in Look, a name about town, and a restart.
#include "RatwGame.h"
#include "RatwSocialCore.h"

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

const char* const Lines[] = {
    "The river rose in the night and took the lower bridge with it, so we came the long way round by the mill.",
    "Then you will have seen the miller's dog, the grey one that guards the ford and barks at every passing cart.",
    "We did, and it followed us halfway to the crossroads before it lost interest and wandered back home again.",
    "That dog has walked that road longer than I have been alive, and it still thinks the whole valley is its own.",
    "Perhaps it is right; the miller certainly behaves as though the valley answers to him and his bad-tempered dog.",
    "He lent us his cart once when the rain came, so I will not hear a word against him, whatever his temper.",
};

// A two-wolf exchange that qualifies (two turns, 35 words and a reply each), in `cell`, said as `party` ("" for none).
std::string converse(SocialLedger& l, const std::string& a, const std::string& b, double at, std::uint64_t& event,
                     const std::string& cell = "tavern", const std::string& party = {})
{
    for (int i = 0; i < 6; ++i)
    {
        const auto& who = i % 2 ? b : a;
        const auto& other = i % 2 ? a : b;
        SocialPost p{event++, at + i * 5, who, cell, 21, false, std::hash<std::string>{}(Lines[i]) + event, {}, party};
        l.record(p, {who, other});
    }
    for (const auto& [id, s] : l.sessions)
        if (s.ended == 0 && s.members.count(a))
            return id;
    return {};
}

// ------------------------------------------------------------------ The ledger alone

void actionsAndPartyScenes()
{
    const auto evidence = roleplayEvidence(parsePost("/action leans in close and listens to every word."));
    expect(evidence.words == 0 && evidence.actionWords == 8, "an action's words are counted apart (" +
                                                                 std::to_string(evidence.actionWords) + ")");
    expect(roleplayEvidence(parsePost("/me is tired")).actionWords == 0, "a state set with /me counts for nothing");
    SocialLedger l;
    std::uint64_t event = 1;
    // Party mates in earshot: their own scene at once, no A-B-A.
    SocialPost first{event++, 100, "ada", "tavern", 12, false, 11, {}, "party-1"};
    l.record(first, {"ada", "bo"});
    expect(l.sessions.size() == 1 && l.sessions.begin()->second.party == "party-1", "a party scene opens on the first line");
    // Strangers talking in the same room don't join it, and don't make it theirs.
    l.record({event++, 103, "cy", "tavern", 12, false, 12, {}, ""}, {"cy", "di"});
    l.record({event++, 106, "di", "tavern", 12, false, 13, {}, ""}, {"di", "cy"});
    l.record({event++, 109, "cy", "tavern", 12, false, 14, {}, ""}, {"cy", "di"});
    int cellScenes = 0;
    for (const auto& [id, s] : l.sessions)
        cellScenes += s.party.empty() && s.members.count("cy") && !s.members.count("ada");
    expect(cellScenes == 1 && !l.sessions.begin()->second.members.count("cy"), "the room's own scene forms apart from the party's");
    // A stranger answering the party joins its scene.
    l.record({event++, 112, "ada", "tavern", 12, false, 15, {}, "party-1"}, {"ada", "bo", "ed"});
    l.record({event++, 115, "ed", "tavern", 12, false, 16, {}, ""}, {"ed", "ada"});
    bool joined = false;
    for (const auto& [id, s] : l.sessions)
        joined |= s.party == "party-1" && s.members.count("ed");
    expect(joined, "someone answering the party joins its scene");
}

// A fight is a scene of its own (doc 33): its players are paid for the fight, twice the usual for talking it through,
// and each may give a Gold Star to each of the others.
void fightScenes()
{
    SocialLedger l;
    std::uint64_t event = 1;
    const double t = 5000;
    for (const char* who : {"ada", "bo", "cy"})
        l.joinFight("b1", "field", who, t);
    const auto scene = SocialLedger::fightScene("b1");
    expect(l.sessions.count(scene) && l.sessions[scene].members.size() == 3 && SocialLedger::isFight(l.sessions[scene]),
           "the fight's scene has its fighters in it from the start");
    // Ada and Bo talk it through (their words carry the fight's tag); Cy fights in silence.
    const auto talk = converse(l, "ada", "bo", t + 1, event, "field", SocialLedger::fightTag("b1"));
    expect(talk == scene, "their words are the fight's scene, not a scene of their own");
    expect(l.endFor("ada", t + 50) == 0 && l.sessions[scene].ended == 0, "a fight's scene ends with the fight, not a scene-end");
    l.settleFight("b1", {"ada", "bo", "cy"}, t + 60);
    const int talked = SocialLedger::FightXP + SocialLedger::FightTalkFactor * 20;
    expect(l.paidFor("ada", scene) == talked && l.paidFor("bo", scene) == talked,
           "talking it through: the fight, and twice a scene's pay: " + std::to_string(l.paidFor("ada", scene)));
    expect(l.paidFor("cy", scene) == SocialLedger::FightXP, "fighting in silence: the fight's pay");
    expect(l.settleFight("b1", {"ada"}, t + 70) == 0 && l.paidFor("ada", scene) == talked, "never paid twice");
    // Stars: one to each of the others, as many as took part.
    expect(l.star("cy", "ada", scene, t + 80).ok && l.star("cy", "bo", scene, t + 81).ok, "Cy stars both of them");
    expect(!l.star("cy", "ada", scene, t + 82).ok, "but each only once");
    expect(l.star("ada", "cy", scene, t + 83).ok, "and the silent fighter can be starred too");
    // One who took no part in it (fewer than two turns, no words) is not paid.
    SocialLedger m;
    m.joinFight("b2", "field", "di", t);
    m.joinFight("b2", "field", "ed", t);
    m.settleFight("b2", {"di"}, t + 30);
    expect(m.paidFor("di", SocialLedger::fightScene("b2")) == SocialLedger::FightXP && m.paidFor("ed", SocialLedger::fightScene("b2")) == 0,
           "only those who took their turns are paid for the fight");
}

void starsAndStories()
{
    SocialLedger l;
    std::uint64_t event = 1;
    double t = 1000;
    const auto one = converse(l, "ada", "bo", t, event);
    expect(!one.empty(), "a scene forms");
    l.settle(one, t + 100);
    expect(l.paidFor("ada", one) == 20 && l.paidFor("bo", one) == 20, "both are paid for it");
    expect(!l.star("ada", "ada", one, t + 110).ok, "no star for oneself");
    expect(!l.star("cy", "ada", one, t + 110).ok, "only those who took part give stars");
    const auto star = l.star("ada", "bo", one, t + 110);
    expect(star.ok && star.amount == 2, "a Gold Star: up to 2");
    expect(!l.star("ada", "bo", one, t + 120).ok, "one star a scene");
    // Stories: begun, agreed, carried on, told.
    const auto proposed = l.propose("ada", one, "The Long Way Round", t + 130);
    expect(proposed.ok && l.stories[proposed.message].state == "pending", "a Story waits for the others' word");
    expect(!l.propose("bo", one, "Again", t + 131).ok, "a scene belongs to one Story");
    expect(l.approve("bo", proposed.message, t + 140).ok && l.stories[proposed.message].state == "active", "two thirds agree: under way");
    expect(!l.close("ada", proposed.message, t + 150).ok, "a Story needs two scenes");
    t += 4000;                              // (Out of the first scene's window.)
    const auto two = converse(l, "bo", "ada", t, event);
    l.settle(two, t + 100);
    expect(!l.extend("bo", proposed.message, two, t + 110).ok, "only whoever began it carries it on");
    expect(l.extend("ada", proposed.message, two, t + 110).ok, "Ada carries it on");
    const int before = l.points["ada"];
    const auto closed = l.close("ada", proposed.message, t + 120);
    const int paid = l.paidFor("ada", one) + l.paidFor("ada", two);
    expect(closed.ok && l.points["ada"] - before == paid / 4 + 1, "told: a quarter of what the scenes paid, and one for continuing");
    const auto storyStar = l.storyStar("bo", "ada", proposed.message, t + 130);
    expect(storyStar.ok && storyStar.amount >= 1, "a Story Star");
    expect(!l.storyStar("bo", "ada", proposed.message, t + 131).ok, "one a Story");
    // A Story nobody agrees to lapses after a day.
    const auto three = converse(l, "ada", "cy", t + 9000, event);
    l.settle(three, t + 9100);
    const auto lonely = l.propose("ada", three, "Unagreed", t + 9200);
    l.tick(t + 9200 + 86401);
    expect(l.stories[lonely.message].state == "expired", "unagreed for a day, it lapses");
    expect(socialTitle(1) == "Stranger" && socialTitle(3) == "Known" && socialTitle(5) == "Familiar Face" && socialTitle(12) == "Notable",
           "titles by level");
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
        expect(sections::fill(v, cache), "a snapshot can be filled");
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
    const json::Value* last(const std::string& type) const
    {
        for (auto it = events.rbegin(); it != events.rend(); ++it)
            if (it->string("type") == type)
                return &*it;
        return nullptr;
    }
    const json::Value& social() const { return snapshots.back()["self"]["social"]; }
};

std::string cmd(std::initializer_list<std::pair<const char*, json::Value>> fields)
{
    auto o = json::Value::object();
    for (const auto& [k, v] : fields)
        o.add(k, v);
    return json::dump(o);
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
    void say(Client& c, const std::string& text, const char* channel = "")
    {
        g.command(&c, cmd({{"type", "chat"}, {"text", text}, {"channel", channel}}));
        tick(2.2);
    }
};

game::Options options(const std::string& save = {})
{
    game::Options o;
    o.devIdentity = true;
    o.hiddenNames = false;
    o.forkSnapshots = false;
    if (!save.empty())
        o.savePath = save;
    return o;
}

void aSceneSeenAndStarred(const std::string& save)
{
    std::remove(save.c_str());
    {
        Two t(options(save));
        t.g.command(&t.ada, cmd({{"type", "action"}, {"action", "invite"}, {"target", t.bo.entityId}}));
        t.g.command(&t.bo, cmd({{"type", "party"}, {"verb", "accept"}}));
        t.tick(.3);
        // Two rounds each: the ledger takes one line a wolf every two real seconds.
        for (int i = 0; i < 4; ++i)
        {
            if (i == 2)
                ::usleep(2100000);
            t.say(i % 2 ? t.bo : t.ada, std::string("\"") + Lines[i] + "\"", "party");
        }
        t.tick(2.2);
        const auto& scene = t.ada.social()["scene"];
        expect(scene.boolean("party") && scene.array("with").size() == 1 && scene.number("turns") >= 2,
               "Ada sees she is in her party's scene, with Bo: " + json::dump(t.ada.social()) + "\n" + t.ada.said());
        expect(t.ada.social().string("title") == "Stranger", "and her title");
        t.g.command(&t.ada, cmd({{"type", "action"}, {"action", "session_end"}}));
        t.tick(2.2);
        expect(t.ada.said().find("The scene ends. +20 social.") != std::string::npos, "it ends, and pays:\n" + t.ada.said());
        const auto& ended = t.ada.social()["ended"];
        expect(ended.number("xp") == 20 && ended.array("starTargets").size() == 1 && ended.boolean("storyable"),
               "she may star Bo, and begin a Story");
        t.g.command(&t.ada, cmd({{"type", "social"}, {"verb", "star"}, {"session", ended.string("id")}, {"target", t.bo.entityId}}));
        t.tick(2.2);
        expect(t.bo.said().find("gives you a Gold Star (+2 social)") != std::string::npos, "Bo gets her star:\n" + t.bo.said());
        expect(t.ada.social()["ended"].array("starTargets").empty(), "and she has given hers");
        t.g.command(&t.ada, cmd({{"type", "social"}, {"verb", "propose"}, {"session", ended.string("id")}, {"name", "The Mill Road"}}));
        t.tick(2.2);
        expect(t.bo.said().find("would make a Story of your scene") != std::string::npos, "Bo is asked to agree");
        const auto& stories = t.bo.social().array("stories");
        expect(stories.size() == 1 && stories[0].string("state") == "pending" && !stories[0].boolean("mine"), "he sees it waiting");
        t.g.command(&t.bo, cmd({{"type", "social"}, {"verb", "approve"}, {"story", stories[0].string("id")}}));
        t.tick(2.2);
        expect(t.ada.social().array("stories")[0].string("state") == "active", "agreed: under way");
        // Look: how a resident regards her, and a note of her own.
        std::string npc;
        for (const auto& [id, e] : t.g.world().entities())
            if (e.npc && !e.transient && e.cellId == t.g.world().entity(t.ada.entityId)->cellId)
                npc = id;
        {
            const auto* a = t.g.world().entity(t.ada.entityId);
            t.g.world().entity(npc)->position = {a->position.x, a->position.y + 1};
            t.g.world().entity(npc)->leaderId = "test-frozen";
        }
        t.g.world().bonds().change(npc, t.ada.entityId, {40, 30, 20, 0, 0}, t.g.world().calendarDays());
        t.g.command(&t.ada, cmd({{"type", "social"}, {"verb", "note"}, {"target", npc}, {"text", "Owes me a favour."}}));
        t.g.command(&t.ada, cmd({{"type", "action"}, {"action", "inspect"}, {"target", npc}}));
        const auto* look = t.ada.last("inspect");
        expect(look && look->string("regard").find("know you") != std::string::npos &&
                   look->string("regard").find("like you") != std::string::npos && look->string("note") == "Owes me a favour.",
               "Look says how they regard her, and shows her note: " + (look ? json::dump(*look) : t.ada.said()));
        // Her name about town.
        t.g.command(&t.ada, cmd({{"type", "social"}, {"verb", "reputation"}}));
        const auto* rep = t.ada.last("reputation");
        expect(rep && !rep->array("lines").empty() && rep->array("lines")[0].asString().find("known to 1 resident") != std::string::npos,
               "her name about town: " + (rep ? json::dump(*rep) : std::string("none")));
        t.g.save();
    }
    {
        Two t(options(save));
        expect(t.ada.social().array("stories").size() == 1 && t.ada.social().array("stories")[0].string("name") == "The Mill Road",
               "the Story survives a restart");
        std::string npc;
        for (const auto& [id, e] : t.g.world().entities())
            if (e.npc && !e.transient && e.cellId == t.g.world().entity(t.ada.entityId)->cellId)
                npc = id;
        {
            const auto* a = t.g.world().entity(t.ada.entityId);
            t.g.world().entity(npc)->position = {a->position.x, a->position.y + 1};
        }
        t.g.command(&t.ada, cmd({{"type", "action"}, {"action", "inspect"}, {"target", npc}}));
        expect(t.ada.last("inspect") && t.ada.last("inspect")->string("note") == "Owes me a favour.", "and so does her note");
        expect(t.bo.snapshots.back()["self"].number("socialXp") == 22, "and Bo's star");
    }
    std::remove(save.c_str());
}
} // namespace

int main()
{
    try
    {
        actionsAndPartyScenes();
        fightScenes();
        starsAndStories();
        aSceneSeenAndStarred("/tmp/ratw-social-test-" + std::to_string(::getpid()) + ".json");
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
    std::cout << "social game tests: " << checks << " checks passed\n";
    return 0;
}
