// The ambient director (Core/RatwAmbient.h; Docs/Design/26-living-npcs.md, Phase 10), in Greyfen: which two residents
// talk, about what, when, and what their talk changes; and the authored lines that stand in for the NPC Mind.
#include "RatwClaims.h"
#include "RatwMind.h"
#include "RatwTopics.h"
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
    // A sentence after a question starts with a capital, though the news or a traveller's look begins in lower case
    // ("Did you hear? A stranger robbed me.": the slice-end review).
    int asked = 0;
    for (const auto* who : {"Sorrel", "Fennel", "Rook", "Linden", "Maple", "Birch", "Wren", "Harrow"})
        for (const auto* kind : {"gossip", "news"})
        {
            c.a = {who, "", ""};
            c.kind = kind;
            c.subjectName = "a grey stranger";
            c.news = std::string("a stranger robbed ") + who;
            const auto x = mind::Client::authoredExchange(c);
            const auto& line = x.lines[0].second;
            if (const auto q = line.find("? "); q != std::string::npos)
            {
                ++asked;
                expect(q + 2 < line.size() && !(line[q + 2] >= 'a' && line[q + 2] <= 'z'), "A capital after the question: " + line);
            }
        }
    expect(asked > 0, "Some of them asked first");
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
    w.roads().camps.push_back({"camp_" + wild, wild, 4, 30, -100, true});
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

// The slice-end review of doc 84's Slice 1: news never told to the one it is about, a theft the victim's news only once
// noticed and its doer named only if seen, the town's news never by or to the subject's own, a cleared camp no longer
// "camped out by...", and the town's crime naming nobody nobody saw.
std::string pickSaid(World& w)
{
    const auto picks = w.ambientPicks({"player-ada"});
    return picks.empty() ? std::string() : picks[0].topic.kind + "|" + picks[0].topic.said;
}
void sliceEndReview()
{
    std::ifstream in(RATW_SOURCE_DIR "/Data/Talk/claims.json");
    std::stringstream text;
    text << in.rdbuf();
    std::string problem;
    expect(claims::load(text.str(), problem), "claims.json loads: " + problem);
    // "I married Fennel" isn't news to Fennel.
    auto own = scene();
    own.recordEvent({"marriage", "sorrel", "fennel", "town", 0, 0, {}, 0, 0, ""});
    for (const auto& p : own.ambientPicks({"player-ada"}))
        expect(!(p.topic.kind == "news" && p.topic.said.find("married") != std::string::npos),
               "One's own news isn't told to the one it is about: " + p.topic.said);
    // A theft is the victim's news only once noticed (witness() calls victimNews), and names the doer only if seen.
    auto v = scene();
    v.recordEvent({"theft", "player-ada", "sorrel", "town", 0, 0, {}, 0, 3, "inc-x"});
    expect(v.save().news["sorrel"].empty(), "A theft recorded, not yet noticed, is nobody's news");
    Incident inc;
    inc.id = "inc-x";
    inc.kind = "theft";
    inc.offender = "player-ada";
    inc.victim = "sorrel";
    inc.cell = "town";
    v.victimNews(inc, false);
    auto news = v.save().news["sorrel"];
    expect(news.size() == 1 && news[0].text == "someone robbed Sorrel" && news[0].about.empty(),
           "Robbed by someone they couldn't make out: nobody named (" + (news.empty() ? std::string() : news[0].text) + ")");
    v.setNamer([](const std::string&, const std::string&) { return std::string(); });
    v.victimNews(inc, true);
    news = v.save().news["sorrel"];
    expect(news.back().text == "a stranger robbed Sorrel" && news.back().about == "player-ada",
           "Seen, a traveller whose name they weren't given is a stranger: " + news.back().text);
    auto thief = scene();
    inc.offender = "fennel";
    thief.victimNews(inc, true);
    expect(thief.save().news["sorrel"].back().text == "Fennel robbed Sorrel", "A resident seen at it is named");
    for (const auto& p : thief.ambientPicks({"player-ada"}))
        expect(!(p.topic.kind == "news" && p.topic.said == "Fennel robbed Sorrel"), "but the victim doesn't tell the thief");
    // The town's news (a death here: it outscores Ada standing by, as a birth doesn't), never by or to the subject's own
    // spouse; births and arrivals go the same way (sceneTopics).
    const auto died = [](World& w) {
        WorldEvent e;
        e.kind = "mourning";
        e.actor = "harrow";                         // (Birch and Harrow keep the barracks; Sorrel and Fennel live elsewhere.)
        e.target = "birch";
        e.cell = "town";
        w.recordEvent(e);
        const auto picks = w.ambientPicks({"player-ada"});
        return !picks.empty() && picks[0].topic.kind == "life" && picks[0].topic.talk.count("topic.kind") &&
               picks[0].topic.talk.at("topic.kind") == "death";
    };
    auto town = scene();
    expect(died(town), "Birch's death is the town's news (" + pickSaid(town) + ")");
    auto kin = scene();
    expect(kin.society().marry("sorrel", "birch"), "Sorrel marries Birch");
    expect(!died(kin), "but not told by or to his wife as the town's news (" + pickSaid(kin) + ")");
    // A cleared camp isn't "camped out by..." any more.
    auto c = scene();
    std::string wild;
    for (const auto& [id, cell] : c.cells())
        if (cell.outdoors && !cell.name.empty() && wild.empty())
            wild = id;
    c.bonds().change("sorrel", "fennel", {5, 5, 20, 0, 0}, c.calendarDays());
    c.roads().camps.push_back({"camp_" + wild, wild, 4, 30, -100, true});
    c.believe("sorrel", "camp_" + wild, "raids the road at " + wild, "the carters", .9);
    expect(pickSaid(c).rfind("bandits|", 0) == 0, "A camp the carters spoke of is talked of (" + pickSaid(c) + ")");
    c.roads().camps.back().active = false;
    expect(pickSaid(c).rfind("bandits|", 0) != 0, "and not once it is cleared (" + pickSaid(c) + ")");
    // The town's crime names the one who did it only if a witness saw who, a traveller as the teller knows them.
    auto crime = scene();
    Incident beat;
    beat.id = "inc-y";
    beat.kind = "assault";
    beat.offender = "player-ada";
    beat.victim = "rook";
    beat.cell = "town";
    beat.town = crime.townIdOf("town");
    beat.day = crime.calendarDays();
    beat.witnesses.push_back({"linden", false, .5, false});
    crime.crime().incidents.push_back(beat);
    const auto subject = [&] {
        const auto picks = crime.ambientPicks({"player-ada"});
        return picks.empty() || picks[0].topic.kind != "crime" ? std::string("(no crime topic)") : picks[0].topic.talk.at("topic.subject");
    };
    expect(subject() == "someone", "Only heard, not seen: someone (" + subject() + ")");
    crime.crime().incidents.back().witnesses[0].identified = true;
    crime.setNamer([](const std::string&, const std::string&) { return std::string(); });
    expect(subject() != "someone" && subject() != "Ada" && subject() != "(no crime topic)",
           "Seen, by a teller not given the name: a description (" + subject() + ")");
    crime.setNamer([](const std::string&, const std::string&) { return std::string("Ada"); });
    expect(subject() == "Ada", "and the name, to one who was given it (" + subject() + ")");
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

void talkCarriesKnowledge()
{
    // Doc 84 §8.6 (Phase 4): a claim told in talk is corroborated by a second, independent source, (1 - c) * 0.4, once
    // a source; not by the one they had it from, nor by one who had it from them. A denial by the one it's about weakens
    // it. And a talk spoken a beat at a time counts from its start (ambientBegun) and leaves its people more familiar at
    // its end (ambientEnded), with no blanket warmth: its lines carry their own.
    auto w = scene();
    w.believe("sorrel", "player-ada", "stole from Wren", "saw it", .5, "inc-1");
    const auto* held = w.holding("sorrel", "player-ada", "stole from Wren");
    expect(held && held->confidence == .5, "held as believed");
    expect(w.holding("sorrel", "player-ada", "something else", "inc-1") == held, "found by its incident too");
    Belief told;
    told.holder = "sorrel";
    told.subject = "player-ada";
    told.claim = "stole from Wren";
    told.confidence = .3;
    told.source = "saw it";
    w.told(told, "");
    expect(std::abs(w.holding("sorrel", "player-ada", "stole from Wren")->confidence - .5) < 1e-9, "the same source again: no surer");
    told.source = "fennel";
    w.told(told, "sorrel");
    expect(std::abs(w.holding("sorrel", "player-ada", "stole from Wren")->confidence - .5) < 1e-9,
           "one who had it from them: no surer");
    told.source = "bram";
    w.told(told, "someone");
    expect(std::abs(w.holding("sorrel", "player-ada", "stole from Wren")->confidence - .7) < 1e-9,
           "a second, independent source: .5 + .5 * .4");
    w.told(told, "someone");
    expect(std::abs(w.holding("sorrel", "player-ada", "stole from Wren")->confidence - .7) < 1e-9, "once a source");
    told.holder = "fennel";
    w.told(told, "someone");
    expect(std::abs(w.holding("fennel", "player-ada", "stole from Wren")->confidence - .3) < 1e-9, "new to the hearer: believed as told");
    w.doubted("sorrel", "player-ada", "stole from Wren", .5);
    expect(std::abs(w.holding("sorrel", "player-ada", "stole from Wren")->confidence - .35) < 1e-9, "a denial weakens it");
    w.doubted("sorrel", "player-ada", "stole from Wren", 3);
    expect(std::abs(w.holding("sorrel", "player-ada", "stole from Wren")->confidence - .35) < 1e-9, "never strengthens it");
    // Begun and ended.
    const auto before = w.bonds().find("fennel", "sorrel") ? *w.bonds().find("fennel", "sorrel") : Bond{};
    AmbientPick pick;
    pick.teller = "sorrel";
    pick.listener = "fennel";
    pick.cell = "town";
    pick.topic.kind = "day";
    w.ambientBegun(pick);
    expect(w.ambientPacing().count("sorrel") && w.ambientPacing().count("cell:town"), "it counts from its start");
    w.ambientEnded(pick, {"sorrel", "fennel"});
    const auto* after = w.bonds().find("fennel", "sorrel");
    expect(after && after->familiarity > before.familiarity && after->affinity < before.affinity + .5,
           "a little more familiar, warmer only as any conversation is (the event's own), never the old blanket (" + std::to_string(before.familiarity) + " -> " +
               std::to_string(after ? after->familiarity : -1) + ", " + std::to_string(before.affinity) + " -> " +
               std::to_string(after ? after->affinity : -1) + ")");
}


void theTalkLedger()
{
    // Doc 84 §8.7 (Phase 5): the talk ledger's records, their caps and its save; and what the director does with it
    // (§8.5): nothing told a listener twice, a pair's topic cooled, an incident talked out in a place, the everyday floor,
    // close pairs talking more; and daily gossip out of hearing writes notes with no words.
    {
        TalkLedger l;
        for (int i = 0; i < 10; ++i)
        {
            auto& n = l.note("a", "b", 1 + i * .1);
            n.topics = {i % 2 ? "weather" : "prices"};
            n.keys = {i % 2 ? "weather" : "price|bread"};
            n.outcome = i == 9 ? "warmed" : "even";
        }
        expect(l.notes("b", "a") && l.notes("a", "b")->size() == TalkLedger::NotesPerPair, "eight notes a pair, either way round");
        expect(l.latest("a", "b") && l.latest("a", "b")->outcome == "warmed" && l.recall("a", "b", "prices") &&
                   l.recall("a", "b", "prices")->day < 1.85,
               "the latest note, and the latest on a topic");
        expect(l.talkedOf("a", "b", "prices", "price|bread", 1.5) && !l.talkedOf("a", "b", "prices", "price|salt", 1.5) &&
                   !l.talkedOf("a", "b", "prices", "price|bread", 1.9) && l.talkedOf("a", "b", "weather", "", 1.5),
               "talked of a thing since a day (an everyday topic by its name alone)");
        auto& open = l.note("a", "b", 2);
        open.thread = "bandits";
        open.threadUntil = 4;
        expect(l.thread("b", "a", 3) == "bandits" && l.thread("a", "b", 4.5).empty(), "a thread, kept its days");
        l.closeThread("a", "b");
        expect(l.thread("a", "b", 3).empty(), "a thread taken up is done with");
        auto& unheard = l.note("a", "b", 2.5);
        unheard.unheard = true;
        expect(l.latest("a", "b", true) == &open && l.latest("a", "b") == &unheard, "notes out of hearing kept apart for pair.last_talk");
        l.told("b", "a", "x|stole", "theft", 2);
        l.told("b", "a", "x|stole", "theft", 2.2);
        expect(l.wasTold("b", "x|stole") && !l.wasTold("a", "x|stole") && l.toldBy("b", "a", "x|stole") && !l.counted("b", "a", "x|stole") &&
                   l.toldMe("b", "a") == "theft",
               "the told set: who told whom what, once a teller");
        l.told("b", "a", "x|stole", "theft", 2.3, true);
        expect(l.counted("b", "a", "x|stole"), "counted as a second source");
        for (int i = 0; i < 260; ++i)
            l.told("c", "d" + std::to_string(i), "k" + std::to_string(i), "", 2);
        expect(!l.wasTold("c", "k0") && l.wasTold("c", "k259"), "256 a listener, the oldest dropped");
        l.said("x", "took_up", "fiddle", "a", 10);
        l.said("x", "moved", "yes", "a", 1);
        auto said = l.saidOf("x", 5);
        expect(said.size() == 1 && said["took_up"] == "fiddle", "what talk said of a third party, until its day");
        l.keep("a", "asked", "favour", 4);
        expect(l.kept("a", 3)["asked"] == "favour" && l.kept("a", 5).empty(), "a speaker's kept tag, until its day");
        open.kept["carry"] = {"declined", 9};
        expect(l.pairKept("a", "b", 3)["carry"] == "declined", "the pair's kept tags (pair.was)");
        for (int i = 0; i < 3; ++i)
            l.placeTalk("square", "prices", "price|bread", true, 5.2);
        l.placeTalk("square", "weather", "weather", false, 5.3);
        expect(l.todayCount("square", "price|bread", 5.9) == 3 && l.todayCount("square", "price|bread", 6.1) == 0 &&
                   std::abs(l.newsShare("square", 8) - .75) < 1e-9 && l.newsShare("nowhere", 8) < 0,
               "a place's day and its last talks");
        l.invites().push_back({"a", "b", "square", "the square", 19, 2, 4, "kept"});
        l.promises().push_back({"b", "a", "healer", 2, 4, "lapsed"});
        expect(l.lastInvite("b", "a") && l.lastPromise("b", "a") && l.lastPromise("b", "a")->thing == "healer" && !l.lastPromise("a", "b"),
               "invitations and promises");
        // Phase 6 (§7.6, §8.8): stances drifted by events, each event once, never past two steps; mood events, 8 a resident.
        l.drift("a", "watch", -1, "robbed_uncaught", "inc-1", 3);
        l.drift("a", "watch", -1, "robbed_uncaught", "inc-1", 3.5);
        expect(l.drifts("a").at("watch") == std::pair<int, std::string>{-1, "robbed_uncaught"} && l.applied("a", "inc-1|watch"),
               "an event drifts a stance a step, once");
        l.drift("a", "watch", -1, "robbed_uncaught", "inc-2", 4);
        l.drift("a", "watch", -1, "kin_charged", "warrant:x", 4);
        l.drift("a", "watch", 1, "robbed_caught", "inc-3", 5);
        expect(l.drifts("a").at("watch") == std::pair<int, std::string>{-1, "robbed_caught"}, "never past two steps either way");
        for (int i = 0; i < 10; ++i)
            l.moodEvent("a", i % 2 ? "quarrel" : "praised", 3 + i * .1);
        expect(l.moodEvents("a", 0).size() == TalkLedger::MoodsPerResident && l.moodEvents("a", 3.75).size() == 2, "mood events, 8 a resident, since a day");
        const auto saved = l.save();
        TalkLedger again;
        again.load(saved);
        expect(json::dump(again.save()) == json::dump(saved), "saved and loaded, the same");
        expect(again.drifts("a") == l.drifts("a") && again.applied("a", "inc-1|watch") && again.moodEvents("a", 0).size() == 8,
               "drift, the events applied and moods come back");
        l.prune(40);
        expect(l.moodEvents("a", 0).empty() && !l.drifts("a").empty(), "moods fade in a week; drift is for good");
        expect(!l.notes("a", "b") && l.saidOf("x", 40).empty() && l.lastInvite("a", "b") == nullptr, "past 30 days: gone");
        TalkLedger big;
        for (int i = 0; i < 600; ++i)
            for (int j = 0; j < 64; ++j)
                big.told("r" + std::to_string(i), "t", "k" + std::to_string(j), "", 1 + i * .001);
        big.prune(2);
        expect(big.size() <= TalkLedger::Records && big.wasTold("r599", "k63") && !big.wasTold("r0", "k63"),
               "past 30,000 records, the least recently used go first (" + std::to_string(big.size()) + ")");
        TalkLedger off;
        off.enabled = false;
        off.told("b", "a", "k", "", 1);
        off.said("x", "k", "v", "a", 5);
        expect(!off.wasTold("b", "k") && off.saidOf("x", 1).empty(), "off (--no-talk-ledger): nothing written");
    }
    // Saved with the world.
    {
        auto w = scene();
        w.talkLedger().told("fennel", "sorrel", "k", "theft", w.calendarDays());
        const auto saved = w.save();
        World back;
        expect(back.loadWorldFile(RATW_SOURCE_DIR "/Data/Worlds/Greyfen/world.ratw").ok && back.restore(saved).ok, "restored");
        expect(back.talkLedger().wasTold("fennel", "k"), "the ledger comes back with the world");
    }
    // The director, with the live topics.json (tuned: told once, cooled, the floor, close pairs).
    std::ifstream in(RATW_SOURCE_DIR "/Data/Talk/topics.json");
    std::stringstream text;
    text << in.rdbuf();
    std::string problem;
    expect(topics::load(text.str(), problem), "topics.json loads: " + problem);
    {
        auto w = scene();
        w.believe("sorrel", "player-ada", "stole from Wren", "saw it", .9, "inc-1");
        const auto gossip = [&] {
            for (const auto& t : w.agendaFor("sorrel", "fennel"))
                if (t.kind == "gossip")
                    return t.score;
            return -1.0;
        };
        const double fresh = gossip();
        const auto band = topics::book().pairBonus.find(w.pairBand("sorrel", "fennel"));
        const double close = band == topics::book().pairBonus.end() ? 0 : band->second;   // (Added last: strangers -0.2.)
        expect(fresh > 3 && w.ambientTopic("sorrel", "fennel").kind == "gossip", "a fresh rumour first");
        const int per = topics::book().saturationPerDay;
        const double factor = topics::book().saturationFactor;
        for (int i = 1; i < per; ++i)
            w.talkLedger().placeTalk("town", "gossip", "player-ada|stole from Wren", true, w.calendarDays());
        expect(per > 1 && std::abs(gossip() - fresh) < 1e-9, "talked of here today, not yet as often as saturates");
        w.talkLedger().placeTalk("town", "gossip", "player-ada|stole from Wren", true, w.calendarDays());
        expect(std::abs(gossip() - ((fresh - close) * factor + close)) < 1e-6, "talked of so often here today: saturated");
        for (int i = 0; i < 8; ++i)
            w.talkLedger().placeTalk("town", "crime", "incident|" + std::to_string(i), true, w.calendarDays());
        double smalltalk = -1;
        for (const auto& t : w.agendaFor("sorrel", "fennel"))
            if (t.kind == "smalltalk")
                smalltalk = t.score;
        expect(smalltalk > 2.2, "the place's last eight talks all news: everyday talk gets the floor (" + std::to_string(smalltalk) + ")");
        auto& note = w.talkLedger().note("sorrel", "fennel", w.calendarDays());
        note.topics = {"smalltalk"};
        note.keys = {"smalltalk"};
        double cooled = -1;
        for (const auto& t : w.agendaFor("sorrel", "fennel"))
            if (t.kind == "smalltalk")
                cooled = t.score;
        const double boost = topics::book().floorBoost;
        expect(std::abs(cooled - ((smalltalk - boost - close) * .4 + boost + close)) < 1e-6, "the two on it lately: x 0.4, the floor after");
        w.talkLedger().told("fennel", "sorrel", "player-ada|stole from Wren", "theft", w.calendarDays());
        expect(gossip() < 0, "told once (the told set), never told again: K3");
        topics::reset();
        expect(gossip() > 0, "today's numbers: no told set");
        expect(topics::load(text.str(), problem), "again");
        w.bonds().change("sorrel", "fennel", {30, 10, 50, 0, 0}, w.calendarDays());
        w.bonds().change("fennel", "sorrel", {30, 10, 50, 0, 0}, w.calendarDays());
        double lore = -1;
        for (const auto& t : w.agendaFor("sorrel", "fennel"))
            if (t.kind == "lore")
                lore = t.score;
        expect(w.pairBand("sorrel", "fennel") == "friends" && lore >= .7 + topics::book().floorBoost + .6 - 1e-9, "friends talk more (+0.6): " + std::to_string(lore));
    }
    // Out of hearing: the daily gossip writes notes with no words, and the told set; off, nothing.
    for (const bool enabled : {true, false})
    {
        auto w = scene();
        w.talkLedger().enabled = enabled;
        w.bonds().change("sorrel", "fennel", {5, 5, 40, 0, 0}, w.calendarDays());
        w.believe("sorrel", "player-ada", "stole from Wren", "saw it", .9, "inc-1");
        const double day = std::floor(w.calendarDays());
        for (int s = 0; s < 4000 && std::floor(w.calendarDays()) < day + 1.5; ++s)
            w.tick(5);
        const auto* note = w.talkLedger().latest("sorrel", "fennel");
        if (enabled)
            expect(note && note->unheard && !note->claims.empty() && w.talkLedger().wasTold("fennel", "player-ada|stole from Wren"),
                   "talk out of hearing: a note with no words, and the told set");
        else
            expect(!note && !w.talkLedger().wasTold("fennel", "player-ada|stole from Wren"), "off: nothing written");
    }
    topics::reset();
}


// Doc 84 §35 (Slice 2's review): the ledger keeps what was heard (a daily note out of hearing never takes a heard one's
// place, nor makes a set look used), the dead take their drift with them; the town's crime is told each wolf once; a
// resident's own row says where they're from, for the director as for their lines.
void sliceTwoReview()
{
    {
        TalkLedger l;
        auto& heard = l.note("a", "b", 1);
        heard.outcome = "soured";
        heard.souredBy = "a";
        for (int d = 2; d <= 25; ++d)
        {
            auto& n = l.note("b", "a", d, true);
            n.topics = {"gossip"};
            n.keys = {"x|stole"};
        }
        int unheard = 0;
        for (const auto& n : *l.notes("a", "b"))
            unheard += n.unheard;
        const auto* last = l.latest("a", "b", true);
        expect(last && last->day == 1 && last->souredBy == "a" && unheard == int(TalkLedger::UnheardPerPair),
               "24 days of gossip out of hearing: the heard note is kept, beside the last 2 days of it (" + std::to_string(unheard) + ")");
        TalkLedger back;
        back.load(l.save());
        expect(back.latest("a", "b", true) && back.latest("a", "b", true)->souredBy == "a", "who soured it, saved and loaded");
        // Past the cap: the notes out of hearing first; a set heard talk used is kept over sets only gossip wrote since.
        TalkLedger big;
        big.told("heard", "t", "k", "", 1);
        for (int i = 0; i < 1600; ++i)
            for (int j = 0; j < 19; ++j)
                big.told("r" + std::to_string(i), "t", "k" + std::to_string(j), "", 2 + i * .001, false, false);
        for (int i = 0; i < 2000; ++i)
            big.note("p" + std::to_string(i), "q", 3, true).keys = {"x"};
        expect(big.size() > TalkLedger::Records, "the fixture is past the cap (" + std::to_string(big.size()) + ")");
        big.prune(3.5);
        expect(big.size() <= TalkLedger::Records && big.wasTold("heard", "k") && !big.notes("p0", "q"),
               "past the cap: gossip's notes go first, and what was heard stays (" + std::to_string(big.size()) + ")");
        // The dead take their drift, its events and their moods with them.
        TalkLedger d;
        d.drift("gone", "watch", -1, "robbed_uncaught", "inc-1", 3);
        d.drift("here", "watch", -1, "robbed_uncaught", "inc-1", 3);
        d.moodEvent("gone", "quarrel", 3);
        d.prune(4, [](const std::string& id) { return id == "gone"; });
        expect(d.drifts("gone").empty() && !d.applied("gone", "inc-1|watch") && d.moodEvents("gone", 0).empty() &&
                   !d.drifts("here").empty(),
               "the dead's drift goes; the living's stays");
    }
    std::ifstream in(RATW_SOURCE_DIR "/Data/Talk/topics.json");
    std::stringstream text;
    text << in.rdbuf();
    std::string problem;
    expect(topics::load(text.str(), problem), "topics.json loads: " + problem);
    {
        // The town's crime, once to each wolf.
        auto w = scene();
        Incident inc;
        inc.id = "inc-z";
        inc.kind = "theft";
        inc.offender = "rook";
        inc.victim = "linden";
        inc.cell = "town";
        inc.town = w.townIdOf("town");
        inc.day = w.calendarDays();
        w.crime().incidents.push_back(inc);
        const auto crime = [&]() -> const AmbientTopic* {
            static AmbientTopic found;
            for (const auto& t : w.agendaFor("sorrel", "fennel"))
                if (t.key == "incident|inc-z")
                    return &(found = t);
            return nullptr;
        };
        expect(crime() != nullptr, "a theft in town is news between the two");
        w.talkLedger().told("fennel", "sorrel", "incident|inc-z", "", w.calendarDays());
        expect(crime() == nullptr, "told it by the other, never told again (nor told back)");
        auto w2 = scene();
        w2.crime().incidents.push_back(inc);
        w2.talkLedger().told("fennel", "birch", "incident|inc-z", "", w2.calendarDays());
        bool offered = false;
        for (const auto& t : w2.agendaFor("sorrel", "fennel"))
            if (t.key == "incident|inc-z")
                offered = true, expect(t.source == "fennel", "the one who has heard it tells the one who hasn't (" + t.source + ")");
        expect(offered, "one has heard it: still news to the other");
        w2.talkLedger().told("sorrel", "birch", "incident|inc-z", "", w2.calendarDays());
        for (const auto& t : w2.agendaFor("sorrel", "fennel"))
            expect(t.key != "incident|inc-z", "both have heard it: no news between them");
        // One who said they're going home talks no more there for the hour.
        auto w3 = scene();
        expect(!w3.ambientPicks({"player-ada"}).empty(), "the two talk");
        w3.ambientLeave("sorrel", 3600);
        expect(w3.ambientPicks({"player-ada"}).empty(), "not once Sorrel has said she's off home");
        // A resident's own origin, as their row says.
        std::string from, accent;
        w2.setTalkOrigins({{"sorrel", {"coldwater", "broad"}}});
        w2.talkOrigin("sorrel", from, accent);
        expect(from == "coldwater" && accent == "broad", "the director's origin is the resident's own row's (" + from + ", " + accent + ")");
    }
    topics::reset();
}

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
        talkCarriesKnowledge();
        theTalkLedger();
        sliceEndReview();
        sliceTwoReview();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
    std::cout << "ambient tests passed (" << checks << " checks)\n";
    return 0;
}
