// The ambient director (Core/RatwAmbient.h; Docs/Design/26-living-npcs.md, Phase 10), in Greyfen: which two residents
// talk, about what, when, and what their talk changes; and the authored lines that stand in for the NPC Mind.
#include "RatwClaims.h"
#include "RatwMind.h"
#include "RatwWorld.h"

#include <fstream>
#include <sstream>

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <set>
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
// Greyfen at noon on an ordinary day, clear, with Ada in the street beside Sorrel and Fennel, who stand together.
World scene()
{
    World w;
    const auto loaded = w.loadWorldFile(RATW_SOURCE_DIR "/Data/Worlds/Greyfen/world.ratw");
    expect(loaded.ok, "Town loads: " + loaded.message);
    expect(w.advanceCalendar(1).ok && w.setTimeOfDay(12).ok, "Hearthday noon");
    for (const auto& [id, c] : w.cells())
        if (c.outdoors)
            w.setWeather(id, Weather::Clear);
    w.addPlayer("player-ada", "Ada");
    // Open ground near the middle of the street: a tile, the one east of it and the one south.
    const auto* town = w.cell("town");
    const auto open = [&](int x, int y) { const auto* t = town->tile(x, y); return t && !t->solid; };
    Vec2 spot{-1, -1};
    for (int r = 0; r < 20 && spot.x < 0; ++r)
        for (int y = town->height / 2 - r; y <= town->height / 2 + r && spot.x < 0; ++y)
            for (int x = town->width / 2 - r; x <= town->width / 2 + r && spot.x < 0; ++x)
                if (open(x, y) && open(x + 1, y) && open(x, y + 1))
                    spot = {x + .5, y + .5};
    for (const auto& [id, at] : {std::pair{"player-ada", Vec2{spot.x, spot.y + 1}}, std::pair{"sorrel", spot},
                                 std::pair{"fennel", Vec2{spot.x + 1, spot.y}}})
    {
        auto* e = w.entity(id);
        e->cellId = "town";
        e->position = at;
        e->path.clear();
        e->velocity = {};
        e->offstage = false;
    }
    expect(spot.x >= 0, "They stand on open ground");
    // Everyone else is out of the way: strangers talk too now (doc 30), and these tests are about the two of them.
    for (const auto& [id, e] : w.entities())
        if (e.npc && id != "sorrel" && id != "fennel")
            w.entity(id)->offstage = true;
    return w;
}
const Belief* belief(const World& w, const std::string& holder, const std::string& subject, const std::string& claim)
{
    if (const auto* all = w.beliefsOf(holder))
        for (const auto& b : *all)
            if (b.subject == subject && b.claim == claim)
                return &b;
    return nullptr;
}

void strangersHaveNothingToSay()
{
    auto w = scene();
    // Strangers now make small talk (doc 30): the everyday topics, as strangers.
    const auto first = w.ambientPicks({"player-ada"});
    static const std::set<std::string> everyday{"smalltalk", "lore", "player", "weather", "work"};
    expect(first.size() == 1 && everyday.count(first[0].topic.kind) && first[0].topic.talk.at("pair.band") == "strangers",
           "Two who don't know each other pass the time of day, as strangers (" + (first.empty() ? std::string("nothing") : first[0].topic.kind) + ")");
    expect(first[0].topic.talk.count("here.place") && first[0].topic.talk.count("here.region") && !first[0].topic.talk.at("here.weekday").empty(),
           "with the place, the region and the day for the scenes");
    auto f = scene();
    f.bonds().change("sorrel", "fennel", {25, 10, 50, 0, 0}, f.calendarDays());
    f.bonds().change("fennel", "sorrel", {20, 10, 50, 0, 0}, f.calendarDays());
    const auto picks = f.ambientPicks({"player-ada"});
    expect(picks.size() == 1 && picks[0].topic.talk.at("pair.band") == "friends" && picks[0].cell == "town",
           "Friends pass the time, as friends (" + (picks.empty() ? std::string("nothing") : picks[0].topic.kind + "/" + picks[0].topic.talk.at("pair.band")) + ")");
    expect(w.ambientPicks({}).empty(), "Only where a player can hear");
    expect(w.ambientPicks({"player-ada"}, {"sorrel"}).empty(), "Never with someone busy (talking to a player)");
    w.entity("player-ada")->position = {40.5, 30.5};
    expect(w.ambientPicks({"player-ada"}).empty() || w.hearingClarity("player-ada", "sorrel") >= .35, "Nor out of earshot");
}

void gossipIsPassedOn()
{
    auto w = scene();
    w.bonds().change("sorrel", "fennel", {5, 5, 20, 0, 0}, w.calendarDays());
    w.believe("sorrel", "player-ada", "stole from Wren", "saw it", .9, "inc-1");
    const auto picks = w.ambientPicks({"player-ada"});
    expect(picks.size() == 1 && picks[0].topic.kind == "gossip", "A fresh rumour is the best thing to talk about");
    const auto& pick = picks[0];
    expect(pick.teller == "sorrel" && pick.listener == "fennel" && pick.topic.subject == "player-ada",
           "Whoever knows tells it, even about a player within earshot");
    expect(!pick.topic.facts.empty() && pick.topic.facts[0].find("Sorrel saw it") != std::string::npos, "The facts say how they know");
    w.ambientSpoken(pick);
    const auto* heard = belief(w, "fennel", "player-ada", "stole from Wren");
    expect(heard && heard->source == "sorrel" && std::abs(heard->confidence - .63) < 1e-9 && heard->incident == "inc-1",
           "Fennel believes it now, a little less surely, and from Sorrel");
    bool logged = false;
    for (const auto& e : w.takeEvents())
        logged |= e.kind == "conversation" && e.actor == "sorrel" && e.target == "fennel" && e.detail == "gossip";
    expect(logged, "It was a conversation");
    expect(w.ambientPicks({"player-ada"}).empty(), "The place is quiet for a while after");
    // Three minutes on, the place may talk again, but not these two for ten.
    for (int s = 0; s < 190; ++s)
        w.tick(1);
    for (const auto& p : w.ambientPicks({"player-ada"}))
        expect(p.teller != "sorrel" && p.teller != "fennel" && p.listener != "sorrel" && p.listener != "fennel",
               "Each speaker rests ten minutes");
}

void grudgesAndNews()
{
    auto w = scene();
    w.bonds().change("sorrel", "fennel", {-40, -30, 30, 0, 0}, w.calendarDays());
    auto picks = w.ambientPicks({"player-ada"});
    expect(picks.size() == 1 && picks[0].topic.kind == "quarrel" && picks[0].teller == "sorrel", "Rivals quarrel, the one bearing the grudge first");
    const double before = w.bonds().find("fennel", "sorrel") ? w.bonds().find("fennel", "sorrel")->affinity : 0;
    w.ambientSpoken(picks[0]);
    expect(w.bonds().find("sorrel", "fennel")->affinity < -40 && w.bonds().find("fennel", "sorrel")->affinity < before,
           "and like each other less for it");
    bool quarrel = false;
    for (const auto& e : w.takeEvents())
        quarrel |= e.kind == "quarrel";
    expect(quarrel, "A quarrel is an event of its own");
    auto news = scene();
    news.bonds().change("sorrel", "fennel", {5, 5, 20, 0, 0}, news.calendarDays());
    news.recordEvent({"marriage", "sorrel", "rook", {}, 0, 0, {}, 0, 0, "Rook moves in"});
    picks = news.ambientPicks({"player-ada"});
    expect(picks.size() == 1 && picks[0].topic.kind == "news" && picks[0].teller == "sorrel" &&
               picks[0].topic.facts[0] == "Sorrel married Rook.",
           "News from one's own life is told: " + (picks.empty() ? std::string("none") : picks[0].topic.kind));
}

void theDayItself()
{
    auto w = scene();
    w.bonds().change("sorrel", "fennel", {0, 0, 15, 0, 0}, w.calendarDays());
    const auto ordinary = w.ambientPicks({"player-ada"});
    expect(ordinary.size() == 1 && ordinary[0].topic.kind != "day" && ordinary[0].topic.kind != "festival",
           "On an ordinary day, acquaintances pass the time with the everyday");
    auto f = scene();
    f.bonds().change("sorrel", "fennel", {0, 0, 15, 0, 0}, f.calendarDays());
    expect(f.callFestival("greyfen", "The Lantern Night").ok, "a festival is called");
    const auto picks = f.ambientPicks({"player-ada"});
    expect(picks.size() == 1 && picks[0].topic.kind == "festival", "but a festival is worth remarking on (" +
           (picks.empty() ? std::string("nothing") : picks[0].topic.kind) + ")");
    bool mentioned = picks[0].topic.talk.count("here.festival") && picks[0].topic.talk.at("here.festival") == "The Lantern Night";
    for (const auto& fact : picks[0].topic.facts)
        mentioned |= fact.find("The Lantern Night") != std::string::npos;
    expect(mentioned, "and its name is there for the words");
}

void authoredLines()
{
    mind::ExchangeContext c;
    c.a = {"Sorrel", "", ""};
    c.b = {"Fennel", "", ""};
    for (const auto* kind : {"gossip", "news", "quarrel", "friends", "day"})
    {
        c.kind = kind;
        c.subjectName = "Ada";
        c.claim = "stole from Wren";
        c.news = "Sorrel married Rook";
        c.day = "Quite a feast for the Blossom Fair.";
        const auto x = mind::Client::authoredExchange(c);
        expect(x.lines.size() == 2 && x.lines[0].first == 0 && x.lines[1].first == 1 && !x.generated,
               std::string("Authored lines for ") + kind + ": one each, teller first");
        for (const auto& [who, text] : x.lines)
            expect(!text.empty() && text.size() <= 240, std::string("Short lines: ") + text);
        if (std::string(kind) == "gossip")
            expect(x.lines[0].second.find("Ada stole from Wren") != std::string::npos, "The rumour is told: " + x.lines[0].second);
        if (std::string(kind) == "news")
            expect(x.lines[0].second.find("I married Rook") != std::string::npos, "News in the teller's own words: " + x.lines[0].second);
    }
    mind::Client offline;
    bool got = false;
    offline.exchange(c, [&](const mind::Exchange& x) { got = x.lines.size() == 2 && !x.generated; });
    expect(got, "Without the NPC Mind, the authored lines at once");
}
// Doc 84, Phase 1: claims said in claims.json's words from the teller's point of view, never a raw id, never told to or
// by their victim as gossip; housemates who aren't kin; births and arrivals the town talks of, kept across a restart.
void claimsInTheirOwnWords()
{
    std::ifstream in(RATW_SOURCE_DIR "/Data/Talk/claims.json");
    std::stringstream text;
    text << in.rdbuf();
    std::string problem;
    expect(claims::load(text.str(), problem), "claims.json loads: " + problem);
    expect(claims::kindOfText("tried to steal from Wren") == "theft.attempt" && claims::kindOfText("raids the road at w_1_2") == "bandits.camp",
           "Older saves' free words get their kind");
    expect(claims::rawId("camp_w_ridgemere_6_8 raids") && claims::rawId("at w_ridgemere_6_8") && !claims::rawId("Wren's well-kept rows"),
           "An id is told from words");
    // A camp the carters spoke of: a road topic in the claim's own words, never its ids.
    auto w = scene();
    w.bonds().change("sorrel", "fennel", {5, 5, 20, 0, 0}, w.calendarDays());
    std::string wild;
    for (const auto& [id, c] : w.cells())
        if (c.outdoors && !c.name.empty() && wild.empty())
            wild = id;
    expect(!wild.empty(), "Greyfen has somewhere under the sky");
    w.believe("sorrel", "camp_" + wild, "raids the road at " + wild, "the carters", .9);
    const auto* camp = belief(w, "sorrel", "camp_" + wild, "raids the road at " + wild);
    expect(camp && camp->kind == "bandits.camp" && camp->place == wild, "The belief knows its kind and place from its words");
    auto picks = w.ambientPicks({"player-ada"});
    expect(picks.size() == 1 && picks[0].topic.kind == "bandits" && picks[0].topic.talk.count("topic.said") &&
               !claims::rawId(picks[0].topic.talk.at("topic.said")) && picks[0].topic.talk.at("topic.kind") == "camp",
           "A camp is talked of on the road's topic, in words (" + (picks.empty() ? std::string("nothing") : picks[0].topic.kind) + ")");
    for (const auto& [k, v] : picks[0].topic.talk)
        expect(!claims::rawId(v) || k == "topic.good", "No fact a line can say holds an id: " + k + " = " + v);
    w.ambientSpoken(picks[0]);
    const auto* told = belief(w, "fennel", "camp_" + wild, "raids the road at " + wild);
    expect(told && told->kind == "bandits.camp" && told->place == wild && told->source == "sorrel", "Told on, the structure goes with it");
    // Never to the victim as gossip, and never by them: the one robbed tells it as their own news.
    auto v = scene();
    v.bonds().change("sorrel", "fennel", {5, 5, 20, 0, 0}, v.calendarDays());
    Belief theft;
    theft.holder = "sorrel";
    theft.subject = "player-ada";
    theft.claim = "stole from someone";
    theft.source = "saw it";
    theft.confidence = .9;
    theft.object = "fennel";
    v.believe(theft);
    for (const auto& p : v.ambientPicks({"player-ada"}))
        expect(p.topic.kind != "gossip", "Nobody is told their own misfortune as gossip");
    auto by = scene();
    by.bonds().change("sorrel", "fennel", {5, 5, 20, 0, 0}, by.calendarDays());
    theft.holder = "fennel";
    theft.object = "fennel";
    by.believe(theft);
    for (const auto& p : by.ambientPicks({"player-ada"}))
        expect(p.topic.kind != "gossip", "Nor does the victim tell it of themselves as gossip");
    // A theft named with its victim: the realization, not the free words, and the kind the answers key on.
    auto named = scene();
    named.bonds().change("sorrel", "fennel", {5, 5, 20, 0, 0}, named.calendarDays());
    Belief attempt = theft;
    attempt.holder = "sorrel";
    attempt.claim = "tried to steal from Wren";
    attempt.object.clear();
    named.believe(attempt);
    picks = named.ambientPicks({"player-ada"});
    expect(picks.size() == 1 && picks[0].topic.kind == "gossip" && picks[0].topic.talk.at("topic.claim_kind") == "theft" &&
               picks[0].topic.talk.at("topic.said").find("tried to steal from Wren") != std::string::npos,
           "Gossip keeps its kind for the answers (" + (picks.empty() ? std::string("nothing") : picks[0].topic.kind) + ")");
    // Births and arrivals are the town's news, and the news survives a restart.
    auto n = scene();
    WorldEvent born;
    born.kind = "birth";
    born.actor = "fennel";
    born.target = "sorrel";
    born.cell = "town";
    n.recordEvent(born);
    std::map<std::string, std::string> facts;
    n.talkTown("sorrel", facts);
    expect(facts["world.birth"] == "recent", "A birth is talked of in town (" + facts["world.birth"] + ")");
    WorldEvent mourning;
    mourning.kind = "marriage";
    mourning.actor = "sorrel";
    mourning.target = "fennel";
    mourning.cell = "town";
    n.recordEvent(mourning);
    const auto saved = n.save();
    expect(!saved.townNews.empty() && !saved.news.empty(), "The town's news and a resident's own are saved");
    World again;
    expect(again.loadWorldFile(RATW_SOURCE_DIR "/Data/Worlds/Greyfen/world.ratw").ok && again.restore(saved).ok, "and restored");
    std::map<std::string, std::string> after;
    again.talkTown("sorrel", after);
    expect(after["world.birth"] == "recent" && after["world.wedding"] == "recent", "with the birth and the wedding still news");
    claims::clear();
}

void housematesAreNotKin()
{
    auto w = scene();
    // Two who share a roof without a kin tie (the user's defect 3: the watch house's 105 pairs were "family").
    const auto& residents = w.society().state().residents;
    std::string a, b;
    for (const auto& [x, rx] : residents)
        for (const auto& [y, ry] : residents)
            if (a.empty() && x < y && !rx.homeCell.empty() && rx.homeCell == ry.homeCell && !w.society().family(x, y))
                a = x, b = y;
    if (a.empty())
        return;                                     // (Greyfen may house no such pair.)
    expect(w.pairBand(a, b) == "housemates", "Housemates who aren't kin are housemates, not family");
}
} // namespace

int main()
{
    try
    {
        strangersHaveNothingToSay();
        gossipIsPassedOn();
        grudgesAndNews();
        theDayItself();
        authoredLines();
        claimsInTheirOwnWords();
        housematesAreNotKin();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
    std::cout << "ambient tests passed (" << checks << " checks)\n";
    return 0;
}
