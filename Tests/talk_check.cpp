// talk_check (Docs/Design/62-talk.md, section 6): why did she say that. The facts of a moment, every rule and scene
// that held with its score, which one was chosen and why, the grammar's expansions step by step, and the lines.
//
//   talk_check [--data DIR] CONCEPT [key=value ...] [--seed N] [--many N] [--all]
//       A moment from the facts given: a.name=Maren a.sex=female a.job=baker here.place=market topic.item=herbs...
//       (`b.` facts make a second speaker; without them the concept is said to no one).
//   talk_check [--data DIR] --world dev|prod|EXPORT_DIR A_ID [B_ID] CONCEPT [key=value ...] [--seed N] [--many N] [--all]
//       Two residents (or one) and where they stand now, from a world read as a scratch server reads it (nothing
//       written). CONCEPT `talk`: the topic the world would choose for the two now, with its facts, as the ambient
//       director asks. Any other concept: the topic's facts (topic.item=herbs) are given as key=value, which also
//       override anything the world said.
//   talk_check [--data DIR] --validate FILE...
//       Checks .talk and .grammar files against the library's facts, settings, grammars and groups, as Atlas and the
//       Dungeon Master will before saving a character's table. Exit 1, with the reasons, if any is refused.
//
//   talk_check [--data DIR] --world dev|prod|EXPORT_DIR [--run HOURS] --residents [key=value]
//       Every resident's facts, counted: how many are each job, each wealth, injured... With key=value, those
//       residents (up to 40) and their facts.
//
// --run HOURS runs the world on that many game hours first (written nowhere), so hunger, work and the rest move.
// --many N picks N times with different seeds and counts what was said (the tiers' odds, heard by ear). --all lists
// every rule and scene that didn't fit, and why. DIR defaults to the source tree's Data/Talk.
#include "RatwGame.h"
#include "RatwTalk.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <numeric>
#include <sstream>
#include <unistd.h>

using namespace ratw;

namespace
{
int usage()
{
    std::cerr << "usage: talk_check [--data DIR] CONCEPT [key=value ...] [--seed N] [--many N] [--all]\n"
                 "       talk_check [--data DIR] --world dev|prod|EXPORT_DIR A_ID [B_ID] CONCEPT [key=value ...] [--seed N] [--many N] [--all]\n"
                 "       talk_check [--data DIR] --validate FILE...\n";
    return 2;
}

std::string conninfo(const std::string& database)
{
    if (const char* url = std::getenv("RATW_DATABASE_URL"))
        return url;
    const std::string command = "python3 '" RATW_SOURCE_DIR "/tools/world_db.py' conninfo " + database + " --role game";
    std::string out;
    if (FILE* p = ::popen(command.c_str(), "r"))
    {
        char buffer[512];
        while (std::fgets(buffer, sizeof buffer, p))
            out += buffer;
        ::pclose(p);
    }
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
        out.pop_back();
    return out;
}

void printFacts(const talk::Query& q)
{
    std::cout << "concept " << q.concept << (q.alone ? " (said to no one)" : "") << "\n";
    for (const auto& [who, s] : {std::pair<const char*, const talk::Speaker*>{"a", &q.a}, {"b", &q.b}})
    {
        if (std::string(who) == "b" && q.alone)
            continue;
        std::cout << "  " << who << ":";
        for (const auto& [k, v] : s->facts)
            std::cout << " " << k << "=" << v;
        if (!s->grammar.empty())
        {
            std::cout << "  (grammar:";
            for (const auto& g : s->grammar)
                std::cout << " " << g;
            std::cout << " plain)";
        }
        std::cout << "\n";
    }
    for (const auto& [k, v] : q.facts)
        std::cout << "  " << k << "=" << v << "\n";
}

int explain(const talk::Library& lib, const talk::Query& q, std::uint64_t seed, int many, bool all)
{
    printFacts(q);
    if (const auto card = lib.voiceCard(q.a); !card.empty())
        std::cout << "  a's voice card (for the model): " << card << "\n";
    const auto none = [](const std::string&) { return false; };
    talk::Trace trace;
    const auto said = lib.converse(q, seed, none, {}, &trace);
    std::cout << "\nheld (" << trace.held.size() << "):\n";
    auto held = trace.held;
    std::stable_sort(held.begin(), held.end(), [](const talk::Trace::Held& x, const talk::Trace::Held& y) { return x.score > y.score; });
    for (const auto& h : held)
        std::cout << "  tier " << h.tier + 1 << "  score " << h.score << (h.priority ? "  priority " + std::to_string(h.priority) : "")
                  << (h.weight != 1 ? "  weight " + (std::ostringstream() << h.weight).str() : "") << "  " << h.id << "\n";
    std::cout << "passed over: " << trace.passed.size() << (all ? "" : " (--all lists them)") << "\n";
    if (all)
        for (const auto& p : trace.passed)
            std::cout << "  " << p << "\n";
    std::cout << "\nchosen: " << trace.why << "\n";
    for (const auto& step : trace.steps)
        std::cout << "  " << step << "\n";
    for (const auto& [who, text] : said.lines)
        std::cout << (who == 0 ? "  a: " : "  b: ") << text << "\n";    for (const auto& e : said.effects)
        std::cout << "  remembers (" << e.scope << " " << e.owner << "): " << e.key << "=" << e.value
                  << (e.days < 0 ? " for good" : " for " + (std::ostringstream() << e.days).str() + " days") << "\n";
    if (many > 0)
    {
        std::map<std::string, int> counts;
        for (int i = 0; i < many; ++i)
            ++counts[lib.pick(q, seed + std::uint64_t(i) * 7919, none, {}).id];
        std::vector<std::pair<int, std::string>> order;
        for (const auto& [id, n] : counts)
            order.push_back({n, id.empty() ? "(nothing)" : id});
        std::sort(order.rbegin(), order.rend());
        std::cout << "\n" << many << " picks:\n";
        for (const auto& [n, id] : order)
            std::cout << "  " << n << "  " << id << "\n";
    }
    return said.id.empty() ? 1 : 0;
}

// key=value onto the query: a.X and b.X are a speaker's, the rest the moment's.
bool setFact(talk::Query& q, const std::string& kv)
{
    const auto eq = kv.find('=');
    if (eq == std::string::npos || eq == 0)
        return false;
    const auto key = kv.substr(0, eq), value = kv.substr(eq + 1);
    if (key == "a.grammar" || key == "b.grammar")
        (key[0] == 'a' ? q.a : q.b).grammar.push_back(value);          // A layer above plain: a.grammar=oakhaven
    else if (key.rfind("a.", 0) == 0)
        q.a.facts[key.substr(2)] = value;
    else if (key.rfind("b.", 0) == 0)
        q.b.facts[key.substr(2)] = value, q.alone = false;
    else
        q.facts[key] = value;
    return true;
}
} // namespace

int main(int argc, char** argv)
{
    std::string data = RATW_SOURCE_DIR "/Data/Talk", world;
    std::vector<std::string> args, validate;
    std::uint64_t seed = 1;
    int many = 0;
    double runHours = 0;
    bool all = false, validating = false, residents = false;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        const auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (a == "--data")
            data = next();
        else if (a == "--world")
            world = next();
        else if (a == "--seed")
            seed = std::stoull(next());
        else if (a == "--many")
            many = std::stoi(next());
        else if (a == "--all")
            all = true;
        else if (a == "--validate")
            validating = true;
        else if (a == "--residents")
            residents = true;
        else if (a == "--run")
            runHours = std::stod(next());
        else if (validating)
            validate.push_back(a);
        else
            args.push_back(a);
    }
    talk::Library lib;
    std::string problem;
    if (validating)
    {
        if (validate.empty())
            return usage();
        if (!lib.loadFoundation(data, problem))
        {
            std::cerr << "the library's foundation doesn't load:\n" << problem << "\n";
            return 1;
        }
        int refused = 0;
        for (const auto& file : validate)
        {
            std::ifstream in(file);
            std::stringstream text;
            text << in.rdbuf();
            const auto name = std::filesystem::path(file).filename().string();
            const bool ok = std::filesystem::path(file).extension() == ".grammar" ? lib.parseGrammar(text.str(), name, problem)
                                                                                    : lib.parse(text.str(), name, problem);
            std::cout << (ok ? "ok      " : "REFUSED ") << file << "\n";
            if (!ok)
                std::cout << problem << "\n", ++refused;
        }
        return refused ? 1 : 0;
    }
    if (world.empty())
    {
        if (args.empty())
            return usage();
        if (!lib.load(data, problem))
        {
            std::cerr << "the talk library doesn't load:\n" << problem << "\n";
            return 1;
        }
        talk::Query q;
        q.concept = args[0];
        q.alone = true;
        for (std::size_t i = 1; i < args.size(); ++i)
            if (!setFact(q, args[i]))
            {
                std::cerr << "'" << args[i] << "' is not key=value\n";
                return 2;
            }
        return explain(lib, q, seed, many, all);
    }
    // A world, read only.
    if (args.size() < 2 && !residents)
        return usage();
    game::Options o;
    o.talkData = data;
    o.voiceData = RATW_SOURCE_DIR "/Data/Voice";       // (The router's tones: a resident's tone fact.)
    o.scratch = true;
    o.forkSnapshots = false;
    o.savePath = "/tmp/ratw-talk-check-" + std::to_string(::getpid()) + ".json";
    if (world == "dev" || world == "prod")
    {
        o.database = world;
        o.conninfo = conninfo(world);
    }
    else
        o.worldExport = world;
    game::Game g(o);
    if (!g.start(problem))
    {
        std::cerr << "the world did not start: " << problem << "\n";
        return 1;
    }
    if (runHours > 0)
    {
        // The world run on (nothing saved: a scratch world), so residents get hungry, go to work, fall out...
        const double until = g.world().calendarDays() + runHours / 24;
        for (long i = 0; i < 20L * 3600 * 24 && g.world().calendarDays() < until; ++i)
            g.tick(.05);
        std::cerr << "ran to day " << g.world().calendarDays() << "\n";
    }
    if (g.talkLibrarySize() == 0)
    {
        std::cerr << "the talk library at " << data << " didn't load (see above)\n";
        return 1;
    }
    if (residents)
    {
        // Every resident's facts, as the library would see them speak (to no one, here and now).
        std::map<std::string, std::map<std::string, int>> counts;
        std::string key, value;
        if (!args.empty() && args[0].find('=') != std::string::npos)
            key = args[0].substr(0, args[0].find('=')), value = args[0].substr(args[0].find('=') + 1);
        int shown = 0, total = 0;
        for (const auto& [id, e] : g.world().entities())
        {
            if (!e.npc || e.transient || e.dead || !g.world().society().spec(id))
                continue;
            ++total;
            const auto q = g.talkQuery(id, {}, "bark");
            for (const auto& [k, v] : q.a.facts)
                if (k != "id" && k != "name" && k != "age")
                    ++counts[k][v];
            if (!key.empty())
            {
                const auto f = q.a.facts.find(key);
                if (f != q.a.facts.end() && f->second == value && shown++ < 40)
                {
                    std::cout << id << " (" << g.world().society().spec(id)->workLabel << "):";
                    for (const auto& [k, v] : q.a.facts)
                        if (k != "id")
                            std::cout << " " << k << "=" << v;
                    std::cout << "\n";
                }
            }
        }
        if (!key.empty())
            return 0;
        std::cout << total << " residents\n";
        for (const auto& [k, values] : counts)
        {
            std::vector<std::pair<int, std::string>> order;
            std::vector<double> numbers;
            for (const auto& [v, n] : values)
            {
                order.push_back({n, v});
                char* end = nullptr;
                const double x = std::strtod(v.c_str(), &end);
                if (!v.empty() && !*end)
                    numbers.insert(numbers.end(), std::size_t(n), x);
            }
            if (numbers.size() == std::size_t(std::accumulate(order.begin(), order.end(), 0, [](int t, const auto& o) { return t + o.first; })))
            {
                std::sort(numbers.begin(), numbers.end());
                const auto at = [&](double q) { return numbers[std::min(numbers.size() - 1, std::size_t(q * double(numbers.size())))]; };
                std::cout << k << ": least " << numbers.front() << ", tenth " << at(.1) << ", median " << at(.5) << ", ninetieth "
                          << at(.9) << ", most " << numbers.back() << "\n";
                continue;
            }
            std::sort(order.rbegin(), order.rend());
            std::cout << k << ":";
            for (std::size_t i = 0; i < order.size() && i < 30; ++i)
                std::cout << " " << order[i].second << " " << order[i].first;
            std::cout << (order.size() > 30 ? " ..." : "") << "\n";
        }
        return 0;
    }
    // A_ID [B_ID] CONCEPT: the concept is the first argument that isn't a resident.
    std::string aId = args[0], bId;
    std::size_t at = 1;
    if (at < args.size() && args[at].find('=') == std::string::npos && args[at].rfind("talk", 0) != 0 && args[at] != "bark" &&
        args[at].find('.') == std::string::npos)
        bId = args[at++];
    if (at >= args.size())
        return usage();
    const std::string concept = args[at++];
    for (const auto& id : {aId, bId})
        if (!id.empty() && !g.world().entity(id))
        {
            std::cerr << "no one called " << id << " in this world\n";
            return 1;
        }
    // "talk": the topic the world would choose for the two now, with its facts.
    auto q = concept == "talk" && !bId.empty() ? g.talkTopicQuery(aId, bId) : g.talkQuery(aId, bId, concept);
    for (; at < args.size(); ++at)
        if (!setFact(q, args[at]))
        {
            std::cerr << "'" << args[at] << "' is not key=value\n";
            return 2;
        }
    return explain(g.talkLibrary(), q, seed, many, all);
}
