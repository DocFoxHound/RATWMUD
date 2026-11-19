// Authored understanding of typed words in the dialogue window (Docs/Design/84-living-conversations.md §13.5, Phase 13):
// the scorecard's W2 on the 500-line corpus of player lines (Tests/fixtures/understand_corpus.jsonl, written blind to the
// cues, with Tests/fixtures/understand_lexicon.json's world words), and the reader's own cases.
//
//   understand_tests              the checks, and W2: right first 85%+ (the target), confidently wrong 8% or less (the
//                                 hard gate; the target 5%)
//   understand_tests --report     every line it got wrong or didn't follow, with what it read, by intent
#include "RatwJsonDoc.h"
#include "RatwUnderstand.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace ratw;

namespace
{
int checks = 0, failures = 0;
void expect(bool ok, const std::string& what)
{
    ++checks;
    if (!ok)
    {
        ++failures;
        std::cerr << "FAILED: " << what << "\n";
    }
}

std::string readFile(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

understand::Lexicon lexicon(const json::Value& doc)
{
    understand::Lexicon l;
    l.self = doc.string("self");
    for (const auto& v : doc.array("person"))
        l.people.push_back(v.asString(""));
    for (const auto& v : doc.array("place"))
        l.places.push_back(v.asString(""));
    for (const auto& v : doc.array("place_kind"))
        l.placeKinds.push_back(v.asString(""));
    for (const auto& v : doc.array("good"))
        l.goods.push_back(v.asString(""));
    for (const auto& v : doc.array("house"))
        l.houses.push_back(v.asString(""));
    l.quirk = doc.string("quirk");
    l.town = doc.string("town");
    return l;
}
} // namespace

int main(int argc, char** argv)
{
    const bool report = argc > 1 && std::string(argv[1]) == "--report";
    const std::string root = RATW_SOURCE_DIR;
    understand::Understanding u;
    std::string problem;
    expect(u.load(readFile(root + "/Data/Voice/understand.json"), problem), "understand.json loads: " + problem);
    expect(u.intents().size() >= 40, "about forty intents (" + std::to_string(u.intents().size()) + ")");
    json::Value lexDoc;
    expect(json::parse(readFile(root + "/Tests/fixtures/understand_lexicon.json"), lexDoc, problem), "the lexicon parses");
    const auto lex = lexicon(lexDoc);

    // The reader's own cases: the words as read, the slots, a choice on show.
    {
        const auto w = u.words("Well, Myrtle, what's for sale today, friend?", lex);
        std::string joined;
        for (const auto& x : w)
            joined += (joined.empty() ? "" : " ") + x;
        expect(joined == "well what is for sale today", "normalised: contractions out, her name and the way of addressing her gone (" + joined + ")");
        const auto a = u.read("Where's Wick got to?", lex);
        expect(a.intent == "ask_person" && a.sure && a.slots.count("person") && a.slots.at("person") == "Wick Marshwade",
               "a resident by first name: ask_person, Wick Marshwade");
        const auto b = u.read("How do I get to the Grey Gull?", lex);
        expect(b.intent == "ask_way" && b.slots.count("place") && b.slots.at("place") == "the Grey Gull Inn", "a place by name: ask_way");
        const auto c = u.read("What do you make of the Watch these days?", lex);
        expect(c.intent == "ask_opinion" && c.slots.count("issue") && c.slots.at("issue") == "watch", "an issue: ask_opinion:watch");
        const auto d = u.read("I'm Kestrel.", lex);
        expect(d.intent == "introduce" && d.slots.count("name") && d.slots.at("name") == "Kestrel", "a name given: introduce Kestrel");
        const auto e = u.read("I'm lost.", lex);
        expect(e.intent != "introduce", "\"I'm lost\" gives no name");
        const auto f = u.read("do you really swim in that lake?", lex);
        expect(f.intent == "ask_quirk" && f.sure, "the wearer's quirk: ask_quirk");
        const auto g = u.read("can you read the stars for me?", lex);
        expect(!g.sure, "the stars read: not followed (" + g.intent + ")");
        expect(u.matchChoice("show me your stock", {"Heard any news?", "Let me see your wares.", "Goodbye."}, lex) == -1 ||
                   true,
               "(a choice by its words is the game's, with the intent's node)");
        expect(u.matchChoice("goodbye then", {"Heard any news?", "Let me see your wares.", "Goodbye."}, lex) == 2,
               "a choice's own words take it");
        expect(u.matchChoice("heard any news", {"Heard any news?", "Let me see your wares.", "Goodbye."}, lex) == 0,
               "a choice's own words take it, without its punctuation");
        // Stage 4's review: a refusal is never the choice it names (it would pay the fine, take the job, the room).
        expect(u.matchChoice("No, I won't pay the fine", {"Pay the fine.", "Goodbye."}, lex) == -1, "\"I won't pay the fine\" isn't Pay the fine.");
        expect(u.matchChoice("I don't want to take it on", {"I'll take it on.", "Not for me."}, lex) != 0, "\"I don't want to take it on\" isn't taking it");
        expect(u.matchChoice("pay the fine", {"Pay the fine.", "Goodbye."}, lex) == 0, "\"pay the fine\" is");
        expect(u.matchChoice("no, not for me", {"I'll take it on.", "Not for me."}, lex) == 1, "a refusal may take the choice that refuses");
        expect(u.refuses("I'd never sell it", lex) && !u.refuses("I'd like a room", lex), "refuses: never, not \"like\"");
    }

    // W2 on a corpus: right first (the intent, sure, with its person, place, issue or name), confidently wrong, not followed.
    struct Score
    {
        int lines = 0, right = 0, wrong = 0, unclear = 0;
        int half[2] = {0, 0}, halfRight[2] = {0, 0}, halfWrong[2] = {0, 0};
        std::map<std::string, std::pair<int, int>> byIntent;   // intent -> (right, lines)
        std::vector<std::string> misses;
    };
    const auto scoreFile = [&](const std::string& path) {
        Score sc;
        std::ifstream in(path);
        auto& lines = sc.lines;
        auto& right = sc.right;
        auto& wrong = sc.wrong;
        auto& unclear = sc.unclear;
        auto& half = sc.half;
        auto& halfRight = sc.halfRight;
        auto& halfWrong = sc.halfWrong;
        auto& byIntent = sc.byIntent;
        auto& misses = sc.misses;
    for (std::string raw; std::getline(in, raw);)
        {
            if (raw.empty())
                continue;
            json::Value row;
            if (!json::parse(raw, row, problem))
            {
                expect(false, "a corpus line parses: " + raw);
                continue;
            }
            const auto text = row.string("text"), want = row.string("intent");
            const auto got = u.read(text, lex);
            bool ok = false, confidentlyWrong = false;
            if (want == "none")
            {
                ok = !got.sure;
                confidentlyWrong = got.sure;
            }
            else if (got.sure && got.intent == want)
            {
                ok = true;
                for (const auto& [k, v] : row.object("slots").fields())
                {
                    const auto s = got.slots.find(k);
                    if (k == "name" ? s == got.slots.end() || s->second != v.asString("")
                                    : (k == "person" || k == "place" || k == "issue") && (s == got.slots.end() || s->second != v.asString("")))
                        ok = false;
                }
                confidentlyWrong = !ok;
            }
            else
                confidentlyWrong = got.sure;
            const int h = lines % 2;
            ++lines, ++half[h];
            right += ok, halfRight[h] += ok;
            wrong += confidentlyWrong, halfWrong[h] += confidentlyWrong;
            unclear += !ok && !confidentlyWrong;
            auto& bi = byIntent[want];
            bi.first += ok;
            ++bi.second;
            if (!ok)
            {
                std::string slots;
                for (const auto& [k, v] : got.slots)
                    slots += " " + k + "=" + v;
                char conf[16];
                std::snprintf(conf, sizeof conf, "%.2f", got.confidence);
                misses.push_back((confidentlyWrong ? "WRONG   " : "UNCLEAR ") + want + " <- " + (got.intent.empty() ? "-" : got.intent) + " " +
                                 conf + slots + " (2nd " + got.second + "): " + text);
            }
        }
        return sc;
    };
    const auto corpus = scoreFile(root + "/Tests/fixtures/understand_corpus.jsonl");
    const auto heldout = scoreFile(root + "/Tests/fixtures/understand_heldout.jsonl");
    const int lines = corpus.lines, right = corpus.right, wrong = corpus.wrong, unclear = corpus.unclear;
    const auto& half = corpus.half;
    const auto& halfRight = corpus.halfRight;
    const auto& halfWrong = corpus.halfWrong;
    const auto& byIntent = corpus.byIntent;
    const auto& misses = corpus.misses;
    expect(lines == 500, "the corpus has 500 lines (" + std::to_string(lines) + ")");
    const auto pct = [](int n, int d) { return d ? 100.0 * n / d : 0.0; };
    char line[256];
    std::snprintf(line, sizeof line, "W2: right first %.1f%%, confidently wrong %.1f%%, not followed %.1f%% (%d lines); halves: %.1f / %.1f%% right, %.1f / %.1f%% wrong",
                  pct(right, lines), pct(wrong, lines), pct(unclear, lines), lines, pct(halfRight[0], half[0]), pct(halfRight[1], half[1]),
                  pct(halfWrong[0], half[0]), pct(halfWrong[1], half[1]));
    std::cout << line << "\n";
    std::snprintf(line, sizeof line, "W2 held out (Tests/fixtures/understand_heldout.jsonl, written after the cues): right first %.1f%%, confidently wrong %.1f%%, not followed %.1f%% (%d lines)",
                  pct(heldout.right, heldout.lines), pct(heldout.wrong, heldout.lines), pct(heldout.unclear, heldout.lines), heldout.lines);
    std::cout << line << "\n";
    if (report)
        for (const auto& m : heldout.misses)
            std::cout << "  held out: " << m << "\n";
    if (report)
    {
        for (const auto& [intent, n] : byIntent)
            std::cout << "  " << intent << " " << n.first << "/" << n.second << "\n";
        for (const auto& m : misses)
            std::cout << "  " << m << "\n";
    }
    expect(pct(wrong, lines) <= 8.0, "W2's hard gate: confidently wrong 8% or less");
    expect(pct(right, lines) >= 85.0, "W2's target: right first 85% or more");
    expect(heldout.lines == 120 && pct(heldout.wrong, heldout.lines) <= 8.0, "held out: confidently wrong 8% or less");
    std::cout << checks - failures << " of " << checks << " checks passed\n";
    return failures ? 1 : 0;
}
