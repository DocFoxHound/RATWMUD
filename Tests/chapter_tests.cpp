// Chapters (Core/RatwChapters.h; Docs/Design/32-parties-chapters-factions.md, Part 3): the rules alone, then through
// the game: founded by three in a scene, its fee, the Chapter in each member's snapshot and on the map, invitations,
// its chats, treasury and hostile list, renown from a scene, and a restart.
#include "RatwChapters.h"
#include "RatwGame.h"

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

void foundingAndRanks()
{
    using namespace chapter;
    expect(!colourProblem("#c0392b").empty() && !colourProblem("#ff2020").empty(), "red is kept for the hostile");
    expect(!colourProblem("#101010").empty(), "too dark to see");
    expect(colourProblem("#5b8bd9").empty() && colourProblem("#d9b67b").empty(), "blue and gold will do");
    Chapters c;
    expect(!c.propose("ada", {"bo"}, "Ashen Lodge", "#5b8bd9", "", "scene-1", 0).ok, "three found it");
    expect(!c.propose("ada", {"bo", "cy"}, "A1", "#5b8bd9", "", "scene-1", 0).ok, "a name of letters");
    expect(!c.propose("ada", {"bo", "cy"}, "Ashen Lodge", "#5b8bd9", "", "", 0).ok, "in a scene");
    expect(c.propose("ada", {"bo", "cy"}, "Ashen Lodge", "#5b8bd9", "We keep the road.", "scene-1", 0).ok, "proposed");
    expect(c.proposalFor("bo", 10) && c.agree("bo", 10).ok && !c.of("ada"), "one agrees; not yet");
    const auto founded = c.agree("cy", 20);
    expect(founded.ok && !founded.message.empty() && c.of("ada") && c.together("bo", "cy"), "the last word founds it");
    const auto* ch = c.of("ada");
    expect(ch->level == 1 && ch->members.at("ada").rank == RankHead && ch->members.at("bo").rank == RankOfficer, "the proposer heads it");
    expect(!c.propose("di", {"ed", "fa"}, "ashen lodge", "#5b8bd9", "", "s", 30).ok, "names are unique");
    // Invitations and ranks.
    expect(c.invite("bo", "di", 100).ok && c.accept("di", 101).ok && c.member("di")->rank == RankInitiate, "an Officer invites; an Initiate joins");
    expect(!c.invite("di", "ed", 102).ok, "an Initiate can't invite");
    expect(c.remove("bo", "di", 103).ok && !c.of("di"), "an Officer sends an Initiate away");
    expect(!c.invite("bo", "di", 104).ok, "who was sent away can't come back yet");
    expect(c.invite("bo", "di", 104 + RejoinSeconds + 1).ok, "until a fortnight has passed");
    expect(!c.remove("bo", "cy", 105).ok, "only the Head sends away more than Initiates");
    expect(!c.renameRank("ada", RankInitiate, "Pup").ok, "ranks are named from Lodge");
    expect(c.setRank("ada", "cy", RankHead, 106).ok && c.member("cy")->rank == RankHead && c.member("ada")->rank == RankOfficer,
           "the headship handed on");
    // The hostile list.
    expect(c.markHostile("bo", "wren", "wolf", "stole from Ada", 107).ok && c.of("bo")->hostiles.size() == 1, "an Officer marks a wolf");
    expect(!c.markHostile("bo", "ada", "wolf", "", 107).ok, "not one's own");
    expect(c.unmarkHostile("bo", "wren").ok && c.of("bo")->hostiles.empty(), "and unmarks them");
    // Leaving: the Head hands on; the last out ends it.
    expect(c.leave("cy", 108).ok && c.of("ada")->members.at("ada").rank == RankHead, "the Head leaves: the longest-serving Officer heads it");
    c.leave("bo", 109);
    expect(c.leave("ada", 110).message == "ended" && c.all().empty(), "the last one out: no Chapter");
}

void renownAndLevels()
{
    using namespace chapter;
    Chapters c;
    c.propose("ada", {"bo", "cy"}, "Ashen Lodge", "#5b8bd9", "", "scene-1", 0);
    c.agree("bo", 1);
    const auto id = c.agree("cy", 2).message;
    expect(c.addRenown(id, "member scene", 250, "s1", "ada", 10) == 250, "renown");
    expect(c.addRenown(id, "member scene", 250, "s2", "ada", 20) == 50, "at most 300 a week");
    expect(c.addRenown(id, "award", 200, "dm", "", 30) == 200, "a Dungeon Master's award is outside the cap");
    expect(c.addRenown(id, "member scene", 100, "s3", "ada", 40 + WeekSeconds) == 100, "the week rolls on");
    for (const char* who : {"di", "ed"})
    {
        c.invite("ada", who, 50);
        c.accept(who, 50);
        c.touch(who, 60);
    }
    expect(c.activeMembers(*c.byId(id), 70) == 5, "five active");
    expect(c.advance(id, 70).empty(), "a Lodge needs a Chapter Story told");
    c.byId(id)->storiesTold = 1;
    const auto reached = c.advance(id, 70);
    expect(reached == std::vector<int>{2} && c.byId(id)->level == 2, "a Lodge (level II)");
    expect(c.advance(id, 70).empty(), "a Company needs its rented hall");
    expect(c.renameRank("ada", RankInitiate, "Pup").ok && c.byId(id)->rankNames[3] == "Pup", "a Lodge names its own ranks");
    // A Head away a month: the longest-serving active Officer heads it.
    c.touch("bo", 70 + HeadAwaySeconds);
    const auto changed = c.tick(80 + HeadAwaySeconds);
    expect(changed.size() == 1 && c.member("bo")->rank == RankHead, "a new Head when the Head is long away");
    Chapters d;
    d.load(parsed(json::dump(c.save())));
    expect(d.byId(id) && d.byId(id)->level == 2 && d.byId(id)->renown == c.byId(id)->renown && d.member("bo")->rank == RankHead &&
               d.byId(id)->rankNames[3] == "Pup",
           "saved and read");
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
    const json::Value& chapter() const { return snapshots.back()["self"]["chapter"]; }
    const json::Value* sees(const std::string& id) const
    {
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

const char* const Lines[] = {
    "\"We have walked this road together for a season now, and I think it is time we gave it a name.\"",
    "\"A name, and a place to meet, and someone to keep the purse when the rest of us are careless.\"",
    "\"Then let it be the Ashen Lodge, for the fire we kept going on the night of the long storm.\"",
    "\"The Ashen Lodge. I like it; it sounds like somewhere a tired wolf would be glad to come home to.\"",
    "\"And we will keep the road safe for the carts, as we have since the bridge went down in spring.\"",
    "\"Agreed, all of it. Let us write it down before somebody thinks better of the whole idea.\"",
};

struct Four
{
    game::Game g;
    Client ada, bo, cy, di;
    std::vector<Client*> all{&ada, &bo, &cy, &di};
    explicit Four(game::Options o) : g(o)
    {
        std::string problem;
        expect(g.start(problem), "starts: " + problem);
        const char* ids[] = {"ada", "bo", "cy", "di"};
        const char* names[] = {"Ada", "Bo", "Cy", "Di"};
        for (int i = 0; i < 4; ++i)
        {
            all[i]->id = std::uint64_t(i + 1);
            g.connect(all[i]);
            g.command(all[i], cmd({{"type", "hello"}, {"id", ids[i]}, {"name", names[i]}}));
        }
        auto* a = g.world().entity(ada.entityId);
        for (int i = 1; i < 4; ++i)
        {
            auto* e = g.world().entity(all[i]->entityId);
            e->cellId = a->cellId;
            e->position = {a->position.x + i, a->position.y};
        }
        tick(.5);
    }
    void tick(double seconds)
    {
        for (double t = 0; t < seconds; t += .05)
        {
            g.tick(.05);
            for (auto* c : all)
                if (!c->snapshots.empty())
                    g.acknowledge(c, c->snapshots.back().number("revision"), false);
        }
    }
};

game::Options options(const std::string& save = {})
{
    game::Options o;
    o.devIdentity = true;
    o.hiddenNames = false;
    o.forkSnapshots = false;
    o.chapterFoundingLevel = 1;
    if (!save.empty())
        o.savePath = save;
    return o;
}

void foundedThroughTheGame(const std::string& save)
{
    std::remove(save.c_str());
    std::string chapterId;
    {
        Four t(options(save));
        // A scene among the three founders: a party of them, two rounds each.
        t.g.command(&t.ada, cmd({{"type", "action"}, {"action", "invite"}, {"target", t.bo.entityId}}));
        t.g.command(&t.bo, cmd({{"type", "party"}, {"verb", "accept"}}));
        t.g.command(&t.ada, cmd({{"type", "action"}, {"action", "invite"}, {"target", t.cy.entityId}}));
        t.g.command(&t.cy, cmd({{"type", "party"}, {"verb", "accept"}}));
        for (int i = 0; i < 6; ++i)
        {
            if (i == 3)
                ::usleep(2100000);
            Client& who = i % 3 == 0 ? t.ada : i % 3 == 1 ? t.bo : t.cy;
            t.g.command(&who, cmd({{"type", "chat"}, {"text", Lines[i]}, {"channel", "party"}}));
            t.tick(.6);
        }
        const auto cash = t.g.world().society().account(t.ada.entityId)->cash;
        auto founders = json::Value::array();
        founders.push(t.bo.entityId);
        founders.push(t.cy.entityId);
        auto o = json::Value::object();
        o.add("type", "chapter");
        o.add("verb", "propose");
        o.add("name", "Ashen Lodge");
        o.add("colour", "#5b8bd9");
        o.add("charter", "We keep the road.");
        o.add("founders", founders);
        t.g.command(&t.ada, json::dump(o));
        t.tick(2.2);
        expect(t.bo.said().find("would found the Chapter \"Ashen Lodge\" with you") != std::string::npos, "Bo is asked:\n" + t.ada.said());
        expect(t.bo.chapter()["proposal"].string("name") == "Ashen Lodge", "and sees the founding waiting");
        t.g.command(&t.bo, cmd({{"type", "chapter"}, {"verb", "agree"}}));
        t.g.command(&t.cy, cmd({{"type", "chapter"}, {"verb", "agree"}}));
        t.tick(2.2);
        expect(t.ada.said().find("The Chapter \"Ashen Lodge\" is founded") != std::string::npos, "founded:\n" + t.ada.said());
        expect(t.g.world().society().account(t.ada.entityId)->cash == cash - chapter::FoundingFee, "Ada paid the two marks");
        const auto& mine = t.ada.chapter();
        chapterId = mine.string("id");
        expect(mine.string("name") == "Ashen Lodge" && mine.number("level") == 1 && mine.string("levelName") == "Gathering" &&
                   mine.number("rank") == chapter::RankHead && mine.array("members").size() == 3,
               "Ada heads a Gathering of three");
        // Out of the party, Bo shows in the Chapter's colour.
        t.g.command(&t.ada, cmd({{"type", "party"}, {"verb", "disband"}}));
        t.tick(.5);
        expect(t.ada.sees(t.bo.entityId)->string("rel") == "chapter" && t.ada.sees(t.bo.entityId)->string("colour") == "#5b8bd9",
               "a Chapter mate in its colour");
        // Di joins as an Initiate.
        t.g.command(&t.bo, cmd({{"type", "action"}, {"action", "invite to chapter"}, {"target", t.di.entityId}}));
        t.g.command(&t.di, cmd({{"type", "chapter"}, {"verb", "accept"}}));
        t.tick(2.2);
        expect(t.di.chapter().number("rank") == chapter::RankInitiate && t.ada.said().find("Di joins the Chapter as an Initiate") != std::string::npos,
               "Di joins as an Initiate");
        // The Chapter's chat, out of character: everyone in it, anywhere.
        t.di.events.clear();
        t.g.command(&t.ada, cmd({{"type", "chat"}, {"text", "Lodge meets at dusk"}, {"channel", "chapterooc"}}));
        bool heard = false;
        for (const auto& e : t.di.events)
            heard |= e.string("channel") == "chapterooc" && e.string("text") == "Lodge meets at dusk";
        expect(heard, "Chapter OOC reaches Di");
        // The treasury: a deposit, and an Initiate can't draw.
        t.g.command(&t.bo, cmd({{"type", "chapter"}, {"verb", "deposit"}, {"amount", 5}}));
        t.g.command(&t.di, cmd({{"type", "chapter"}, {"verb", "withdraw_money"}, {"amount", 1}}));
        t.tick(2.2);
        expect(t.ada.chapter().number("treasury") == 5, "Bo's five pennies in the treasury");
        expect(t.di.said().find("Only Officers and the Head draw") != std::string::npos, "Di can't draw on it");
        // The meeting place, and a wolf marked hostile.
        t.g.command(&t.ada, cmd({{"type", "chapter"}, {"verb", "meet"}}));
        t.g.command(&t.ada, cmd({{"type", "chapter"}, {"verb", "hostile"}, {"target", "npc_scout"}, {"reason", "spies for the Syndicate"}}));
        t.tick(2.2);
        expect(t.ada.chapter()["meeting"].string("name").size() > 0, "a meeting place");
        auto* scout = t.g.world().entity("npc_scout");
        scout->cellId = t.g.world().entity(t.di.entityId)->cellId;
        scout->position = {t.g.world().entity(t.di.entityId)->position.x + 1, t.g.world().entity(t.di.entityId)->position.y};
        t.tick(.5);
        expect(t.di.sees("npc_scout") && t.di.sees("npc_scout")->string("rel") == "hostile" &&
                   t.di.sees("npc_scout")->string("why").find("spies for the Syndicate") != std::string::npos,
               "the marked wolf shows red to every member, with the reason");
        // Renown from the founders' scene, once it settles: a quarter of the best pay (5), +5 for a Chapter scene.
        t.g.command(&t.ada, cmd({{"type", "action"}, {"action", "session_end"}}));
        t.tick(2.2);
        expect(t.ada.chapter().number("renown") == 10, "renown from the scene: " + std::to_string(t.ada.chapter().number("renown")));
        t.g.save();
    }
    {
        Four t(options(save));
        expect(t.di.chapter().string("id") == chapterId && t.di.chapter().number("renown") == 10 && t.di.chapter().array("members").size() == 4,
               "the Chapter survives a restart");
        expect(t.ada.chapter().array("hostiles").size() == 1 && t.ada.chapter().number("treasury") == 5, "its hostile list and treasury too");
    }
    std::remove(save.c_str());
}
} // namespace

int main()
{
    try
    {
        foundingAndRanks();
        renownAndLevels();
        foundedThroughTheGame("/tmp/ratw-chapter-test-" + std::to_string(::getpid()) + ".json");
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
    std::cout << "chapter tests: " << checks << " checks passed\n";
    return 0;
}
