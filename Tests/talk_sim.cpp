// talk_sim (Docs/Design/84-living-conversations.md §15.1): the offline conversation simulator. It builds a scratch
// game::Game as talk_check --world does (nothing is ever written), places listeners nobody plays where the profile says
// (World::addPlayer), and runs the game's own ambient director over them (Game::ambient: ambientSpeak and ambientStart,
// the server's pair choice, pacing, breaks and heard sets), writing what is said as transcripts:
//
//   OUT/lines.jsonl     one line spoken a line: who, to whom, the words, the rule or scene and its alternative, the
//                       grammar layers that filled it, the topic, the place, the listener it was picked for...
//   OUT/convs.jsonl     one exchange a line: its people, topic, route, how it ended and why, lines said of written
//   OUT/residents.jsonl the residents heard, with the facts the library sees of them (dialect, accent, traits, job)
//   OUT/run.json        how it was run: mode, world, profile, listeners and their places, seed, hours, wall time
//
//   talk_sim --world KEY|EXPORT_DIR [--frozen | --fixtures DIR | --tick] [--profile NAME] [--seed N] [--hours H]
//            [--exchanges N] [--out DIR] [--run NAME] [--checkpoint FILE] [--library-from REF] [--talk-data DIR]
//            [--warm GAME_HOURS] [--fixtures DIR --freeze-at DAY] [--ablate shuffle] [--golden FILE [--bless]]
//   talk_sim --world KEY --tick --days D --record DIR [--profile none]
//
// Modes. --frozen (the default): the world doesn't tick; everyone stands where the save (or --checkpoint FILE) had
// them, the clock and calendar move on (World::advanceClock) and the director looks every five seconds as the server's
// does (--warm H: the world run H game hours first, then frozen: a synthetic town wakes at home). Who is near a
// listener is on stage, as the server puts residents near a player; the rest live offstage and don't talk.
// --fixtures DIR: talk replayed over a recorded ticking run (DIR from --record): its checkpoint every game midnight (a
// new scratch game from each, the listeners' heard sets, the talk memory and the director's pacing carried over:
// Game::talkCarry) and, every 120 world seconds between them, where everyone was and what they were doing
// (World::replayPlace, Society::replayTask); the world is otherwise frozen between steps. --tick: the world really runs
// (a simulation: Docs/Design/84 §21, Q6 allows only the runs the plan names); --record DIR writes the fixture as it goes.
// --fixtures DIR --freeze-at DAY: frozen, from the recorded world as it was at that game day (29.5: noon of day 29), so
// a frozen run starts from a moment that doesn't change as DEV's save does.
//
// Profiles (§15.2): L1 the 4 busiest places of Ridgemere, a listener in each; L2 a wanderer, a new busy place every game
// hour; L3-L9 a listener in each town's busiest place; L10 a regular, in Ridgemere's busiest tavern from noon to
// midnight (two real hours) every game day for 30 game days. "town" (talk_town's three places, one listener each) and
// "none" (no listener: a recording run). L1-L9 listen 100 hours (360,000 world seconds) unless --hours says otherwise.
//
// --ablate shuffle also writes the shuffled control (§15.5): the same conversations with every line after the first
// replaced by a line from another conversation of the same topic, said at the same side (opener or answerer) by a
// speaker of the same dialect where one can be found: lines-shuffled.jsonl and convs-shuffled.jsonl.
// --library-from REF: the talk library as git had it at REF (the old-against-new packet; the goldens' frozen library).
// --golden FILE compares the transcript's speech (conversation, beat, who, entry, alternative, words) with FILE and
// exits 1 at the first difference; with --bless it writes FILE instead.
#include "RatwCalendar.h"
#include "RatwGame.h"
#include "RatwTalk.h"
#include "RatwTopics.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <sstream>
#include <unistd.h>

using namespace ratw;
namespace fs = std::filesystem;

namespace
{
constexpr double HourSeconds = calendar::SecondsPerDay / 24;   // A game hour, in world seconds.
constexpr double TrackEvery = 120;                             // World seconds between a fixture's track steps.
constexpr double CheckpointEveryDays = 1;                      // A fixture's checkpoints: every game midnight.

std::string shellQuote(const std::string& s)
{
    std::string out = "'";
    for (const char c : s)
        out += c == '\'' ? std::string("'\\''") : std::string(1, c);
    return out + "'";
}
std::string run(const std::string& command)
{
    std::string out;
    if (FILE* p = ::popen(command.c_str(), "r"))
    {
        char buffer[4096];
        for (std::size_t n; (n = std::fread(buffer, 1, sizeof buffer, p)) > 0;)
            out.append(buffer, n);
        if (::pclose(p) != 0)
            return "";
    }
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
        out.pop_back();
    return out;
}
std::string worldDb(const std::string& arguments)
{
    return run("python3 " + shellQuote(RATW_SOURCE_DIR "/tools/world_db.py") + " " + arguments);
}
std::uint64_t fnv(const std::string& s)
{
    std::uint64_t h = 1469598103934665603ULL;
    for (const unsigned char c : s)
        h = (h ^ c) * 1099511628211ULL;
    return h;
}
std::string hex(std::uint64_t h)
{
    char b[17];
    std::snprintf(b, sizeof b, "%08llx", static_cast<unsigned long long>(h & 0xffffffffULL));
    return b;
}
// A file's bytes hashed (FNV-1a, 64 bits, as 16 hex digits; "" if it can't be read): a fixture's checkpoints and track are
// written with theirs and checked before they're replayed, so a partial or damaged copy is never replayed as the real one.
std::string fileFnv(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return {};
    std::uint64_t h = 1469598103934665603ULL;
    char buffer[65536];
    while (in.read(buffer, sizeof buffer) || in.gcount() > 0)
    {
        for (std::streamsize i = 0; i < in.gcount(); ++i)
            h = (h ^ static_cast<unsigned char>(buffer[i])) * 1099511628211ULL;
        if (!in)
            break;
    }
    char b[17];
    std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(h));
    return b;
}
double hourOf(double days) { return std::fmod(days, 1.0) * 24; }
const char* weatherWord(Weather w)
{
    switch (w)
    {
    case Weather::Clear: return "clear";
    case Weather::Rain: return "rain";
    case Weather::Fog: return "fog";
    case Weather::Snow: return "snow";
    case Weather::Overcast: return "overcast";
    case Weather::Storm: return "storm";
    case Weather::Sandstorm: return "sandstorm";
    }
    return "clear";
}

struct Args
{
    std::string world, mode = "frozen", fixtures, profile = "L1", out, runName, checkpoint, libraryFrom, talkData, golden,
                record, ablate, places, dumpTopics, topics;
    std::uint64_t seed = 1;
    bool dumpTopicsAll = false;
    double hours = -1, days = 25, warm = 0, freezeAt = 0;
    long exchanges = -1;
    bool bless = false;
};

// One listener nobody plays: where it stands (or whether it's away), and for whom the profile moves it.
struct Listener
{
    std::string id, cell;
    Vec2 at{};
    bool present = true;
};

// What a place is like for the director: how many stand about there free to talk, and where they stand on average.
struct Crowd
{
    double free = 0, x = 0, y = 0;
    double paired = 0;                      // Of them, those with another within talking distance (3 tiles).
    long seen = 0;
};

class Sim
{
  public:
    explicit Sim(Args a) : args_(std::move(a)) {}
    ~Sim()
    {
        // (The library copied out of git for --library-from, and the scratch save's path: nothing left in /tmp.)
        g_.reset();
        std::error_code ec;
        if (!libraryDir_.empty())
            fs::remove_all(libraryDir_, ec);
        fs::remove(savePath(), ec);
    }
    int main();

  private:
    Args args_;
    std::unique_ptr<game::Game> g_;
    std::string talkData_, problem_;
    fs::path libraryDir_;
    std::int64_t fixtureBuild_ = 0;                          // The world build a fixture was recorded on (0: newest).
    static std::string savePath() { return "/tmp/ratw-talk-sim-" + std::to_string(::getpid()) + ".json"; }
    bool checkFixture(const json::Value& manifest);
    std::vector<Listener> listeners_;
    std::ofstream lines_, convs_;
    // --dump-topics (doc 84, Phase 5): every topic the director weighs, hashed a step at a time (or every one, with
    // --dump-topics-all), so two builds' directors can be compared exactly.
    std::ofstream topics_;
    std::uint64_t topicHash_ = 1469598103934665603ULL;
    long topicCount_ = 0;
    void flushTopics(double t);
    std::mt19937_64 rng_;
    long exchanges_ = 0, lineCount_ = 0;
    double startTime_ = 0, endTime_ = 0, startDay_ = 0;
    std::string profile_;
    // Transcript state: each exchange's lines as spoken, its facts, the residents heard.
    struct Line
    {
        json::Value v;
        int side = 0;                       // 0: the opener's, 1: the other's.
    };
    struct Conv
    {
        json::Value head;
        std::vector<Line> lines;
        std::string topic, opener, answerer, dialectOf0, dialectOf1;
    };
    std::map<std::uint64_t, Conv> open_;                     // By the run's own exchange number.
    std::map<std::uint64_t, std::uint64_t> convIds_;         // The game's exchange id -> the run's number.
    std::uint64_t nextConv_ = 1;
    std::vector<Conv> done_;
    std::map<std::string, json::Value> residents_;          // id -> facts (at first hearing)
    std::map<std::string, std::map<std::string, long>> heardBy_;   // id -> listener -> lines
    std::map<std::string, std::string> cellPlace_, cellRegion_;
    std::shared_ptr<game::Game::TalkTiming> timing_ = std::make_shared<game::Game::TalkTiming>();   // (B1: every game's.)
    // Fixture replay.
    struct Checkpoint
    {
        double t = 0, day = 0;
        std::string file;
    };
    std::vector<Checkpoint> checkpoints_;
    std::size_t nextCheckpoint_ = 0;
    std::vector<std::string> trackIds_, trackCells_, trackTasks_, trackPostures_;
    struct Place
    {
        int cell = -1;
        float x = 0, y = 0;
        int flags = 0, task = -1, posture = -1;
    };
    std::vector<Place> trackState_;
    FILE* track_ = nullptr;
    double trackNextT_ = -1;
    std::string trackPending_;

    bool makeGame(const std::string& checkpoint);
    bool prepareLibrary();
    void hook();
    json::Value speakerFacts(const std::string& id);
    void placeListeners();
    void placeListener(Listener& l, const std::string& cell, Vec2 near);
    void profileStep(double t);
    std::map<std::string, Crowd> crowds(bool overTrack);
    Vec2 openSpot(const std::string& cell, Vec2 near);
    void settleEveryone();
    std::string placeKind(const std::string& cell);
    std::map<std::string, std::string> kinds_;
    std::set<std::string> homes_;
    void stage();
    std::set<std::string> stage_;
    bool openTrack();
    void advanceTrack(double t);
    void applyTrackState();
    bool restartAt(const Checkpoint& c);
    int recordRun();
    void writeShuffled();
    int compareGolden();
    void writeResidents(double wall);
    std::vector<std::string> presentIds() const;
};

bool Sim::checkFixture(const json::Value& m)
{
    // Every file the fixture names is the one it recorded (fnv64; a fixture from before the hashes has none to check),
    // and the world build it was recorded on is the one it replays on (Options::worldBuild; doc 84 §15.1).
    std::vector<std::pair<std::string, std::string>> files;
    for (const auto& c : m.array("checkpoints"))
        files.push_back({c.string("file"), c.string("fnv64")});
    if (const auto& t = m["track"]; t.isObject())
        files.push_back({t.string("file"), t.string("fnv64")});
    else
        files.push_back({"track.jsonl.gz", ""});
    for (const auto& [file, want] : files)
    {
        const auto path = (fs::path(args_.fixtures) / file).string();
        if (!fs::is_regular_file(path))
        {
            problem_ = "the fixture's " + file + " is missing from " + args_.fixtures;
            return false;
        }
        if (!want.empty())
            if (const auto got = fileFnv(path); got != want)
            {
                problem_ = "the fixture's " + file + " isn't the one recorded (fnv64 " + got + ", recorded " + want +
                           "): a partial or damaged copy";
                return false;
            }
    }
    fixtureBuild_ = std::int64_t(m.number("build"));
    if (fixtureBuild_ <= 0)
        std::cerr << "talk_sim: the fixture names no world build; it replays on the newest, whatever that is now\n";
    return true;
}

bool Sim::prepareLibrary()
{
    talkData_ = args_.talkData.empty() ? RATW_SOURCE_DIR "/Data/Talk" : args_.talkData;
    if (args_.libraryFrom.empty())
        return true;
    // The library as git had it at REF, in a directory of its own for this run.
    const auto dir = fs::temp_directory_path() / ("ratw-talk-lib-" + std::to_string(::getpid()));
    fs::create_directories(dir);
    libraryDir_ = dir;                              // (Removed when the run ends: ~Sim.)
    const auto command = "git -C " + shellQuote(RATW_SOURCE_DIR) + " archive " + shellQuote(args_.libraryFrom) +
                         " Data/Talk | tar -x -C " + shellQuote(dir.string()) + " && echo ok";
    if (run(command) != "ok" || !fs::is_directory(dir / "Data" / "Talk"))
    {
        problem_ = "the talk library at " + args_.libraryFrom + " can't be read from git";
        return false;
    }
    talkData_ = (dir / "Data" / "Talk").string();
    // The engine's own data that came after it (doc 84, Phase 1: the claims a belief is said in, the truth lexicon), from
    // the source tree, so an older library's gossip is still said and checked as the engine now says it.
    std::error_code ec;
    for (const char* own : {"claims.json", "truth.json"})
        if (!fs::exists(dir / "Data" / "Talk" / own) && fs::exists(fs::path(RATW_SOURCE_DIR "/Data/Talk") / own))
            fs::copy_file(fs::path(RATW_SOURCE_DIR "/Data/Talk") / own, dir / "Data" / "Talk" / own, ec);
    return true;
}

bool Sim::makeGame(const std::string& checkpoint)
{
    g_.reset();
    game::Options o;
    o.talkData = talkData_;
    o.voiceData = RATW_SOURCE_DIR "/Data/Voice";
    o.scratch = true;
    o.forkSnapshots = false;
    o.savePath = savePath();
    o.checkpointFile = checkpoint;
    o.worldBuild = fixtureBuild_;                   // (A fixture replays on the build it was recorded on.)
    if (!fs::is_directory(args_.world))
    {
        const auto key = worldDb("resolve " + shellQuote(args_.world));
        if (key.empty())
        {
            problem_ = "'" + args_.world + "' is neither an export directory nor a registered world";
            return false;
        }
        o.database = key;
        if (const char* url = std::getenv("RATW_DATABASE_URL"))
            o.conninfo = url;
        else
            o.conninfo = worldDb("conninfo " + key + " --role game");
    }
    else
        o.worldExport = args_.world;
    g_ = std::make_unique<game::Game>(o);
    g_->log = [](const char* level, const std::string& text) {
        if (std::string(level) != "info")
            std::cerr << "[" << level << "] " << text << "\n";
    };
    if (!g_->start(problem_))
        return false;
    g_->setTalkTiming(timing_);
    if (g_->talkLibrarySize() == 0)
    {
        problem_ = "the talk library at " + talkData_ + " didn't load";
        return false;
    }
    if (!args_.topics.empty())
    {
        // Another topics.json over the library's (doc 84, Phase 5: today's numbers, for ctest talk_topics).
        std::ifstream in(args_.topics);
        std::stringstream text;
        text << in.rdbuf();
        std::string why;
        if (!in || !topics::load(text.str(), why))
        {
            problem_ = "--topics " + args_.topics + ": " + (why.empty() ? std::string("can't be read") : why);
            return false;
        }
    }
    hook();
    return true;
}

json::Value Sim::speakerFacts(const std::string& id)
{
    if (const auto found = residents_.find(id); found != residents_.end())
        return found->second;
    auto facts = json::Value::object();
    if (g_->world().entity(id))
    {
        const auto q = g_->talkQuery(id, {}, "bark");
        for (const auto& [k, v] : q.a.facts)
            facts.add(k, v);
        facts.add("_dialect", g_->talkLibrary().dialectOf(q.a.facts.count("from") ? q.a.facts.at("from") : std::string()));
        auto layers = json::Value::array();
        for (const auto& l : q.a.grammar)
            layers.push(l);
        layers.push("plain");
        facts.add("_layers", layers);
        if (const auto* life = g_->world().society().resident(id))
            facts.add("_home", g_->world().townIdOf(life->homeCell));
        if (const auto* spec = g_->world().society().spec(id))
            facts.add("_work_label", spec->workLabel);
    }
    residents_[id] = facts;
    return facts;
}

void Sim::flushTopics(double t)
{
    if (!topicCount_)
        return;
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.3f %ld %016llx\n", t - startTime_, topicCount_, static_cast<unsigned long long>(topicHash_));
    topics_ << buf;
    topicHash_ = 1469598103934665603ULL;
    topicCount_ = 0;
}

void Sim::hook()
{
    if (topics_.is_open())
        g_->world().onTopic = [this](const std::string& a, const std::string& b, const AmbientTopic& t) {
            char score[40];
            std::snprintf(score, sizeof score, "%.17g", t.score);
            std::string line = a + "|" + b + "|" + t.kind + "|" + score + "|" + t.source + "|" + t.subject + "|" + t.claim + "|" +
                               t.said + "|" + t.incident + "|" + (t.passes ? "p" : "") + t.claimKind + "|" + t.object + "|" + t.seen;
            for (const auto& [k, v] : t.talk)
                if (k != "topic.subject_sex" && k != "topic.victim_sex")   // (Phase 6's pronouns: no part of a topic's weight.)
                    line += "|" + k + "=" + v;
            for (const auto& f : t.facts)
                line += "|" + f;
            for (const unsigned char c : line)
                topicHash_ = (topicHash_ ^ c) * 1099511628211ULL;
            topicHash_ = (topicHash_ ^ '\n') * 1099511628211ULL;
            ++topicCount_;
            if (args_.dumpTopicsAll)
                topics_ << line << "\n";
        };
    g_->onAmbientLine = [this](const game::Game::AmbientLine& l) {
        auto& w = g_->world();
        // The run's own number for the exchange: a game's ids start again in each fixture snapshot's new world.
        auto mapped = convIds_.find(l.conv);
        if (l.beat == 0 || mapped == convIds_.end())
            mapped = convIds_.insert_or_assign(l.conv, nextConv_++).first;
        const auto conv = mapped->second;
        auto& c = open_[conv];
        if (l.beat == 0)
        {
            // The exchange's facts once, at its first line: the place and moment, the pair, the topic.
            const auto q = g_->talkQuery(l.who, l.to, "talk.smalltalk");
            auto head = json::Value::object();
            head.add("run", args_.runName);
            head.add("conv", double(conv));
            head.add("t", w.time() - startTime_);
            head.add("day", w.calendarDays());
            head.add("hour", hourOf(w.calendarDays()));
            head.add("cell", l.cell);
            head.add("listener", l.hearer);
            head.add("topic", l.topic);
            head.add("claim", l.claim);
            head.add("subject", l.subject);
            head.add("route", l.route);
            head.add("teller", l.who);
            head.add("other", l.to);
            if (!l.seen.empty())
                head.add("seen", l.seen);       // (The teller saw it, or only heard: T3's eyewitness words.)
            auto facts = json::Value::object();
            for (const auto& [k, v] : q.facts)
                if (k.rfind("here.", 0) == 0 || k.rfind("pair.", 0) == 0 || k.rfind("world.", 0) == 0)   // (pair.kin too: T2.)
                    facts.add(k, v);
            if (const auto* cell = w.cell(l.cell))
            {
                facts.set("_outdoors", cell->outdoors ? "yes" : "no");
                if (const auto* e = w.entity(l.who))
                    facts.set("_weather", weatherWord(w.weatherAt(l.cell, e->position).kind));
            }
            head.add("facts", facts);
            cellPlace_[l.cell] = q.facts.count("here.place") ? q.facts.at("here.place") : "";
            cellRegion_[l.cell] = q.facts.count("here.region") ? q.facts.at("here.region") : "";
            c.head = head;
            c.topic = l.topic;
            c.opener = l.who;
            ++exchanges_;
        }
        const auto who = speakerFacts(l.who);
        speakerFacts(l.to);
        auto v = json::Value::object();
        v.add("run", args_.runName);
        v.add("t", w.time() - startTime_);
        v.add("day", w.calendarDays());
        v.add("cell", l.cell);
        v.add("place", cellPlace_[l.cell]);
        v.add("region", cellRegion_[l.cell]);
        v.add("listener", l.hearer);
        v.add("conv", double(conv));
        v.add("beat", l.beat);
        v.add("who", l.who);
        v.add("to", l.to);
        v.add("entry", l.info.entry);
        v.add("alt", l.info.alt);
        // What the line does in the talk (doc 84 §15.1, Phase 2): its moves, what it makes due (owes; invites, soft),
        // which of the line before it it answers, its tone, the claims it passes on or denies, the gist, its tags.
        const auto list = [](const std::vector<std::string>& xs) {
            auto a = json::Value::array();
            for (const auto& x : xs)
                a.push(x);
            return a;
        };
        v.add("moves", list(l.info.moves));
        v.add("owes", list(l.info.owes));
        v.add("invites", list(l.info.invites));
        v.add("answers", list(l.info.answers));
        v.add("topic", l.topic);
        v.add("hour", hourOf(w.calendarDays()));   // (Its own moment: T2 judges a line by when it was said, Phase 5.)
        // Phase 5 (§8.5, §8.7): the topic the line was said on (a talk may turn: C5), whether it stands on the talk
        // ledger (a callback: C9), the goal it is a step of.
        if (!l.info.topic.empty())
            v.add("topic_now", l.info.topic);
        if (l.info.callback)
            v.add("callback", true);
        if (!l.info.goal.empty())
            v.add("goal", l.info.goal);
        v.add("depth", 0);
        v.add("tone", l.info.tone);
        if (l.info.best >= 0)
        {
            v.add("fit", l.info.fit);               // (Phase 3: how well it answered what was owed, and the best that held: C2.)
            v.add("best", l.info.best);
        }
        v.add("asserts", list(l.info.asserts));
        if (!l.info.denies.empty())
            v.add("denies", list(l.info.denies));
        if (!l.knew.empty())
            v.add("to_knew", l.knew);               // (Phase 4: the one spoken to held the topic's claim before it began: T5.)
        if (l.info.gist)
            v.add("gist", true);
        if (!l.info.tags.empty())
        {
            auto tags = json::Value::object();
            for (const auto& [k, x] : l.info.tags)
                tags.add(k, x);
            v.add("tags", tags);
        }
        v.add("dialect", who.string("_dialect"));
        v.add("accent", who.string("accent"));
        // Phase 6: the accent the line was spoken in (accommodation: the scorecard's D1), the stance it aired (T4), and the
        // topic's claim kind and kind as it was said (T2's pity and gladness).
        if (!l.info.accent.empty())
            v.add("spoken_accent", l.info.accent);
        if (!l.info.issue.empty())
        {
            v.add("issue", l.info.issue);
            v.add("stance", l.info.stance);
            v.add("drift", l.info.drift);
            v.add("basis", l.info.basis);
        }
        if (!l.claimKind.empty())
            v.add("claim_kind", l.claimKind);
        if (!l.topicKind.empty())
            v.add("topic_kind", l.topicKind);
        auto layers = json::Value::array();
        for (const auto& x : who.array("_layers"))
            layers.push(x);
        v.add("layers", layers);
        auto used = json::Value::array();
        for (const auto& x : l.info.layers)
            used.push(x);
        v.add("used", used);
        auto habits = json::Value::array();
        for (const auto& x : l.info.habits)
            habits.push(x);
        v.add("habits", habits);
        v.add("facts", hex(fnv(json::dump(who))));
        v.add("route", l.route);
        v.add("text", l.text);
        // (The opener's side, the other's, or a third who joined: the control swaps lines within a side.)
        c.lines.push_back({v, l.who == c.opener ? 0 : (c.answerer.empty() || l.who == c.answerer) ? 1 : 2});
        if (l.who != c.opener && c.answerer.empty())
            c.answerer = l.who;
        ++heardBy_[l.who][l.hearer];
        lines_ << json::dump(v) << '\n';
        ++lineCount_;
    };
    g_->onAmbientEnd = [this](const game::Game::AmbientEnd& e) {
        const auto mapped = convIds_.find(e.conv);
        if (mapped == convIds_.end())
            return;                                 // (Nothing of it was said: begun, then broken off before a word.)
        auto found = open_.find(mapped->second);
        convIds_.erase(mapped);
        if (found == open_.end())
            return;
        auto& c = found->second;
        auto head = c.head;
        head.add("reason", e.reason);
        head.add("spoken", e.spoken);
        head.add("written", e.written);
        if (!e.ended.empty())
            head.add("ended", e.ended);             // (How the turn engine ended it: farewell, cap, nothing, curt...)
        if (e.marked)
            head.add("marked", true);               // (Phase 4: broken off with a line that says so: C3's marked break.)
        if (!e.stake.empty())
            head.add("stake", e.stake);             // (Phase 4: why a third joined: C8.)
        if (e.dropped)
            head.add("dropped", e.dropped);         // (Phase 4: a scene's turns dropped when they no longer held.)
        head.add("lines", double(c.lines.size()));
        auto chain = json::Value::array();
        for (const auto& id : e.chain)
            chain.push(id);
        head.add("chain", chain);
        head.add("t_end", g_->world().time() - startTime_);
        auto people = json::Value::array();
        std::set<std::string> seen;
        for (const auto& l : c.lines)
            if (seen.insert(l.v.string("who")).second)
                people.push(l.v.string("who"));
        for (const auto& p : e.people)              // (Everyone in it, a third who joined last, said a word or not.)
            if (seen.insert(p).second)
                people.push(p);
        head.add("people", people);
        c.head = head;
        convs_ << json::dump(head) << '\n';
        done_.push_back(std::move(c));
        open_.erase(found);
    };
}

std::vector<std::string> Sim::presentIds() const
{
    std::vector<std::string> out;
    for (const auto& l : listeners_)
        if (l.present)
            out.push_back(l.id);
    return out;
}

Vec2 Sim::openSpot(const std::string& cellId, Vec2 near)
{
    auto& w = g_->world();
    w.ensureLoaded(cellId);
    const auto* c = w.cell(cellId);
    if (!c)
        return near;
    // The nearest open tile to `near` (a crowd's middle), so the listener stands among them.
    Vec2 best{-1, -1};
    double bestD = 1e18;
    for (int y = 0; y < c->height; ++y)
        for (int x = 0; x < c->width; ++x)
            if (const auto* t = c->tile(x, y); t && !t->solid)
            {
                const double d = std::hypot(x + .5 - near.x, y + .5 - near.y);
                if (d < bestD)
                    bestD = d, best = {x + .5, y + .5};
            }
    return best.x < 0 ? near : best;
}

std::string Sim::placeKind(const std::string& cell)
{
    // What the library calls a place (here.place: market, tavern, home...: World::placeKindOf); "home" for anywhere a
    // resident lives (the library calls a cottage "work").
    if (const auto k = kinds_.find(cell); k != kinds_.end())
        return k->second;
    if (homes_.empty())
        for (const auto& [id, r] : g_->world().society().state().residents)
            if (!r.homeCell.empty())
                homes_.insert(r.homeCell);
    if (homes_.count(cell))
        return kinds_[cell] = "home";
    return kinds_[cell] = g_->world().placeKindOf(cell);
}

void Sim::placeListener(Listener& l, const std::string& cell, Vec2 near)
{
    auto& w = g_->world();
    const auto at = openSpot(cell, near);
    l.cell = cell;
    l.at = at;
    if (!w.entity(l.id))
        w.addPlayer(l.id, "Listener " + l.id, cell, at);
    else if (!w.teleport(l.id, cell, at.x, at.y).ok)
    {
        auto* e = w.entity(l.id);
        e->cellId = cell;
        e->position = at;
    }
}

// Who stands about where, free to talk as the director counts them (on stage, alive, standing still, awake): at this
// moment, or (overTrack) summed over every step of a fixture's track.
std::map<std::string, Crowd> Sim::crowds(bool overTrack)
{
    std::map<std::string, Crowd> out;
    static const std::set<std::string> occupied{"sleep", "patrol", "companion", "relocate", "held in the gaol",
                                                "looking for a chance", "guarding a caravan", "carrying a letter"};
    auto& w = g_->world();
    // One moment's free residents, by place: each counted, and counted again as `paired` if another stands within
    // talking distance (the director pairs only those).
    std::map<std::string, std::vector<Vec2>> moment;
    const auto add = [&](const std::string& cell, double x, double y) { moment[cell].push_back({x, y}); };
    const auto settle = [&]() {
        for (const auto& [cell, at] : moment)
        {
            auto& c = out[cell];
            for (std::size_t i = 0; i < at.size(); ++i)
            {
                c.free += 1;
                c.x += at[i].x;
                c.y += at[i].y;
                for (std::size_t j = 0; j < at.size(); ++j)
                    if (j != i && std::hypot(at[i].x - at[j].x, at[i].y - at[j].y) <= 3)
                    {
                        c.paired += 1;
                        break;
                    }
            }
        }
        moment.clear();
    };
    if (!overTrack)
    {
        for (const auto& [id, e] : w.entities())
        {
            if (!e.npc || e.dead || e.transient)     // (Offstage or not: a listener coming brings them on stage.)
                continue;
            const auto* life = w.society().resident(id);
            if (!life || occupied.count(life->task) || !e.path.empty() || std::hypot(e.velocity.x, e.velocity.y) > .05)
                continue;
            add(e.cellId, e.position.x, e.position.y);
        }
        settle();
        return out;
    }
    // Over the track: read it once from the start (a separate pass), counting each step's free residents.
    const auto path = fs::path(args_.fixtures) / "track.jsonl.gz";
    FILE* p = ::popen(("gzip -dc " + shellQuote(path.string())).c_str(), "r");
    if (!p)
        return out;
    std::vector<std::string> ids, cells, tasks;
    std::vector<Place> state;
    std::string text;
    char buffer[1 << 16];
    std::string carry;
    const auto handle = [&](const std::string& line) {
        json::Value v;
        std::string error;
        if (!json::parse(line, v, error))
            return;
        for (const auto& x : v.array("ids"))
            ids.push_back(x.asString("")), state.push_back({});
        for (const auto& x : v.array("cells"))
            cells.push_back(x.asString(""));
        for (const auto& x : v.array("tasks"))
            tasks.push_back(x.asString(""));
        for (const auto& row : v.array("p"))
        {
            const auto& r = row.items();
            if (r.size() < 6)
                continue;
            const auto i = std::size_t(r[0].asNumber());
            if (i >= state.size())
                continue;
            state[i] = {int(r[1].asNumber()), float(r[2].asNumber() / 10), float(r[3].asNumber() / 10), int(r[4].asNumber()),
                        int(r[5].asNumber()), r.size() > 6 ? int(r[6].asNumber()) : -1};
        }
        if (!v.has("t"))
            return;
        for (std::size_t i = 0; i < state.size(); ++i)
        {
            const auto& s = state[i];
            if (s.cell < 0 || std::size_t(s.cell) >= cells.size() || (s.flags & 5) != 0)
                continue;                           // (Moving or dead. Offstage counts: nobody listened then.)
            if (s.task >= 0 && std::size_t(s.task) < tasks.size() && occupied.count(tasks[std::size_t(s.task)]))
                continue;
            add(cells[std::size_t(s.cell)], s.x, s.y);
        }
        settle();
    };
    for (std::size_t n; (n = std::fread(buffer, 1, sizeof buffer, p)) > 0;)
    {
        carry.append(buffer, n);
        for (std::size_t at; (at = carry.find('\n')) != std::string::npos;)
        {
            handle(carry.substr(0, at));
            carry.erase(0, at + 1);
        }
    }
    if (!carry.empty())
        handle(carry);
    ::pclose(p);
    return out;
}

// The listeners the profile names, where it says.
void Sim::placeListeners()
{
    auto& w = g_->world();
    const bool fixture = args_.mode == "fixtures";
    auto all = crowds(fixture);
    const auto middle = [&](const std::string& cell) {
        const auto& c = all[cell];
        return c.free > 0 ? Vec2{c.x / c.free, c.y / c.free} : Vec2{8, 8};
    };
    const auto kindOf = [&](const std::string& cell) { return placeKind(cell); };
    // A player listens where anyone may go: never in someone's home (L1-L10 choose among public places).
    const auto ranked = [&](const std::function<bool(const std::string&)>& want) {
        std::vector<std::pair<double, std::string>> order;
        for (const auto& [cell, c] : all)
            if (want(cell))
                order.push_back({-(c.paired * 1000 + c.free), cell});   // (Those with someone to talk to first.)
        std::sort(order.begin(), order.end());
        std::vector<std::pair<double, std::string>> open;
        for (const auto& o : order)
            if (open.size() < 12 && kindOf(o.second) != "home" && !kindOf(o.second).empty())
                open.push_back(o);
        order = open;
        std::sort(order.begin(), order.end());
        std::vector<std::string> out;
        for (const auto& o : order)
            out.push_back(o.second);
        return out;
    };
    std::vector<std::string> chosen;
    if (!args_.places.empty())
    {
        std::stringstream in(args_.places);
        for (std::string c; std::getline(in, c, ',');)
            if (!c.empty())
                chosen.push_back(c);
    }
    const auto town = [&](const std::string& cell) { return w.townIdOf(cell); };
    if (profile_ == "none")
        return;
    if (profile_ == "town")
    {
        if (chosen.empty())                         // (Each place where two or more stand about: not the homes.)
            for (const auto& [id, c] : all)
                if (c.free >= 2)
                    chosen.push_back(id);
    }
    else if (profile_ == "L1" && chosen.empty())
    {
        auto r = ranked([&](const std::string& c) { return town(c) == "ridgemere"; });
        r.resize(std::min<std::size_t>(4, r.size()));
        chosen = r;
    }
    else if (profile_ == "L10" && chosen.empty())
    {
        // Ridgemere's busiest tavern, as the library calls a place (here.place=tavern).
        for (const auto& cell : ranked([&](const std::string& c) { return town(c) == "ridgemere" && kindOf(c) == "tavern"; }))
        {
            chosen.push_back(cell);
            break;
        }
    }
    else if (profile_.size() >= 2 && profile_[0] == 'L' && chosen.empty() && profile_ != "L2")
    {
        // L3-L9: the towns besides Ridgemere by how many live there, each one's busiest place.
        std::map<std::string, int> lived;
        for (const auto& [id, r] : w.society().state().residents)
            if (!r.homeCell.empty())
                ++lived[town(r.homeCell)];
        std::vector<std::pair<int, std::string>> towns;
        for (const auto& [t, n] : lived)
            if (!t.empty() && t != "ridgemere")
                towns.push_back({-n, t});
        std::sort(towns.begin(), towns.end());
        const int index = std::atoi(profile_.c_str() + 1) - 3;
        if (index >= 0 && std::size_t(index) < towns.size())
        {
            const auto t = towns[std::size_t(index)].second;
            const auto r = ranked([&](const std::string& c) { return town(c) == t; });
            if (!r.empty())
                chosen.push_back(r.front());
        }
    }
    if (profile_ == "L2")
    {
        Listener l{"sim-l2-0"};
        listeners_.push_back(l);
        profileStep(w.time());                      // (Its first place.)
        return;
    }
    int n = 0;
    for (const auto& cell : chosen)
    {
        Listener l{"sim-" + profile_ + "-" + std::to_string(n++)};
        for (auto& ch : l.id)
            ch = char(std::tolower(static_cast<unsigned char>(ch)));
        placeListener(l, cell, middle(cell));
        listeners_.push_back(l);
    }
}

// What the profile does as time passes: the wanderer moves on every game hour; the regular comes and goes.
void Sim::profileStep(double t)
{
    auto& w = g_->world();
    if (profile_ == "L2")
    {
        static double nextMove = -1;
        if (t < nextMove)
            return;
        nextMove = t + HourSeconds;
        auto now = crowds(false);
        std::vector<std::pair<std::string, Crowd>> busy;
        double total = 0;
        // A public place where at least two stand together, more likely the more do (never into a home).
        for (const auto& [cell, c] : now)
            if (c.paired >= 2 && placeKind(cell) != "home" && !placeKind(cell).empty())
                busy.push_back({cell, c}), total += c.paired;
        if (busy.empty())
            return;
        double roll = std::uniform_real_distribution<double>(0, total)(rng_);
        auto pick = busy.back();
        for (const auto& b : busy)
            if ((roll -= b.second.paired) <= 0)
            {
                pick = b;
                break;
            }
        placeListener(listeners_[0], pick.first, {pick.second.x / pick.second.free, pick.second.y / pick.second.free});
        stage();
    }
    else if (profile_ == "L10")
    {
        const double hour = hourOf(w.calendarDays());
        const bool in = hour >= 12 && w.calendarDays() < startDay_ + 30;
        for (auto& l : listeners_)
            l.present = in;
    }
    g_->setTalkListeners(presentIds());
}

void Sim::stage()
{
    // Who is on stage: those near a listener, as the server puts residents near a player (the tick's errands step);
    // the rest live offstage, and the director passes over them. Frozen, everyone on stage stands where they are.
    auto& w = g_->world();
    w.restage();
    stage_ = w.playerStage();
    if (args_.mode == "frozen")
        settleEveryone();
}

void Sim::settleEveryone()
{
    // The frozen world: everyone stands where the save had them, as if they had just stopped (the walkers too).
    auto& w = g_->world();
    std::vector<std::tuple<std::string, std::string, Vec2, bool>> all;
    for (const auto& [id, e] : w.entities())
        if (e.npc && !e.transient)
            all.push_back({id, e.cellId, e.position, e.offstage});
    for (const auto& [id, cell, at, off] : all)
        w.replayPlace(id, cell, at, false, "", off);
}

bool Sim::openTrack()
{
    const auto path = fs::path(args_.fixtures) / "track.jsonl.gz";
    track_ = ::popen(("gzip -dc " + shellQuote(path.string())).c_str(), "r");
    return track_ != nullptr;
}

// The track's steps up to world time t read and applied: each line adds to its tables and moves who changed.
void Sim::advanceTrack(double t)
{
    if (!track_)
        return;
    while (true)
    {
        if (trackNextT_ > t)
            return;
        if (!trackPending_.empty())
        {
            json::Value v;
            std::string error;
            if (json::parse(trackPending_, v, error))
            {
                for (const auto& x : v.array("ids"))
                    trackIds_.push_back(x.asString("")), trackState_.push_back({});
                for (const auto& x : v.array("cells"))
                    trackCells_.push_back(x.asString(""));
                for (const auto& x : v.array("tasks"))
                    trackTasks_.push_back(x.asString(""));
                for (const auto& x : v.array("postures"))
                    trackPostures_.push_back(x.asString(""));
                auto& w = g_->world();
                for (const auto& row : v.array("p"))
                {
                    const auto& r = row.items();
                    if (r.size() < 6)
                        continue;
                    const auto i = std::size_t(r[0].asNumber());
                    if (i >= trackState_.size())
                        continue;
                    auto& s = trackState_[i];
                    s = {int(r[1].asNumber()), float(r[2].asNumber() / 10), float(r[3].asNumber() / 10), int(r[4].asNumber()),
                         int(r[5].asNumber()), r.size() > 6 ? int(r[6].asNumber()) : -1};
                    if (s.cell < 0 || std::size_t(s.cell) >= trackCells_.size())
                        continue;
                    w.replayPlace(trackIds_[i], trackCells_[std::size_t(s.cell)], {s.x, s.y}, (s.flags & 1) != 0,
                                  s.posture >= 0 && std::size_t(s.posture) < trackPostures_.size() ? trackPostures_[std::size_t(s.posture)] : "",
                                  !stage_.count(trackCells_[std::size_t(s.cell)]));
                    if (s.task >= 0 && std::size_t(s.task) < trackTasks_.size())
                        w.society().replayTask(trackIds_[i], trackTasks_[std::size_t(s.task)]);
                }
            }
            trackPending_.clear();
        }
        // The next line.
        std::string line;
        int ch;
        while ((ch = std::fgetc(track_)) != EOF && ch != '\n')
            line += char(ch);
        if (line.empty() && ch == EOF)
        {
            ::pclose(track_);
            track_ = nullptr;
            trackNextT_ = 1e300;
            return;
        }
        // Its time, without parsing the whole line yet.
        const auto at = line.find("\"t\":");
        trackNextT_ = at == std::string::npos ? -1 : std::strtod(line.c_str() + at + 4, nullptr);
        trackPending_ = std::move(line);
    }
}

void Sim::applyTrackState()
{
    // Everyone where the track last had them (after a new snapshot's world is made: it is the same moment's anyway).
    auto& w = g_->world();
    for (std::size_t i = 0; i < trackState_.size(); ++i)
    {
        const auto& s = trackState_[i];
        if (s.cell < 0 || std::size_t(s.cell) >= trackCells_.size())
            continue;
        w.replayPlace(trackIds_[i], trackCells_[std::size_t(s.cell)], {s.x, s.y}, (s.flags & 1) != 0,
                      s.posture >= 0 && std::size_t(s.posture) < trackPostures_.size() ? trackPostures_[std::size_t(s.posture)] : "",
                      !stage_.count(trackCells_[std::size_t(s.cell)]));
        if (s.task >= 0 && std::size_t(s.task) < trackTasks_.size())
            w.society().replayTask(trackIds_[i], trackTasks_[std::size_t(s.task)]);
    }
}

bool Sim::restartAt(const Checkpoint& c)
{
    // A new scratch world from this checkpoint, with what the listeners heard and the talk remembered carried over.
    const auto carried = g_->talkCarry();
    // Exchanges under way end with the old world (rare: a snapshot falls at midnight, when most are asleep).
    for (auto& [id, c] : open_)
    {
        auto head = c.head;
        head.add("reason", "snapshot");
        head.add("lines", double(c.lines.size()));
        c.head = head;
        convs_ << json::dump(head) << '\n';
        done_.push_back(std::move(c));
    }
    open_.clear();
    convIds_.clear();
    auto keep = std::move(g_);
    if (!makeGame((fs::path(args_.fixtures) / c.file).string()))
    {
        // (Never carried on in the day before's world, and scored as if it were this one: the run fails.)
        std::cerr << "talk_sim: " << c.file << " didn't load (" << problem_ << "); the run stops\n";
        return false;
    }
    keep.reset();
    g_->talkResume(carried);
    for (auto& l : listeners_)
        placeListener(l, l.cell, l.at);
    g_->setTalkListeners(presentIds());
    stage();
    applyTrackState();
    return true;
}

int Sim::recordRun()
{
    // The ticking run (doc 84 §15.1 and Q6): the world really runs for --days game days, written nowhere but DIR: its
    // checkpoint every game midnight and, every 120 world seconds, where everyone is and what they are doing (only what
    // changed since the step before).
    auto& w0 = g_->world();
    fs::create_directories(args_.record);
    std::string trackFile = (fs::path(args_.record) / "track.jsonl.gz").string();
    FILE* track = ::popen(("gzip -c > " + shellQuote(trackFile)).c_str(), "w");
    if (!track)
    {
        std::cerr << "talk_sim: can't write " << trackFile << "\n";
        return 1;
    }
    std::map<std::string, int> ids, cells, tasks, postures;
    std::map<std::string, Place> last;
    auto manifest = json::Value::object();
    auto cps = json::Value::array();
    const auto wallStart = std::chrono::steady_clock::now();
    const double t0 = w0.time(), day0 = w0.calendarDays();
    const auto writeCheckpoint = [&](int n) {
        auto& w = g_->world();
        const std::string name = "cp-" + std::to_string(n) + ".json.gz";
        FILE* p = ::popen(("gzip -c > " + shellQuote((fs::path(args_.record) / name).string())).c_str(), "w");
        if (!p)
            return;
        const auto text = g_->checkpointText();
        std::fwrite(text.data(), 1, text.size(), p);
        ::pclose(p);
        auto c = json::Value::object();
        c.add("file", name);
        c.add("t", w.time());
        c.add("day", w.calendarDays());
        c.add("fnv64", fileFnv((fs::path(args_.record) / name).string()));
        cps.push(c);
        std::cerr << "talk_sim: checkpoint " << name << " at day " << w.calendarDays() << "\n";
    };
    const auto step = [&]() {
        auto& w = g_->world();
        auto line = json::Value::object();
        auto newIds = json::Value::array(), newCells = json::Value::array(), newTasks = json::Value::array(),
             newPostures = json::Value::array(), rows = json::Value::array();
        const auto index = [](std::map<std::string, int>& table, const std::string& s, json::Value& added) {
            const auto found = table.find(s);
            if (found != table.end())
                return found->second;
            const int n = int(table.size());
            table[s] = n;
            added.push(s);
            return n;
        };
        for (const auto& [id, e] : w.entities())
        {
            if (!e.npc || e.transient)
                continue;
            const auto* life = w.society().resident(id);
            const int i = index(ids, id, newIds);
            Place s;
            s.cell = index(cells, e.cellId, newCells);
            s.x = float(std::round(e.position.x * 10) / 10);
            s.y = float(std::round(e.position.y * 10) / 10);
            s.flags = (!e.path.empty() || std::hypot(e.velocity.x, e.velocity.y) > .05 ? 1 : 0) | (e.offstage ? 2 : 0) | (e.dead ? 4 : 0);
            s.task = life ? index(tasks, life->task, newTasks) : -1;
            s.posture = index(postures, e.posture, newPostures);
            auto& before = last[id];
            if (before.cell == s.cell && before.x == s.x && before.y == s.y && before.flags == s.flags && before.task == s.task &&
                before.posture == s.posture)
                continue;
            before = s;
            auto row = json::Value::array();
            for (const double x : {double(i), double(s.cell), double(std::lround(s.x * 10)), double(std::lround(s.y * 10)),
                                   double(s.flags), double(s.task), double(s.posture)})
                row.push(x);
            rows.push(row);
        }
        if (newIds.size())
            line.add("ids", newIds);
        if (newCells.size())
            line.add("cells", newCells);
        if (newTasks.size())
            line.add("tasks", newTasks);
        if (newPostures.size())
            line.add("postures", newPostures);
        line.add("t", w.time());
        line.add("day", w.calendarDays());
        line.add("p", rows);
        const auto text = json::dump(line) + "\n";
        std::fwrite(text.data(), 1, text.size(), track);
    };
    int n = 0;
    writeCheckpoint(n++);
    step();
    double nextTrack = t0 + TrackEvery;
    double nextCp = (std::floor(day0 / CheckpointEveryDays) + 1) * CheckpointEveryDays;
    const double until = day0 + args_.days;
    long ticks = 0;
    while (g_->world().calendarDays() < until)
    {
        g_->tick(.05);
        ++ticks;
        if (g_->world().time() >= nextTrack)
        {
            step();
            nextTrack += TrackEvery;
        }
        if (g_->world().calendarDays() >= nextCp)
        {
            writeCheckpoint(n++);
            nextCp += CheckpointEveryDays;
        }
        if (ticks % (20 * 3600) == 0)
            std::cerr << "talk_sim: day " << g_->world().calendarDays() << "\n";
    }
    writeCheckpoint(n++);
    ::pclose(track);
    auto trackEntry = json::Value::object();
    trackEntry.add("file", "track.jsonl.gz");
    trackEntry.add("fnv64", fileFnv(trackFile));
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - wallStart).count();
    manifest.add("about", "A ticking run recorded for talk_sim --fixtures (Docs/Design/84 §15.1; the user's Q6).");
    manifest.add("world", args_.world);
    manifest.add("build", double(g_->loadedBuild()));   // (Replayed on this build: a later one moves doors and homes.)
    manifest.add("seed", double(args_.seed));
    manifest.add("days", args_.days);
    manifest.add("from_day", day0);
    manifest.add("to_day", g_->world().calendarDays());
    manifest.add("ticks", double(ticks));
    manifest.add("tick_seconds", .05);
    manifest.add("world_seconds", g_->world().time() - t0);
    manifest.add("wall_seconds", wall);
    manifest.add("speed", (g_->world().time() - t0) / std::max(1e-9, wall));
    manifest.add("talk_data", talkData_.rfind(RATW_SOURCE_DIR "/", 0) == 0 ? talkData_.substr(sizeof(RATW_SOURCE_DIR)) : talkData_);
    manifest.add("checkpoints", cps);
    manifest.add("track", trackEntry);
    manifest.add("track_every", TrackEvery);
    std::ofstream(fs::path(args_.record) / "fixture.json") << json::dump(manifest) << "\n";
    std::cerr << "talk_sim: recorded " << args_.days << " game days in " << wall << " s (" << ticks << " ticks)\n";
    return 0;
}

void Sim::writeShuffled()
{
    // The control (§15.5): every conversation's lines after the first replaced by lines said at the same side of other
    // conversations on the same topic, by a speaker of the same dialect where possible (criteria kept as far as the
    // transcript can: the same kind of place and voice). The people and the timing stay as they were.
    std::map<std::string, std::vector<std::pair<std::size_t, std::size_t>>> pool;   // key -> (conv, line)
    const auto key = [&](const Conv& c, const Line& l, bool dialect) {
        return c.topic + "|" + std::to_string(l.side) + (dialect ? "|" + l.v.string("dialect") : std::string());
    };
    for (std::size_t i = 0; i < done_.size(); ++i)
        for (std::size_t j = 1; j < done_[i].lines.size(); ++j)
        {
            pool[key(done_[i], done_[i].lines[j], true)].push_back({i, j});
            pool[key(done_[i], done_[i].lines[j], false)].push_back({i, j});
        }
    std::mt19937_64 r(args_.seed * 7919 + 17);
    std::ofstream lines(fs::path(args_.out) / "lines-shuffled.jsonl"), convs(fs::path(args_.out) / "convs-shuffled.jsonl");
    for (std::size_t i = 0; i < done_.size(); ++i)
    {
        const auto& c = done_[i];
        for (std::size_t j = 0; j < c.lines.size(); ++j)
        {
            auto v = c.lines[j].v;
            if (j > 0)
            {
                for (const bool dialect : {true, false})
                {
                    auto& from = pool[key(c, c.lines[j], dialect)];
                    std::vector<std::pair<std::size_t, std::size_t>> others;
                    for (const auto& p : from)
                        if (p.first != i)
                            others.push_back(p);
                    if (others.empty())
                        continue;
                    const auto pick = others[std::size_t(r() % others.size())];
                    const auto& src = done_[pick.first].lines[pick.second].v;
                    // (The line whole, with what it does: the moves are the line's, so the control's C1M is the
                    // shuffled lines' own.)
                    for (const char* k : {"entry", "alt", "used", "habits", "text", "route", "moves", "owes", "invites",
                                          "answers", "tone", "asserts"})
                        if (const auto* x = src.find(k))
                            v.set(k, *x);
                    for (const char* k : {"denies", "gist", "tags", "fit", "best"})
                        if (const auto* x = src.find(k))
                            v.set(k, *x);
                        else
                            v.erase(k);
                    v.set("shuffled_from", src.number("conv"));
                    break;
                }
            }
            lines << json::dump(v) << '\n';
        }
        convs << json::dump(c.head) << '\n';
    }
}

int Sim::compareGolden()
{
    // The speech only: conversation, beat, who, entry, alternative and words, in order.
    std::ostringstream now;
    for (const auto& c : done_)
        for (const auto& l : c.lines)
        {
            auto v = json::Value::object();
            for (const char* k : {"conv", "beat", "who", "to", "entry", "alt", "topic", "text"})
                if (const auto* x = l.v.find(k))
                    v.add(k, *x);
            now << json::dump(v) << '\n';
        }
    if (args_.bless)
    {
        std::ofstream(args_.golden) << now.str();
        std::cerr << "talk_sim: golden written to " << args_.golden << " (" << lineCount_ << " lines)\n";
        return 0;
    }
    std::ifstream in(args_.golden);
    if (!in)
    {
        std::cerr << "talk_sim: no golden at " << args_.golden << " (--bless writes one)\n";
        return 1;
    }
    std::istringstream mine(now.str());
    std::string a, b;
    long n = 0;
    while (true)
    {
        const bool ga = bool(std::getline(in, a)), gb = bool(std::getline(mine, b));
        ++n;
        if (!ga && !gb)
            break;
        if (ga != gb || a != b)
        {
            std::cerr << "talk_sim: the transcript differs from the golden at line " << n << ":\n  golden: " << (ga ? a : "(ended)")
                      << "\n  now:    " << (gb ? b : "(ended)") << "\n";
            return 1;
        }
    }
    std::cerr << "talk_sim: the transcript matches the golden (" << n - 1 << " lines)\n";
    return 0;
}

void Sim::writeResidents(double wall)
{
    std::ofstream out(fs::path(args_.out) / "residents.jsonl");
    for (const auto& [id, facts] : residents_)
    {
        auto v = facts;
        v.set("id", id);
        auto heard = json::Value::object();
        long total = 0;
        if (const auto found = heardBy_.find(id); found != heardBy_.end())
            for (const auto& [l, n] : found->second)
                heard.add(l, double(n)), total += n;
        v.set("_heard", heard);
        v.set("_lines", double(total));
        out << json::dump(v) << '\n';
    }
    auto meta = json::Value::object();
    meta.add("run", args_.runName);
    meta.add("mode", args_.mode);
    meta.add("world", args_.world);
    meta.add("profile", profile_);
    meta.add("seed", double(args_.seed));
    meta.add("checkpoint", args_.checkpoint);
    meta.add("fixtures", args_.fixtures);
    meta.add("build", double(g_ ? g_->loadedBuild() : 0));
    meta.add("library_from", args_.libraryFrom);
    meta.add("talk_data", talkData_);
    meta.add("world_seconds", endTime_ - startTime_);
    meta.add("hours", (endTime_ - startTime_) / 3600);
    meta.add("start_day", startDay_);
    meta.add("exchanges", double(exchanges_));
    meta.add("lines", double(lineCount_));
    meta.add("wall_seconds", wall);
    meta.add("exchanges_per_second", double(exchanges_) / std::max(1e-9, wall));
    // B1 (doc 84 §16, Phase 4): the director's beats and looks, in milliseconds (this run's load included).
    const auto stats = [](std::vector<double> xs) {
        auto o = json::Value::object();
        std::sort(xs.begin(), xs.end());
        double sum = 0;
        for (const double x : xs)
            sum += x;
        o.add("n", double(xs.size()));
        o.add("mean", xs.empty() ? 0.0 : sum / double(xs.size()));
        o.add("p99", xs.empty() ? 0.0 : xs[std::min(xs.size() - 1, std::size_t(double(xs.size()) * .99))]);
        o.add("max", xs.empty() ? 0.0 : xs.back());
        return o;
    };
    meta.add("beat_ms", stats(timing_->beats));
    meta.add("look_ms", stats(timing_->looks));
    auto ls = json::Value::array();
    for (const auto& l : listeners_)
    {
        auto o = json::Value::object();
        o.add("id", l.id);
        o.add("cell", l.cell);
        o.add("x", l.at.x);
        o.add("y", l.at.y);
        ls.push(o);
    }
    meta.add("listeners", ls);
    std::ofstream(fs::path(args_.out) / "run.json") << json::dump(meta) << "\n";
}

int Sim::main()
{
    rng_.seed(args_.seed);
    profile_ = args_.profile;
    if (args_.runName.empty())
        args_.runName = args_.mode + "-" + profile_ + "-s" + std::to_string(args_.seed);
    if (args_.out.empty())
        args_.out = "talk-sim-" + args_.runName;
    if (!prepareLibrary())
    {
        std::cerr << "talk_sim: " << problem_ << "\n";
        return 77;                                  // (ctest's skip: no git history here.)
    }
    if (!args_.dumpTopics.empty())
        topics_.open(args_.dumpTopics);
    std::string first = args_.checkpoint;
    if (args_.mode == "fixtures")
    {
        std::ifstream in(fs::path(args_.fixtures) / "fixture.json");
        std::stringstream text;
        text << in.rdbuf();
        json::Value m;
        std::string error;
        if (!json::parse(text.str(), m, error))
        {
            std::cerr << "talk_sim: no fixture at " << args_.fixtures << " (fixture.json: " << error << ")\n";
            return 1;
        }
        for (const auto& c : m.array("checkpoints"))
            checkpoints_.push_back({c.number("t"), c.number("day"), c.string("file")});
        if (checkpoints_.empty())
        {
            std::cerr << "talk_sim: the fixture has no checkpoints\n";
            return 1;
        }
        if (!checkFixture(m))
        {
            std::cerr << "talk_sim: " << problem_ << "\n";
            return 1;
        }
        std::size_t from = 0;
        if (args_.freezeAt > 0)                     // (The last checkpoint before the moment to freeze at.)
            for (std::size_t i = 0; i < checkpoints_.size(); ++i)
                if (checkpoints_[i].day <= args_.freezeAt)
                    from = i;
        first = (fs::path(args_.fixtures) / checkpoints_[from].file).string();
        nextCheckpoint_ = from + 1;
    }
    if (!makeGame(first))
    {
        std::cerr << "talk_sim: the world did not start: " << problem_ << "\n";
        return 1;
    }
    if (!args_.record.empty())
        return recordRun();
    fs::create_directories(args_.out);
    lines_.open(fs::path(args_.out) / "lines.jsonl");
    convs_.open(fs::path(args_.out) / "convs.jsonl");
    auto& w0 = g_->world();
    startTime_ = w0.time();
    startDay_ = w0.calendarDays();
    if (args_.mode == "fixtures")
    {
        if (!openTrack())
        {
            std::cerr << "talk_sim: the fixture's track can't be read\n";
            return 1;
        }
        advanceTrack(startTime_);
        applyTrackState();
        if (args_.freezeAt > 0)
        {
            // --freeze-at DAY: the recorded world as it was then (the track replayed up to that moment), frozen there.
            const double target = startTime_ + (args_.freezeAt - startDay_) * calendar::SecondsPerDay;
            while (g_->world().time() < target)
            {
                g_->world().advanceClock(std::min(TrackEvery, target - g_->world().time()));
                advanceTrack(g_->world().time());
            }
            applyTrackState();
            if (track_)
                ::pclose(track_), track_ = nullptr;
            checkpoints_.clear();
            args_.mode = "frozen";
            startTime_ = g_->world().time();
            startDay_ = g_->world().calendarDays();
        }
    }
    else if (args_.mode == "frozen")
    {
        // --warm: the world run on first (a synthetic town wakes at home), then frozen where it stands.
        for (long i = 0, n = long(args_.warm * HourSeconds / .05); i < n; ++i)
            g_->tick(.05);
        startTime_ = g_->world().time();
        startDay_ = g_->world().calendarDays();
        settleEveryone();
    }
    // A seed moves the start a little, so seeded runs differ (the director's own choices are seeded by the clock).
    if (args_.mode == "frozen" && args_.seed > 1)
        g_->world().advanceClock(double(args_.seed % 997) * 1.7);
    placeListeners();
    stage();
    if (profile_ != "none" && listeners_.empty())
    {
        std::cerr << "talk_sim: profile " << profile_ << " found nowhere to listen; where people are:\n";
        std::map<std::string, std::map<std::string, int>> where;
        for (const auto& [id, e] : g_->world().entities())
            if (e.npc && !e.transient)
                if (const auto* life = g_->world().society().resident(id))
                    ++where[e.cellId][life->task + (e.path.empty() ? "" : " (walking)")];
        for (const auto& [cell, tasks] : where)
        {
            std::cerr << "  " << cell << ":";
            for (const auto& [t, n] : tasks)
                std::cerr << " " << t << " " << n << ";";
            std::cerr << "\n";
        }
        return 1;
    }
    g_->setTalkListeners(presentIds());
    const double hours = args_.hours > 0 ? args_.hours : profile_ == "L10" ? 30 * 24 * HourSeconds / 3600 : 100;
    double until = g_->world().time() + hours * 3600;
    if (args_.mode == "fixtures" && !checkpoints_.empty())
    {
        // Never past the recording's end.
        std::ifstream in(fs::path(args_.fixtures) / "fixture.json");
        std::stringstream text;
        text << in.rdbuf();
        json::Value m;
        std::string error;
        json::parse(text.str(), m, error);
        const double recordedEnd = checkpoints_.front().t + m.number("world_seconds");
        until = std::min(until, recordedEnd);
    }
    const auto wallStart = std::chrono::steady_clock::now();
    const double dt = args_.mode == "tick" ? .05 : .25;
    long steps = 0;
    while (g_->world().time() < until && (args_.exchanges < 0 || exchanges_ < args_.exchanges))
    {
        const double t = g_->world().time();
        if (args_.mode == "fixtures")
        {
            while (nextCheckpoint_ < checkpoints_.size() && t >= checkpoints_[nextCheckpoint_].t)
                if (!restartAt(checkpoints_[nextCheckpoint_++]))
                    return 1;
            advanceTrack(t);
        }
        profileStep(t);
        if (args_.mode == "tick")
            g_->tick(dt);
        else
        {
            g_->world().advanceClock(dt);
            g_->ambient(dt);
        }
        if (topics_.is_open())
            flushTopics(t);
        if (++steps % 144000 == 0)
            std::cerr << "talk_sim " << args_.runName << ": " << (g_->world().time() - startTime_) / 3600 << " h, " << exchanges_
                      << " exchanges\n";
    }
    endTime_ = g_->world().time();
    // Exchanges still under way end here, as they are.
    for (auto& [id, c] : open_)
    {
        auto head = c.head;
        head.add("reason", "run_end");
        head.add("lines", double(c.lines.size()));
        c.head = head;
        convs_ << json::dump(head) << '\n';
        done_.push_back(std::move(c));
    }
    open_.clear();
    std::sort(done_.begin(), done_.end(), [](const Conv& a, const Conv& b) { return a.head.number("conv") < b.head.number("conv"); });
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - wallStart).count();
    lines_.close();
    convs_.close();
    writeResidents(wall);
    if (args_.ablate == "shuffle")
        writeShuffled();
    std::cerr << "talk_sim " << args_.runName << ": " << exchanges_ << " exchanges, " << lineCount_ << " lines in "
              << (endTime_ - startTime_) / 3600 << " world hours; " << wall << " s wall\n";
    if (!args_.golden.empty())
        return compareGolden();
    return 0;
}

int usage()
{
    std::cerr << "usage: talk_sim --world KEY|EXPORT_DIR [--frozen | --fixtures DIR | --tick] [--profile L1..L10|town|none]\n"
                 "                [--seed N] [--hours H] [--exchanges N] [--out DIR] [--run NAME] [--checkpoint FILE]\n"
                 "                [--places CELL,CELL] [--warm HOURS] [--freeze-at DAY] [--library-from REF] [--talk-data DIR]\n"
                 "                [--ablate shuffle]\n"
                 "                [--golden FILE [--bless]]\n"
                 "       talk_sim --world KEY --tick --days D --record DIR\n";
    return 2;
}
} // namespace

int main(int argc, char** argv)
{
    Args a;
    for (int i = 1; i < argc; ++i)
    {
        const std::string s = argv[i];
        const auto next = [&]() -> std::string {
            if (i + 1 >= argc)
                throw std::runtime_error(s + " needs a value");
            return argv[++i];
        };
        try
        {
            if (s == "--world") a.world = next();
            else if (s == "--frozen") a.mode = "frozen";
            else if (s == "--fixtures") a.mode = "fixtures", a.fixtures = next();
            else if (s == "--tick") a.mode = "tick";
            else if (s == "--profile") a.profile = next();
            else if (s == "--seed") a.seed = std::stoull(next());
            else if (s == "--hours") a.hours = std::stod(next());
            else if (s == "--days") a.days = std::stod(next());
            else if (s == "--warm") a.warm = std::stod(next());
            else if (s == "--freeze-at") a.freezeAt = std::stod(next());
            else if (s == "--exchanges") a.exchanges = std::stol(next());
            else if (s == "--out") a.out = next();
            else if (s == "--run") a.runName = next();
            else if (s == "--checkpoint") a.checkpoint = next();
            else if (s == "--places") a.places = next();
            else if (s == "--library-from") a.libraryFrom = next();
            else if (s == "--talk-data") a.talkData = next();
            else if (s == "--ablate") a.ablate = next();
            else if (s == "--golden") a.golden = next();
            else if (s == "--dump-topics") a.dumpTopics = next();
            else if (s == "--dump-topics-all") a.dumpTopicsAll = true;
            else if (s == "--topics") a.topics = next();
            else if (s == "--bless") a.bless = true;
            else if (s == "--record") a.record = next();
            else
                return usage();
        }
        catch (const std::exception& e)
        {
            std::cerr << "talk_sim: " << e.what() << "\n";
            return usage();
        }
    }
    if (a.world.empty() || (!a.ablate.empty() && a.ablate != "shuffle") || (!a.record.empty() && a.mode != "tick"))
        return usage();
    if (!a.record.empty())
        a.profile = "none";
    return Sim(std::move(a)).main();
}
