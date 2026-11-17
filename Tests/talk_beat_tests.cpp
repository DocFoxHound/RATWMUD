// The ambient director a beat at a time (Docs/Design/84 §8.2-8.10, Phase 4), in a game: the synthetic talk town
// (Tests/fixtures/talk_town), a listener nobody plays, the director's own looks. Each line is chosen when it comes due;
// a claim passes to the one told only once the line telling it is said (and to whoever stands by and hears it, less
// surely); one being talked of who stands by cuts in; rain starting on them breaks the talk off with a line that says so,
// and the talk is kept as a thread; a talk leaves its people a little more familiar, and its farewell warmer.
#include "RatwCalendar.h"
#include "RatwGame.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unistd.h>
#include <vector>

using namespace ratw;
namespace fs = std::filesystem;

namespace
{
int checks = 0;
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

struct Town
{
    std::unique_ptr<game::Game> g;
    std::string listener = "listen_1", cell;
    std::vector<game::Game::AmbientLine> lines;
    std::vector<game::Game::AmbientEnd> ends;
    std::string save;

    Town()
    {
        game::Options o;
        o.talkData = RATW_SOURCE_DIR "/Data/Talk";
        o.voiceData = RATW_SOURCE_DIR "/Data/Voice";
        o.scratch = true;
        o.forkSnapshots = false;
        save = (fs::temp_directory_path() / ("ratw-talk-beats-" + std::to_string(::getpid()) + ".json")).string();
        o.savePath = save;
        o.worldExport = RATW_SOURCE_DIR "/Tests/fixtures/talk_town";
        g = std::make_unique<game::Game>(o);
        g->log = [](const char*, const std::string&) {};
        std::string problem;
        expect(g->start(problem), "the talk town starts: " + problem);
        for (long i = 0, n = long(2 * calendar::SecondsPerDay / 24 / .05); i < n; ++i)
            g->tick(.05);                           // (It wakes at home: two game hours to be about.)
        g->onAmbientLine = [this](const game::Game::AmbientLine& l) { lines.push_back(l); };
        g->onAmbientEnd = [this](const game::Game::AmbientEnd& e) { ends.push_back(e); };
    }
    ~Town()
    {
        std::error_code ec;
        fs::remove(save, ec);
    }
    World& w() { return g->world(); }
    // Everyone stands where they are (the frozen world), the listener among them.
    void freeze()
    {
        w().restage();
        std::vector<std::tuple<std::string, std::string, Vec2, bool>> all;
        for (const auto& [id, e] : w().entities())
            if (e.npc && !e.transient)
                all.push_back({id, e.cellId, e.position, e.offstage});
        for (const auto& [id, c, at, off] : all)
            w().replayPlace(id, c, at, false, "", off);
    }
    void run(double seconds)
    {
        for (double t = 0; t < seconds; t += .25)
        {
            w().advanceClock(.25);
            g->ambient(.25);
        }
    }
    // Run until a talk between these two has ended (or the time is up); its end.
    const game::Game::AmbientEnd* until(const std::string& a, const std::string& b, double seconds)
    {
        const auto before = ends.size();
        for (double t = 0; t < seconds; t += .25)
        {
            w().advanceClock(.25);
            g->ambient(.25);
            for (std::size_t i = before; i < ends.size(); ++i)
                if ((ends[i].teller == a && ends[i].listener == b) || (ends[i].teller == b && ends[i].listener == a))
                    return &ends[i];
        }
        return nullptr;
    }
};

const Belief* belief(World& w, const std::string& holder, const std::string& subject)
{
    if (const auto* all = w.beliefsOf(holder))
        for (const auto& b : *all)
            if (b.subject == subject)
                return &b;
    return nullptr;
}

void aBeatAtATime()
{
    Town t;
    auto& w = t.w();
    // Two who stand free together in the busiest place, and the listener among them.
    std::map<std::string, std::vector<const Entity*>> by;
    for (const auto& [id, e] : w.entities())
        if (e.npc && !e.dead && !e.transient && e.path.empty() && w.society().resident(id) && w.placeKindOf(e.cellId) != "home")
            by[e.cellId].push_back(&e);
    std::string cell;
    for (const auto& [c, es] : by)
        if (cell.empty() || es.size() > by[cell].size())
            cell = c;
    expect(!cell.empty() && by[cell].size() >= 4, "a place with a few standing about");
    auto& here = by[cell];
    // The teller, the one told, one standing by (who hears), and the one talked of, all within a few tiles.
    const std::string teller = here[0]->id, told = here[1]->id, near = here[2]->id, subject = here[3]->id;
    const Vec2 at = here[0]->position;
    std::vector<std::pair<std::string, Vec2>> spots{{teller, at}, {told, {at.x + 1.5, at.y}}, {near, {at.x + 1, at.y + 2}},
                                                    {subject, {at.x + .7, at.y + 1.2}}};
    for (const auto& [id, p] : spots)
        w.replayPlace(id, cell, p, false, "", false);
    // Everyone else in the place stands well away, so this is the talk the director picks.
    double away = 0;
    for (const auto* e : here)
        if (std::none_of(spots.begin(), spots.end(), [&](const auto& s) { return s.first == e->id; }))
            w.replayPlace(e->id, cell, {std::fmod(at.x + 30 + (away += 1.3), 40.0), std::fmod(at.y + 20, 30.0)}, false, "", false);
    w.addPlayer(t.listener, "Listener", cell, {at.x, at.y + 3});
    t.freeze();
    for (const auto& [id, p] : spots)
        w.replayPlace(id, cell, p, false, "", false);
    // The teller has it that the subject stole from the one standing by; the subject is known to the one told.
    w.believe(teller, subject, "stole from " + w.entity(near)->name, "saw it", .9);
    w.bonds().change(told, subject, {0, 0, 5, 0, 0}, w.calendarDays());
    w.bonds().change(teller, subject, {0, 0, 5, 0, 0}, w.calendarDays());
    w.ambientPacing().clear();
    auto timing = std::make_shared<game::Game::TalkTiming>();
    t.g->setTalkTiming(timing);
    t.g->setTalkListeners({t.listener});
    const auto* end = t.until(teller, told, 600);
    expect(end != nullptr, "the two talk");
    expect(end->topic == "gossip" && end->teller == teller, "of what the teller has heard: " + end->topic);
    // Each line was chosen as it came due, and timed (B1); the first as the talk began (a look).
    std::vector<game::Game::AmbientLine> said;
    for (const auto& l : t.lines)
        if (l.conv == end->conv)
            said.push_back(l);
    expect(said.size() >= 2 && said.front().who == teller && said.front().to == told, "the teller opens, to the one told");
    expect(timing->beats.size() + 1 >= said.size() && !timing->looks.empty(), "every later line a beat, timed");
    // The claim passed once it was said, as sure as the teller is, by how far the one told trusts them; the one standing
    // by heard it too, less surely.
    const auto* heard = belief(w, told, subject);
    expect(heard && heard->source == teller, "the one told believes it now, from the teller");
    const auto* trust = w.bonds().find(told, teller);
    const double factor = std::clamp(.5 + .4 * (trust ? trust->trust : 0) / 100, .1, .9);
    expect(std::abs(heard->confidence - .9 * factor) < .02 || heard->confidence > .9 * factor,
           "as surely as the teller, by trust: " + std::to_string(heard->confidence));
    const auto* overheard = belief(w, near, subject);
    expect(!overheard || overheard->source == teller, "one standing by heard it from the teller");
    // The one talked of stood by: they cut in (§8.9), and the talk had three in it.
    const bool joined = std::find(end->people.begin(), end->people.end(), subject) != end->people.end();
    expect(joined && end->stake == "named" && end->people.size() == 3,
           "the one talked of, standing right there, cuts in (" + end->stake + ", " + std::to_string(end->people.size()) + ")");
    bool spoke = false;
    for (const auto& l : said)
        spoke |= l.who == subject && l.info.moves.size() && l.info.moves.front() == "join.object";
    expect(spoke, "with a line that objects");
    // Talking left them more familiar (ambientEnded), the farewell warmer (moves.json's effects).
    const auto* f = w.bonds().find(told, teller);
    expect(f && f->familiarity >= 1, "the two know each other a little better");
    expect(end->ended == "farewell" || end->ended == "curt", "and it closed: " + end->ended);
    // The talk ledger (§8.7, Phase 5): the one told was told it (never again: K3), the place counts it today, and the two
    // (and the one who cut in) have a note of it: its topic and what about, how it went.
    const auto key = subject + "|stole from " + w.entity(near)->name;
    auto& ledger = w.talkLedger();
    expect(ledger.wasTold(told, key) && ledger.toldBy(told, teller, key), "the one told was told it, by the teller");
    expect(ledger.todayCount(cell, key, w.calendarDays()) >= 1, "the place counts it today (saturation)");
    const auto* note = ledger.latest(teller, told, true);
    expect(note && !note->topics.empty() && note->topics.front() == "gossip" && note->keys.front() == key && !note->outcome.empty(),
           "the two have a note of it: " + (note ? note->outcome : std::string("none")));
    expect(ledger.latest(teller, subject, true) != nullptr, "and the one who cut in, with each of them");
    for (const auto& t2 : w.agendaFor(teller, told))
        expect(!(t2.kind == "gossip" && t2.key == key), "never told the one told again");
}

void pledgesAndCarry()
{
    // Doc 84 §8.7 (Phase 5): an invitation is kept when both are in its place in its hour, missed when its days run out; a
    // talk-only promise lapses; the ledger goes with the checkpoint and with what the simulator carries over a restart.
    Town t;
    auto& w = t.w();
    std::string a, b, cell;
    for (const auto& [id, e] : w.entities())
        if (e.npc && !e.dead && !e.transient && w.society().resident(id))
        {
            if (a.empty())
                a = id, cell = e.cellId;
            else if (b.empty() && e.cellId == cell)
                b = id;
        }
    expect(!a.empty() && !b.empty(), "two in one place");
    t.freeze();
    auto& ledger = w.talkLedger();
    const double day = w.calendarDays();
    const int hour = int(std::floor(calendar::calendarAt(day + 6 / calendar::SecondsPerDay).hour));   // (The hour of the look.)
    ledger.invites().push_back({a, b, cell, "here", hour, day - 1, day + 1, "open"});
    ledger.invites().push_back({b, a, cell, "here", (hour + 12) % 24, day - 3, day - .5, "open"});
    ledger.promises().push_back({b, a, "healer", day - 3, day - 1, "open"});
    t.run(6);
    expect(ledger.invites()[0].state == "kept", "both in its place in its hour: kept (" + ledger.invites()[0].state + ")");
    expect(ledger.invites()[1].state == "missed", "its days run out: missed");
    expect(ledger.promises()[0].state == "lapsed", "a promise past its days: lapsed");
    const auto* inv = ledger.lastInvite(a, b);
    expect(inv && (inv->state == "kept" || inv->state == "missed"), "pair.kept_invite reads the latest checked");
    // The ledger in the checkpoint, and carried over the simulator's restarts.
    ledger.told(a, b, "k|carried", "theft", day);
    expect(t.g->checkpointText().find("\"talkLedger\"") != std::string::npos, "saved in the checkpoint");
    const auto carried = t.g->talkCarry();
    ledger.clear();
    expect(!ledger.wasTold(a, "k|carried"), "cleared");
    t.g->talkResume(carried);
    expect(w.talkLedger().wasTold(a, "k|carried") && w.talkLedger().promises().size() == 1, "carried over a restart");
}

void brokenOffByRain()
{
    Town t;
    auto& w = t.w();
    // A pair standing together in the open, and rain coming on them mid-talk.
    std::map<std::string, std::vector<const Entity*>> by;
    for (const auto& [id, e] : w.entities())
        if (const auto* c = w.cell(e.cellId); c && c->outdoors && e.npc && !e.dead && !e.transient && w.society().resident(id))
            by[e.cellId].push_back(&e);
    std::string cell;
    for (const auto& [c, es] : by)
        if (cell.empty() || es.size() > by[cell].size())
            cell = c;
    expect(!cell.empty() && by[cell].size() >= 2, "an open place with two in it");
    const auto a = by[cell][0]->id, b = by[cell][1]->id;
    const Vec2 at = by[cell][0]->position;
    w.setWeather(cell, Weather::Clear);
    std::size_t moved = 0;
    for (const auto* e : by[cell])
        if (e->id != a && e->id != b)
            w.replayPlace(e->id, cell, {std::fmod(at.x + 25 + double(++moved) * 1.3, 40.0), std::fmod(at.y + 20, 30.0)}, false, "", false);
    w.addPlayer(t.listener, "Listener", cell, {at.x, at.y + 2});
    t.freeze();
    w.replayPlace(a, cell, at, false, "", false);
    w.replayPlace(b, cell, {at.x + 1.5, at.y}, false, "", false);
    w.ambientPacing().clear();
    t.g->setTalkListeners({t.listener});
    // As soon as their first line is said, it rains.
    bool raining = false;
    t.g->onAmbientLine = [&](const game::Game::AmbientLine& l) {
        t.lines.push_back(l);
        if (!raining && (l.who == a || l.who == b))
            w.setWeather(cell, Weather::Rain), raining = true;
    };
    const auto* end = t.until(a, b, 600);
    expect(end != nullptr && raining, "the two talk, and it rains");
    expect(end->ended == "break" && end->marked, "the rain breaks it off with a line that says so: " + end->ended);
    std::string last;
    for (const auto& l : t.lines)
        if (l.conv == end->conv)
            last = l.info.moves.empty() ? "" : l.info.moves.front();
    expect(last == "break", "its last line breaks off");
}
} // namespace

int main()
{
    try
    {
        aBeatAtATime();
        brokenOffByRain();
        pledgesAndCarry();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Talk beat tests passed: " << checks << " checks.\n";
    return 0;
}
