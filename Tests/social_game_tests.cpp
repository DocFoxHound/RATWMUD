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
    // A party's scene starts Private (doc 51): a stranger answering the party doesn't join it.
    std::string partyScene;
    for (const auto& [id, s] : l.sessions)
        if (s.party == "party-1")
            partyScene = id;
    expect(l.sessions[partyScene].openness == "private", "a party's scene starts private");
    l.record({event++, 112, "ada", "tavern", 12, false, 15, {}, "party-1"}, {"ada", "bo", "ed"});
    l.record({event++, 115, "ed", "tavern", 12, false, 16, {}, ""}, {"ed", "ada"});
    expect(!l.sessions[partyScene].members.count("ed"), "private: someone answering the party stays out of it");
    // Made Open, it takes one who answers a member (heard them lately, and is heard by them).
    expect(l.setOpenness("ada", partyScene, "open", 140).ok, "Ada opens it");
    l.record({event++, 141, "ada", "tavern", 12, false, 17, {}, "party-1"}, {"ada", "bo", "fi"});
    l.record({event++, 144, "fi", "tavern", 12, false, 18, {}, ""}, {"fi", "ada"});
    expect(l.sessions[partyScene].members.count("fi"), "open: someone answering the party joins its scene");
}

// A fight is a scene of its own (doc 33): those who talk it through are paid a scene's ordinary pay (the user,
// 2026-10-07, doc 51), and each may give a Gold Star to each of the others.
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
    const int talked = 20;
    expect(l.paidFor("ada", scene) == talked && l.paidFor("bo", scene) == talked,
           "talking it through: a scene's ordinary pay: " + std::to_string(l.paidFor("ada", scene)));
    expect(l.paidFor("cy", scene) == 0 && l.receiptsOf("cy").size() == 1,
           "fighting in silence pays no social XP (it teaches fighting: doc 49), but leaves a receipt, so stars still come");
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
    expect(m.receiptsOf("di").size() == 1 && m.receiptsOf("ed").empty(), "only those who took their turns have the fight's receipt");
}

// Working together is a scene of its own (doc 53, 2.2): its members in it from the start; those who talk it through are
// paid when the work ends, and working in silence pays nothing.
void workScenes()
{
    SocialLedger l;
    std::uint64_t event = 1;
    const double t = 7000;
    for (const char* who : {"ada", "bo", "cy"})
        l.joinWork("j1", "wilds", who, t);
    const auto scene = SocialLedger::workScene("j1");
    expect(l.sessions.count(scene) && l.sessions[scene].members.size() == 3 && SocialLedger::isWork(l.sessions[scene]) &&
               l.sessions[scene].openness == "open",
           "the work's scene, open, with its members in it from the start");
    const auto talk = converse(l, "ada", "bo", t + 1, event, "wilds", SocialLedger::workTag("j1"));
    expect(talk == scene, "their words while working are the work's scene");
    l.settleWork("j1", t + 60);
    expect(l.paidFor("ada", scene) == 20 && l.paidFor("bo", scene) == 20, "talking it through pays: " + std::to_string(l.paidFor("ada", scene)));
    expect(l.paidFor("cy", scene) == 0, "working in silence pays nothing");
    expect(l.settleWork("j1", t + 70) == 0 && l.paidFor("ada", scene) == 20, "never paid twice");
}

// Openness, joining and knocking (doc 51, Phase 3), in the ledger alone.
void opennessJoiningAndKnocking()
{
    std::uint64_t event = 1;
    const auto post = [&](SocialLedger& l, const std::string& who, double at, std::vector<std::string> hearers) {
        l.record({event++, at, who, "tavern", 12, false, event * 7919, {}, ""}, hearers);
    };
    // A room scene starts Open; Join lets one in, and their next line counts at once, heard or not.
    SocialLedger l;
    const auto scene = converse(l, "ada", "bo", 1000, event);
    expect(l.sessions[scene].openness == "open", "a room scene starts open");
    expect(l.join("cy", scene, 1040).ok, "Cy joins it");
    post(l, "cy", 1041, {"cy"});
    expect(l.sessions[scene].members.count("cy"), "Join: the next line counts, with no A-B-A");
    expect(l.scenesOf("cy").count(scene), "and the index has her in it");
    // Knock: a line heard by its wolves doesn't join it; a knock let in does; turned away waits five minutes.
    expect(!l.setOpenness("ada", scene, "ajar", 1050).ok, "open, knock or private");
    expect(!l.setOpenness("di", scene, "private", 1050).ok, "only its wolves change it");
    expect(l.setOpenness("ada", scene, "private", 1045).ok, "the first change, any time");
    expect(!l.setOpenness("bo", scene, "knock", 1060).ok, "the next not within 30 s of it");
    expect(l.setOpenness("ada", scene, "knock", 1076).ok && l.sessions[scene].openness == "knock", "Ada makes it knock to join");
    expect(!l.join("di", scene, 1051).ok, "no Join on a Knock scene");
    post(l, "di", 1052, {"di", "ada", "bo"});
    expect(!l.sessions[scene].members.count("di"), "heard by its wolves, Di still isn't in it");
    expect(l.knock("di", scene, 1053).ok && !l.knock("di", scene, 1054).ok, "Di knocks, once");
    expect(l.refuse("bo", scene, "di", 1055).ok, "Bo: not now");
    expect(!l.knock("di", scene, 1056).ok && !l.knock("di", scene, 1055 + 299).ok, "turned away: no knocking for five minutes");
    expect(l.knock("di", scene, 1055 + 301).ok, "then a knock again");
    expect(l.admit("ada", scene, "di", 1360).ok, "Ada lets her in");
    post(l, "di", 1362, {"di", "ada"});
    expect(l.sessions[scene].members.count("di"), "let in: her next line counts");
    expect(l.knock("ed", scene, 1400).ok && !l.admit("ada", scene, "ed", 1400 + 121).ok, "a knock lapses after two minutes");
    // Private: no Join, no knock, nobody by themselves.
    expect(l.setOpenness("bo", scene, "private", 1500).ok, "Bo makes it private");
    expect(!l.join("fi", scene, 1501).ok && !l.knock("fi", scene, 1502).ok, "private: no Join, no knock");
    // Several scenes in one place: two strangers beside the private one make their own.
    post(l, "fi", 1503, {"fi", "gu"});
    post(l, "gu", 1506, {"gu", "fi"});
    post(l, "fi", 1509, {"fi", "gu", "ada"});
    int scenesHere = 0;
    for (const auto& sid : l.openIn("tavern"))
        scenesHere += l.sessions[sid].ended == 0;
    expect(scenesHere == 2 && !l.sessions[scene].members.count("fi"), "two scenes in the tavern; the private one keeps to itself");
    // A copy, as a load makes one: indexes rebuilt, routing as before.
    SocialLedger copy;
    copy.sessions = l.sessions;
    copy.reindexScenes();
    expect(copy.scenesOf("ada").count(scene) && copy.openIn("tavern").size() == 2, "indexes rebuilt from the scenes");
    // Ended scenes are kept eight days (players are told seven), then let go; each member's last ended scene is known.
    l.settle(scene, 2000);
    expect(l.lastEnded("ada") == scene && !l.scenesOf("ada").count(scene) && !l.openIn("tavern").count(scene), "ended: out of the indexes");
    l.tick(2000 + 8 * 86400 - 10);
    expect(l.sessions.count(scene), "kept for eight days");
    l.tick(2000 + 8 * 86400 + 10);
    expect(!l.sessions.count(scene) && l.lastEnded("ada").empty(), "then let go");
}

void leavingAScene()
{
    SocialLedger l;
    std::uint64_t event = 1;
    double t = 1000;
    const auto scene = converse(l, "ada", "bo", t, event);
    // Cy says a few words in it and goes.
    l.record({event++, t + 40, "cy", "tavern", 6, false, 77, {}, {}}, {"cy", "ada", "bo"});
    expect(l.sessions[scene].members.count("cy"), "Cy is in the scene");
    int paid = -1;
    expect(l.leave("cy", scene, t + 45, &paid) && paid == 0 && l.paidFor("cy", scene) == 0, "Cy steps out with nothing");
    expect(l.leave("ada", scene, t + 50, &paid) && paid == 20 && l.paidFor("ada", scene) == 20,
           "Ada steps out, qualified with Bo: paid at once");
    expect(l.sessions[scene].ended == 0, "the scene carries on without her");
    expect(!l.leave("ada", scene, t + 51), "she can't step out twice");
    const int words = l.sessions[scene].members["ada"].words;
    l.record({event++, t + 60, "ada", "tavern", 21, false, 99, {}, {}}, {"ada", "bo"});
    expect(l.sessions[scene].members["ada"].words == words, "her words count no more there");
    l.settle(scene, t + 100);
    expect(l.paidFor("bo", scene) == 20, "Bo is paid when it ends: Ada's part still counts for the scene");
    int receipts = 0;
    for (const auto& e : l.entries)
        receipts += e.actor == "ada" && e.session == scene;
    expect(receipts == 1, "and Ada isn't paid twice");
    l.joinFight("b1", "tavern", "ada", t + 200);
    expect(!l.leave("ada", SocialLedger::fightScene("b1"), t + 201), "no stepping out of a fight's scene");
    expect(!l.leave("ada", "no-such-scene", t + 202), "nor out of one that isn't there");
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
        const auto& scenes = t.ada.social().array("scenes");
        expect(scenes.size() == 1 && scenes[0].boolean("next") && !scenes[0].boolean("fight") && !scenes[0].boolean("quiet") &&
                   scenes[0].number("endsIn") > 1700 && scenes[0].number("needTurns") == 0 && scenes[0].number("needWords") == 0 &&
                   !scenes[0].boolean("needReply") && scenes[0].number("othersShaped") == 1,
               "the scenes she is in: where her words go, what she still needs, when it ends: " + json::dump(t.ada.social()));
        t.g.command(&t.ada, cmd({{"type", "social"}, {"verb", "leave"}, {"session", "no-such-scene"}}));
        expect(t.ada.said().find("You aren't in that scene.") != std::string::npos, "stepping out of a scene she isn't in");
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
        // (22: his scene and Ada's star. A place first visited pays nothing any more: doc 49.)
        expect(t.bo.snapshots.back()["self"].number("socialXp") == 22, "and Bo's star: " + std::to_string(t.bo.snapshots.back()["self"].number("socialXp")));
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
        workScenes();
        leavingAScene();
        opennessJoiningAndKnocking();
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
