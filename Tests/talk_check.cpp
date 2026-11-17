// talk_check (Docs/Design/62-talk.md, section 6): why did she say that. The facts of a moment, every rule and scene
// that held with its score, which one was chosen and why, the grammar's expansions step by step, and the lines.
//
//   talk_check [--data DIR] CONCEPT [key=value ...] [--seed N] [--many N] [--all]
//       A moment from the facts given: a.name=Maren a.sex=female a.job=baker here.place=market topic.item=herbs...
//       (`b.` facts make a second speaker; without them the concept is said to no one).
//   talk_check [--data DIR] --world KEY|EXPORT_DIR A_ID [B_ID] CONCEPT [key=value ...] [--seed N] [--many N] [--all]
//       Two residents (or one) and where they stand now, from a world read as a scratch server reads it (nothing
//       written). CONCEPT `talk`: the topic the world would choose for the two now, with its facts, as the ambient
//       director asks. Any other concept: the topic's facts (topic.item=herbs) are given as key=value, which also
//       override anything the world said.
//   talk_check [--data DIR] --character NPC_ID [--grammar FILE] [--rules FILE]
//       Checks one resident's character table (their own words and lines: live.npc_voice's grammar and rules, doc 62
//       phase 6) against the whole library, as the game loads it. Exit 1, with the reasons, if it's refused.
//   talk_check [--data DIR] --dialect NAME --grammar FILE
//       Checks a dialect's grammar (live.dialects, decision 7) as the game loads it over the files'.
//   talk_check [--data DIR] --validate FILE...      (.talk, .grammar, or a dialogue .tree)
//       Checks .talk and .grammar files against the library's facts, settings, grammars and groups, as Atlas and the
//       Dungeon Master will before saving a character's table. Exit 1, with the reasons, if any is refused.
//
//   talk_check [--data DIR] --mantle PATH... [--dialects DIR] [--json]
//       Doc 84 §14.4 G1, §17.3 (Phase 9): each .mantle file (a directory: every one under it) read and checked against
//       the whole library as the game loads it (its syntax, facts, symbols and its own words, moves, 160 characters,
//       the window's trees and nodes, a graft's nodes and ways out, what its lines owe answered), in any state. With
//       --dialects, the world's own dialects (NAME.grammar, as live.dialects holds them) over the files'. Exit 1, with
//       the reasons by item key, if any is refused; --json, one object a mantle (tools/mantle_store.py reads it).
//
//   (--mantles-from DIR, with --world: the residents wear the mantles under DIR, each the one written for them, in place of
//   the world's rows: an export, or Tests/fixtures/mantles.)
//
//   talk_check [--data DIR] --coverage [--profiles FILE] [--json] [--holes N]
//       Doc 84 §15.4 (Phase 7): for every move a line can owe, every topic (topics.json) and every replier kind
//       (Data/Talk/profiles.json, tools/talk_profiles.py: Ridgemere 3's residents clustered) at each band (strangers,
//       acquaintances, friends, family): is there a move line (one with no concept, found by its move) doing it, or a
//       variant of it, that holds for that kind on that topic? No rendering. The holes listed, by move; the core moves'
//       (greet.return, react.news, react.complaint, answer.detail, accept_preclose, farewell.return, accept_decline)
//       first. Exit 0; it lists, it doesn't judge (Phase 8's gate: no holes in the core moves).
//   talk_check [--data DIR] --render-trees [--json] [--seed N]
//       Every dialogue tree's NPC lines (Data/Talk/dialogue, doc 83) in every voice: plain and each dialect, at a light
//       and a broad accent, each alternative filled in with sample facts. Exit 1 if a line comes out longer than 160
//       characters or with a symbol unfilled (tools/dialect_check.py runs it, and writes the listen sheet from --json).
//
//   talk_check [--data DIR] --world KEY|EXPORT_DIR [--run HOURS] --residents [key=value]
//       Every resident's facts, counted: how many are each job, each wealth, injured... With key=value, those
//       residents (up to 40) and their facts. With --json, every resident's facts, a JSON object a line.
//
// KEY is a world in the registry (python3 tools/world_db.py list; atlas and live are the local choices), else a
// world_build.py export directory.
// --run HOURS runs the world on that many game hours first (written nowhere), so hunger, work and the rest move.
// --many N picks N times with different seeds and counts what was said (the tiers' odds, heard by ear). --all lists
// every rule and scene that didn't fit, and why. DIR defaults to the source tree's Data/Talk.
#include "RatwGame.h"
#include "RatwTalk.h"

#include <algorithm>
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
                 "       talk_check [--data DIR] --world KEY|EXPORT_DIR A_ID [B_ID] CONCEPT [key=value ...] [--seed N] [--many N] [--all]\n"
                 "       talk_check [--data DIR] --validate FILE...\n";
    return 2;
}

// What tools/world_db.py prints for a command ("" if it failed), its last line ending trimmed.
std::string worldDb(const std::string& arguments)
{
    const std::string command = "python3 '" RATW_SOURCE_DIR "/tools/world_db.py' " + arguments;
    std::string out;
    if (FILE* p = ::popen(command.c_str(), "r"))
    {
        char buffer[512];
        while (std::fgets(buffer, sizeof buffer, p))
            out += buffer;
        if (::pclose(p) != 0)
            return "";
    }
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
        out.pop_back();
    return out;
}

std::string conninfo(const std::string& database)
{
    if (const char* url = std::getenv("RATW_DATABASE_URL"))
        return url;
    return worldDb("conninfo " + database + " --role game");
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

// Every tree line in every voice (--render-trees): sample facts for what the trees say, the speaker an adult of each
// dialect (and plain), light and broad.
int renderTrees(const talk::Library& lib, std::uint64_t seed, bool asJson)
{
    static const std::map<std::string, std::string> Sample{
        {"topic.why", "Someone else has taken it on."}, {"topic.job_why", "Someone else has taken it on."},
        {"here.town", "Coldwater"}, {"here.folk", "66"}, {"topic.player", "Ash"}, {"topic.claim", "has been seen at the docks at night"},
        {"here.take_on_why", "They don't know you well enough to take you on yet."}, {"here.shops", "5"}, {"here.inns", "1"},
        {"here.service", "from nine in the morning until eleven"}, {"here.fine", "8"},
        {"here.bed_why", "You have a lodging already."}, {"topic.subject", "Hesta Marl"}, {"topic.job_paid", "twelve pennies"},
        {"topic.job_reward", "12"}, {"here.bed_night", "3"}, {"topic.job_title", "Carry a letter to Oda Fenn"},
        {"topic.job_due", "within two days"}, {"here.faction", "the Lodge"}, {"topic.job_poster", "Ragnar"},
        {"topic.job_handover", "I've a letter for you, from Ragnar."},
        {"topic.job_directions", "A letter for Oda Fenn, at the Glasswater Inn. She'll be in the taproom of an evening."},
        {"here.upstairs_night", "14"}, {"here.district", "Greatly"}, {"here.bed_week", "15"}, {"world.news_from", "Ridgemere"},
        {"world.news", "the Lodge has a new master"}, {"world.event_words", "the horse fair on Marketday"},
        {"topic.trouble_words", "I'll lend you a few pennies till Restday."}, {"topic.answer", "The Lodge holds you well: trusted."},
        {"topic.place_way", "The inn's across the green, the door with the lamp. Ask for Loam."},
        {"topic.place_title", "The Glasswater Inn"}, {"topic.place_kind", "the inn"},
        {"topic.job_risk", "Bandits have been seen on the East Road. Keep to the daylight."},
        {"topic.job_progress", "You've the letter still. Oda's at the Glasswater Inn."}, {"here.trade_title", "smithing"},
        {"here.tithe_fee", "20"}, {"here.report_fee", "5"},
        {"here.bounty", "There's a price on the bandit camp out by the Greyteeth: 40 pennies to whoever clears it."},
        {"here.innkeeper", "Loam Pinefell"}, {"topic.greeting", "Welcome to the Glasswater."}};
    std::vector<std::string> voices{"plain"};
    for (const auto& g : lib.grammars())
        if (g != "plain" && g.rfind("person:", 0) != 0)
            voices.push_back(g);
    auto out = json::Value::array();
    int problems = 0;
    for (const auto& e : lib.entries())
    {
        if (e.id.rfind("dlg.", 0) != 0 || e.turns.empty() || e.concepts.empty())
            continue;
        const auto concept = *e.concepts.begin();                 // dlg.<tree>.<node>
        const auto dot = concept.find('.', 4);
        const auto tree = concept.substr(4, dot - 4), node = concept.substr(dot + 1);
        std::string criteria;
        for (const auto& c : e.when)
            if (c.full != "concept" && c.name != "concept")
                criteria += (criteria.empty() ? "" : " ") + (c.full.empty() ? c.name : c.full) + (c.op == talk::Criterion::Op::NotIn ? "!=" : "=") +
                            [&] { std::string v; for (const auto& x : c.values) v += (v.empty() ? "" : "|") + x; return v; }();
        int alt = 0;
        for (const auto& variant : e.turns[0].variants)
            for (const auto& text : variant.texts)
            {
                ++alt;
                for (const auto& voice : voices)
                    for (const char* accent : {"light", "broad"})
                    {
                        if (voice == "plain" && std::string(accent) == "broad")
                            continue;
                        talk::Query q;
                        q.concept = concept;
                        q.alone = true;
                        q.a.facts = {{"id", "sample"}, {"name", "Sample"}, {"sex", "female"}, {"stage", "adult"}, {"age", "40"},
                                     {"job", "smith"}, {"role", "merchant"}, {"trade", "smith"}};
                        if (voice != "plain")
                        {
                            q.a.facts["from"] = voice;
                            q.a.facts["accent"] = accent;
                            q.a.grammar.push_back(voice);
                        }
                        q.b.facts = {{"id", "player"}, {"name", "Ash"}, {"sex", "male"}, {"stage", "adult"}, {"age", "30"}};
                        for (const auto& [k, v] : Sample)
                            q.facts[k] = v;
                        std::string said, problem;
                        if (!lib.expand(text, q, 0, seed + std::uint64_t(alt) * 31, said))
                            problem = "a fact it needs is missing";
                        else if (said.find('#') != std::string::npos)
                            problem = "a symbol left unfilled";
                        else if (said.size() > 160)
                            problem = "longer than 160 characters (" + std::to_string(said.size()) + ")";
                        problems += !problem.empty();
                        if (asJson)
                        {
                            auto r = json::Value::object();
                            r.add("tree", tree);
                            r.add("node", node);
                            r.add("rule", e.id);
                            r.add("alt", alt);
                            r.add("criteria", criteria);
                            r.add("voice", voice);
                            r.add("accent", accent);
                            r.add("template", text);
                            r.add("text", said);
                            if (!problem.empty())
                                r.add("problem", problem);
                            out.push(std::move(r));
                        }
                        else if (!problem.empty())
                            std::cout << e.id << " (" << voice << ", " << accent << "): " << problem << ": " << said << "\n";
                    }
            }
    }
    if (asJson)
        std::cout << json::dump(out) << "\n";
    else
        std::cout << (problems ? std::to_string(problems) + " problems\n" : std::string("every tree line renders in every voice\n"));
    return problems ? 1 : 0;
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
    for (std::size_t i = 0; i < said.lines.size(); ++i)
    {
        std::cout << (said.lines[i].first == 0 ? "  a: " : "  b: ") << said.lines[i].second << "\n";
        // What it does in the talk (doc 84, Phase 2): its moves, what it owes, what it answers, its tone.
        if (i < said.info.size() && !said.info[i].moves.empty())
        {
            const auto& l = said.info[i];
            const auto join = [](const std::vector<std::string>& v) {
                std::string out;
                for (const auto& x : v)
                    out += (out.empty() ? "" : ", ") + x;
                return out;
            };
            std::cout << "       does " << join(l.moves) << (l.owes.empty() ? "" : "; owes " + join(l.owes))
                      << (l.invites.empty() ? "" : "; invites " + join(l.invites)) << (l.answers.empty() ? "" : "; answers " + join(l.answers))
                      << "; " << l.tone << (l.asserts.empty() ? "" : "; asserts " + join(l.asserts)) << "\n";
        }
    }
    for (const auto& e : said.effects)
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

// The coverage check (--coverage, doc 84 §15.4): a move line for every owed move, topic and replier kind.
int coverage(const talk::Library& lib, const std::string& data, const std::string& profilesFile, bool asJson, int listHoles)
{
    const auto read = [](const std::string& file, json::Value& out) {
        std::ifstream in(file);
        std::stringstream text;
        text << in.rdbuf();
        std::string error;
        return in.good() || !text.str().empty() ? json::parse(text.str(), out, error) : false;
    };
    json::Value profiles, topicsDoc;
    if (!read(profilesFile, profiles) || !profiles.isObject())
    {
        std::cerr << "talk_check --coverage: no replier kinds in " << profilesFile << " (tools/talk_profiles.py ridgemere3)\n";
        return 1;
    }
    std::vector<std::string> topics;
    if (read(data + "/topics.json", topicsDoc))
        for (const auto& [name, t] : topicsDoc["topics"].fields())
            topics.push_back(name);
    if (topics.empty())
        topics = {"smalltalk"};
    // Every move a line can owe: what any move expects, and what any line writes it owes.
    std::set<std::string> owed;
    for (const auto& [name, m] : lib.moves().all())
        owed.insert(m.expects.begin(), m.expects.end());
    for (const auto& e : lib.entries())
        for (const auto& o : e.owes)
            if (o.who != "self")
                owed.insert(o.moves.begin(), o.moves.end());
    static const std::vector<std::string> Core{"greet.return", "react.news", "react.complaint", "answer.detail",
                                               "accept_preclose", "farewell.return", "accept_decline"};
    // The move lines (no concept) doing each owed move or a variant of it.
    std::map<std::string, std::vector<const talk::Entry*>> lines;
    for (const auto& m : owed)
        if (const auto* list = lib.byMove(m))
            for (const auto i : *list)
                if (lib.entries()[i].concepts.empty() && !lib.entries()[i].scene)
                    lines[m].push_back(&lib.entries()[i]);
    struct Kind
    {
        std::string id, region, label;
        int count = 0;
        talk::Facts facts;
    };
    std::vector<Kind> kinds;
    for (const auto& k : profiles["kinds"].items())
    {
        Kind kind;
        kind.id = k.string("id");
        kind.region = k.string("region", "ridgemere");
        kind.count = int(k.number("count"));
        for (const auto& [key, v] : k["axes"].fields())
            kind.label += (kind.label.empty() ? "" : " ") + v.asString(std::string());
        for (const auto& [key, v] : k["facts"].fields())
            if (v.isString())
                kind.facts[key] = v.asString();
        kind.facts["id"] = "kind_" + kind.id;
        kinds.push_back(std::move(kind));
    }
    std::vector<std::string> bands;
    for (const auto& b : profiles["bands"].items())
        bands.push_back(b.asString(std::string()));
    if (kinds.empty() || bands.empty())
    {
        std::cerr << "talk_check --coverage: " << profilesFile << " has no kinds or bands\n";
        return 1;
    }
    // The one who made the answer due: an ordinary grown resident (the commonest kind, another of them).
    talk::Facts other = kinds.front().facts;
    other["id"] = "kind_other";
    struct Tally
    {
        long cells = 0, holes = 0;
        std::map<std::string, int> byTopic, byKind, byBand;
        std::vector<std::string> examples;
    };
    std::map<std::string, Tally> tally;
    long people = 0;
    for (const auto& k : kinds)
        people += k.count;
    for (const auto& topic : topics)
        for (const auto& k : kinds)
            for (const auto& band : bands)
            {
                talk::Query q;
                q.concept = "talk." + topic;
                q.a.facts = k.facts;
                q.b.facts = other;
                q.a.facts["knows"] = "no";
                q.facts = {{"pair.band", band}, {"pair.named", band == "strangers" ? "no" : "yes"}, {"here.time", "day"},
                           {"here.weather", "clear"}, {"here.outdoors", "clear"}, {"here.place", "street"},
                           {"here.region", k.region}, {"conv.topic", topic}, {"conv.beat", "2"}};
                if (band == "family")
                    q.facts["pair.kin"] = "siblings";
                for (const auto& m : owed)
                {
                    auto& t = tally[m];
                    ++t.cells;
                    const auto found = lines.find(m);
                    const bool held = found != lines.end() && std::any_of(found->second.begin(), found->second.end(),
                                                                          [&](const talk::Entry* e) { return lib.fits(*e, q, 0); });
                    if (held)
                        continue;
                    ++t.holes;
                    ++t.byTopic[topic];
                    ++t.byKind[k.id + " " + k.label];
                    ++t.byBand[band];
                    if (int(t.examples.size()) < listHoles)
                        t.examples.push_back(topic + " / " + k.id + " " + k.label + " / " + band);
                }
            }
    long cells = 0, holes = 0, coreHoles = 0;
    for (const auto& [m, t] : tally)
    {
        cells += t.cells;
        holes += t.holes;
        if (std::find(Core.begin(), Core.end(), m) != Core.end())
            coreHoles += t.holes;
    }
    const auto top = [](const std::map<std::string, int>& counts, std::size_t n) {
        std::vector<std::pair<int, std::string>> order;
        for (const auto& [k, v] : counts)
            order.push_back({v, k});
        std::sort(order.rbegin(), order.rend());
        order.resize(std::min(order.size(), n));
        return order;
    };
    if (asJson)
    {
        auto out = json::Value::object();
        out.set("moves_owed", json::Value(double(owed.size())));
        out.set("topics", json::Value(double(topics.size())));
        out.set("kinds", json::Value(double(kinds.size())));
        out.set("residents_covered", json::Value(double(people)));
        out.set("bands", json::Value(double(bands.size())));
        out.set("cells", json::Value(double(cells)));
        out.set("holes", json::Value(double(holes)));
        out.set("hole_rate", json::Value(cells ? double(holes) / double(cells) : 0.0));
        out.set("core_holes", json::Value(double(coreHoles)));
        auto byMove = json::Value::object();
        for (const auto& [m, t] : tally)
        {
            auto row = json::Value::object();
            row.set("core", json::Value(std::find(Core.begin(), Core.end(), m) != Core.end()));
            row.set("lines", json::Value(double(lines.count(m) ? lines.at(m).size() : 0)));
            row.set("cells", json::Value(double(t.cells)));
            row.set("holes", json::Value(double(t.holes)));
            auto topicsOut = json::Value::object();
            for (const auto& [n, name] : top(t.byTopic, 8))
                topicsOut.set(name, json::Value(double(n)));
            row.set("worst_topics", topicsOut);
            auto bandsOut = json::Value::object();
            for (const auto& [name, n] : t.byBand)
                bandsOut.set(name, json::Value(double(n)));
            row.set("by_band", bandsOut);
            auto ex = json::Value::array();
            for (const auto& e : t.examples)
                ex.push(json::Value(e));
            row.set("examples", ex);
            byMove.set(m, row);
        }
        out.set("by_move", byMove);
        std::cout << json::dump(out) << "\n";
        return 0;
    }
    std::cout << "coverage (doc 84 §15.4): " << owed.size() << " moves a line can owe x " << topics.size() << " topics x "
              << kinds.size() << " replier kinds (" << people << " residents) x " << bands.size() << " bands = " << cells
              << " cells; " << holes << " holes (" << (cells ? 100.0 * double(holes) / double(cells) : 0.0) << "%), "
              << coreHoles << " in the core moves\n";
    std::vector<std::string> order(Core.begin(), Core.end());
    for (const auto& [m, t] : tally)
        if (std::find(Core.begin(), Core.end(), m) == Core.end())
            order.push_back(m);
    for (const auto& m : order)
    {
        const auto& t = tally[m];
        const bool core = std::find(Core.begin(), Core.end(), m) != Core.end();
        std::cout << (core ? "core " : "     ") << m << ": " << (lines.count(m) ? lines.at(m).size() : 0) << " move lines, "
                  << t.holes << " of " << t.cells << " cells a hole";
        if (t.holes)
        {
            std::cout << "; worst topics";
            for (const auto& [n, name] : top(t.byTopic, 3))
                std::cout << " " << name << " " << n;
            std::cout << "; by band";
            for (const auto& [name, n] : t.byBand)
                std::cout << " " << name << " " << n;
        }
        std::cout << "\n";
        for (const auto& e : t.examples)
            std::cout << "        " << e << "\n";
    }
    return 0;
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

// --mantle (doc 84 Phase 9): every .mantle file named, or under a directory named, checked as the game loads one.
int checkMantles(talk::Library& lib, const std::string& data, const std::string& dialects, const std::vector<std::string>& paths,
                 bool asJson)
{
    std::string problem;
    if (paths.empty())
        return usage();
    if (!lib.load(data, problem))
    {
        std::cerr << "the talk library doesn't load:\n" << problem << "\n";
        return 2;
    }
    if (!dialects.empty())
        for (const auto& f : std::filesystem::directory_iterator(dialects))
            if (f.path().extension() == ".grammar")
            {
                std::ifstream in(f.path());
                std::stringstream text;
                text << in.rdbuf();
                if (!lib.setGrammar(f.path().stem().string(), text.str(), problem))
                    std::cerr << "the dialect " << f.path().stem().string() << " is refused (the file's kept):\n" << problem << "\n";
            }
    std::vector<std::string> files;
    for (const auto& p : paths)
        if (std::filesystem::is_directory(p))
        {
            for (const auto& f : std::filesystem::recursive_directory_iterator(p))
                if (f.is_regular_file() && f.path().extension() == ".mantle")
                    files.push_back(f.path().string());
        }
        else
            files.push_back(p);
    std::sort(files.begin(), files.end());
    int refused = 0;
    std::size_t entries = 0, items = 0;
    const auto quote = [](const std::string& s) {
        std::string out = "\"";
        for (const char c : s)
        {
            if (c == '"' || c == '\\')
                out += '\\', out += c;
            else if (c == '\n')
                out += "\\n";
            else if (static_cast<unsigned char>(c) < 0x20)
                out += ' ';
            else
                out += c;
        }
        return out + "\"";
    };
    if (asJson)
        std::cout << "[";
    bool first = true;
    for (const auto& file : files)
    {
        std::ifstream in(file);
        if (!in)
        {
            std::cout << (asJson ? "" : "REFUSED " + file + "\ncan't be read\n");
            ++refused;
            continue;
        }
        std::stringstream text;
        text << in.rdbuf();
        talk::Mantle m;
        const bool ok = lib.checkMantle({"", text.str(), file}, m, problem);
        refused += ok ? 0 : 1;
        entries += m.entries;
        items += m.items.size();
        if (asJson)
        {
            std::cout << (first ? "" : ",") << "\n {\"file\": " << quote(file) << ", \"id\": " << quote(m.id) << ", \"ok\": "
                      << (ok ? "true" : "false") << ", \"state\": " << quote(m.state) << ", \"revision\": " << m.revision
                      << ", \"items\": " << m.items.size() << ", \"entries\": " << m.entries << ", \"problems\": [";
            if (!ok)
            {
                std::istringstream lines(problem);
                bool one = true;
                for (std::string l; std::getline(lines, l);)
                    std::cout << (one ? "" : ", ") << quote(l), one = false;
            }
            std::cout << "], \"stances\": {";
            bool one = true;
            for (const auto& [issue, st] : m.stances)
                std::cout << (one ? "" : ", ") << quote(issue) << ": " << quote(st.side + " " + std::to_string(st.strength)), one = false;
            std::cout << "}, \"grafts\": " << m.grafts.size() << ", \"keys\": [";
            for (std::size_t i = 0; i < m.items.size(); ++i)
                std::cout << (i ? ", " : "") << quote(m.items[i].key);
            std::cout << "]}";
            first = false;
        }
        else
        {
            std::cout << (ok ? "ok      " : "REFUSED ") << file << " (" << (m.id.empty() ? "?" : m.id) << ", " << m.state << ", "
                      << m.items.size() << " items, " << m.entries << " rules and scenes)\n";
            if (!ok)
                std::cout << problem << "\n";
        }
    }
    if (asJson)
        std::cout << "\n]\n";
    else
        std::cout << files.size() << " mantles, " << items << " items, " << entries << " rules and scenes; " << refused << " refused\n";
    return refused ? 1 : 0;
}

int main(int argc, char** argv)
{
    std::string data = RATW_SOURCE_DIR "/Data/Talk", world;
    std::vector<std::string> args, validate;
    std::uint64_t seed = 1;
    int many = 0;
    double runHours = 0;
    std::string character, dialect, grammarFile, rulesFile;
    bool all = false, validating = false, residents = false, asJson = false, renderingTrees = false, covering = false;
    std::string profilesFile, dialectsDir, mantlesFrom;
    std::vector<std::string> mantles;
    bool checkingMantles = false;
    int listHoles = 0;
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
        else if (a == "--render-trees")
            renderingTrees = true;
        else if (a == "--coverage")
            covering = true;
        else if (a == "--profiles")
            profilesFile = next();
        else if (a == "--holes")
            listHoles = std::stoi(next());
        else if (a == "--json")
            asJson = true;
        else if (a == "--run")
            runHours = std::stod(next());
        else if (a == "--character")
            character = next();
        else if (a == "--dialect")
            dialect = next();
        else if (a == "--grammar")
            grammarFile = next();
        else if (a == "--rules")
            rulesFile = next();
        else if (a == "--mantle")
            checkingMantles = true;
        else if (a == "--dialects")
            dialectsDir = next();
        else if (a == "--mantles-from")
            mantlesFrom = next();
        else if (checkingMantles)
            mantles.push_back(a);
        else if (validating)
            validate.push_back(a);
        else
            args.push_back(a);
    }
    talk::Library lib;
    std::string problem;
    if (!dialect.empty())
    {
        if (!lib.load(data, problem))
        {
            std::cerr << "the talk library doesn't load:\n" << problem << "\n";
            return 1;
        }
        std::ifstream in(grammarFile);
        std::stringstream text;
        text << in.rdbuf();
        if (!lib.setGrammar(dialect, text.str(), problem))
        {
            std::cout << "REFUSED the dialect\n" << problem << "\n";
            return 1;
        }
        std::cout << "ok      " << dialect << "\n";
        return 0;
    }
    if (!character.empty())
    {
        if (!lib.load(data, problem))
        {
            std::cerr << "the talk library doesn't load:\n" << problem << "\n";
            return 1;
        }
        const auto read = [](const std::string& file) {
            std::ifstream in(file);
            std::stringstream text;
            text << in.rdbuf();
            return text.str();
        };
        bool ok = true;
        if (!grammarFile.empty())
        {
            const auto layer = talk::Library::personalLayer(character);
            if (!lib.setGrammar(layer, "grammar " + layer + "\n" + read(grammarFile), problem))
                std::cout << "REFUSED their words\n" << problem << "\n", ok = false;
        }
        if (!rulesFile.empty() && !lib.setCharacter(character, read(rulesFile), problem))
            std::cout << "REFUSED their lines\n" << problem << "\n", ok = false;
        if (ok)
            std::cout << "ok      " << character << "\n";
        return ok ? 0 : 1;
    }
    if (checkingMantles)
        return checkMantles(lib, data, dialectsDir, mantles, asJson);
    if (renderingTrees)
    {
        if (!lib.load(data, problem))
        {
            std::cerr << "the talk library doesn't load:\n" << problem << "\n";
            return 2;
        }
        return renderTrees(lib, seed, asJson);
    }
    if (covering)
    {
        if (!lib.load(data, problem))
        {
            std::cerr << "the talk library doesn't load:\n" << problem << "\n";
            return 1;
        }
        return coverage(lib, data, profilesFile.empty() ? data + "/profiles.json" : profilesFile, asJson, listHoles);
    }
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
            const auto ext = std::filesystem::path(file).extension();
            const bool ok = ext == ".grammar" ? lib.parseGrammar(text.str(), name, problem)
                          : ext == ".tree"    ? lib.parseTree(text.str(), name, problem)   // (A dialogue tree: doc 83.)
                                              : lib.parse(text.str(), name, problem);
            std::cout << (ok ? "ok      " : "REFUSED ") << file << "\n";
            if (!ok)
                std::cout << problem << "\n", ++refused;
        }
        // What the files' lines say they do (doc 84, Phase 2): written moves, moves by an answer concept's alias, none.
        int written = 0, aliased = 0, none = 0;
        for (const auto& e : lib.entries())
            (e.aliasMoves ? aliased : std::any_of(e.does.begin(), e.does.end(), [](const auto& t) { return !t.empty(); }) ? written : none) += 1;
        if (!lib.moves().empty() && lib.size() > 0)
            std::cout << "moves: " << written << " entries say what they do, " << aliased << " by their concept's alias, " << none
                      << " not yet\n";
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
    o.mantlesFrom = mantlesFrom;                         // (Doc 84 Phase 9: mantles from files, not the world's rows.)
    o.voiceData = RATW_SOURCE_DIR "/Data/Voice";       // (The router's tones: a resident's tone fact.)
    o.scratch = true;
    o.forkSnapshots = false;
    o.savePath = "/tmp/ratw-talk-check-" + std::to_string(::getpid()) + ".json";
    if (!std::filesystem::is_directory(world))
    {
        // A world in the database: a registered key (or atlas, live), as the registry says.
        const auto key = worldDb("resolve '" + world + "'");
        if (key.empty())
        {
            std::cerr << "'" << world << "' is neither an export directory nor a registered world (python3 tools/world_db.py list)\n";
            return 2;
        }
        o.database = key;
        o.conninfo = conninfo(key);
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
            if (asJson)
            {
                // One line a resident, their facts as the library sees them (tools/talk_seed.py reads these).
                auto line = json::Value::object();
                for (const auto& [k, v] : q.a.facts)
                    line.add(k, v);
                // Where they live as the server places them, and the origin the game works out alone (World::talkOrigin,
                // before any live.npc_voice row): tools/dialect_check.py and talk_seed.py --reorigin read these (doc 83,
                // Phase 4). Underscored, so no one takes them for the library's facts.
                const auto* life = g.world().society().resident(id);
                const auto homeCell = life && !life->homeCell.empty() ? life->homeCell : e.cellId;
                std::string from, accent;
                g.world().talkOrigin(id, from, accent);
                line.add("_home_cell", homeCell);
                line.add("_home", g.world().townIdOf(homeCell));
                line.add("_origin_from", from);
                line.add("_origin_accent", accent);
                line.add("_work_label", g.world().society().spec(id)->workLabel);
                if (const auto* post = g.world().society().jobOf(id))
                    line.add("_post_title", post->title), line.add("_post_role", post->role);
                std::cout << json::dump(line) << "\n";
                continue;
            }
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
        if (!key.empty() || asJson)
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
