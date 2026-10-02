// Cheaper NPC voices (Core/RatwVoice.h; Docs/Design/28-ai-cost.md): the speech router against its corpus of player
// lines, tones, the choice of lines, and the exchange library.
#include "RatwJsonDoc.h"
#include "RatwVoice.h"

#include <cstdlib>
#include <fstream>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
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
const std::string Dir = RATW_SOURCE_DIR "/Data/Voice";
voice::Rules rules()
{
    voice::Rules r;
    std::string problem;
    expect(r.load(Dir, problem) && r.routes(), "The voice data loads: " + problem);
    return r;
}

void theCorpus()
{
    const auto r = rules();
    std::ifstream in(Dir + "/corpus.json");
    std::stringstream text;
    text << in.rdbuf();
    json::Value corpus;
    std::string error;
    expect(json::parse(text.str(), corpus, error), "The corpus parses: " + error);
    int answered = 0, total = 0;
    for (const auto& line : corpus.array("lines"))
    {
        const auto said = line.string("say");
        const auto got = r.intent(said, "Wren");
        expect(got.id == line.string("intent"), "\"" + said + "\" goes to " + (got.id.empty() ? "a model" : got.id) +
                                                    ", not " + (line.string("intent").empty() ? "a model" : line.string("intent")));
        if (line.has("target"))
            expect(got.target == line.string("target"), "\"" + said + "\" asks after \"" + got.target + "\"");
        answered += !got.id.empty();
        ++total;
    }
    expect(total >= 40 && answered > total / 3 && answered < total * 3 / 4, "The corpus has both kinds in good measure");
}

void normalising()
{
    expect(voice::Rules::normalise("\"What's for sale, Wren?\"", "Wren") == "what is for sale", "Quotes, names, contractions");
    expect(voice::Rules::normalise("Please, where's Sorrel?!", "Old Rook") == "where is sorrel", "Please and punctuation");
}

void tonesAndLines()
{
    const auto r = rules();
    expect(r.toneOf("Meticulous and unhurried; he feeds the plaza sparrows.") == "formal", "Meticulous is formal");
    expect(r.toneOf("A gruff, kind-hearted old carter.") == "gruff", "The first such word decides");
    expect(r.toneOf("Unremarkable.") == "plain", "Plain when nothing fits");
    const std::map<std::string, std::string> known{{"player", "Ada"}};
    std::set<std::string> seen;
    std::string last;
    for (std::uint64_t seed = 0; seed < 12; ++seed)
    {
        const auto line = r.line("greet", "known", "warm", known, seed, last);
        expect(!line.empty() && line.find("Ada") != std::string::npos && line != last, "A warm greeting for Ada, never twice running");
        seen.insert(line);
        last = line;
    }
    expect(seen.size() >= 2, "and more than one of them");
    expect(r.line("greet", "stranger", "plain", {{"greeting", "Welcome to the Fair Scale."}}, 0) == "Welcome to the Fair Scale.",
           "A stranger may hear the NPC's own greeting");
    expect(r.line("greet", "stranger", "plain", {}, 0).find("{") == std::string::npos, "Lines with unfilled blanks are left out");
    const auto price = r.line("trade", "any", "formal", {{"answer", "A meal is 6 pennies."}}, 1);
    expect(price.find("A meal is 6 pennies.") != std::string::npos, "The game's answer, opened in the NPC's tone: " + price);
    expect(r.line("trade", "any", "plain", {}, 0).empty(), "No answer, no line: a model speaks instead");
    for (std::uint64_t seed = 0; seed < 6; ++seed)
    {
        const auto plain = r.line("trade", "any", "plain", {{"answer", "A meal is 6 pennies."}}, seed);
        expect(plain == "A meal is 6 pennies." || plain == "Well, a meal is 6 pennies.", "An opener runs on in lower case: " + plain);
        const auto mine = r.line("job", "any", "plain", {{"answer", "I'm Wren."}}, seed);
        expect(mine.find("i'm") == std::string::npos, "but \"I\" stays itself: " + mine);
    }
    expect(r.line("nonsense", "any", "plain", {{"answer", "x"}}, 0).empty(), "An intent with no lines says nothing");
}

void theLibrary()
{
    const auto r = rules();
    expect(r.libraryEntries() >= 20, "The library has a start");
    std::set<std::size_t> recent;
    const std::map<std::string, std::string> facts{{"teller", "Sorrel"}, {"listener", "Fennel"}, {"subject", "Ada"},
                                                   {"claim", "stole from Wren"}};
    std::set<std::string> openings;
    for (std::uint64_t seed = 0; seed < 6; ++seed)
    {
        const auto x = r.exchange("gossip", "friends", facts, seed, recent);
        expect(x.lines.size() >= 2 && x.lines[0].first == 0, "A gossip exchange for friends, opened by the teller");
        for (const auto& [who, text] : x.lines)
            expect(text.find('{') == std::string::npos, "with every blank filled: " + text);
        openings.insert(x.lines[0].second);
    }
    expect(openings.size() >= 3, "and the place doesn't hear the same one again and again");
    expect(r.exchange("gossip", "friends", {{"teller", "Sorrel"}}, 0, recent).lines.empty(), "Not without the facts it needs");
    expect(r.exchange("quarrel", "friends", facts, 0, recent).lines.empty(), "Friends don't quarrel from the library");
}

void badData()
{
    const std::string dir = "/tmp/ratw-voice-test-" + std::to_string(::getpid());
    std::system(("mkdir -p " + dir).c_str());
    std::ofstream(dir + "/router.json") << R"({"intents": [{"id": "greet", "patterns": ["(unclosed"]}]})";
    voice::Rules r;
    std::string problem;
    expect(!r.load(dir, problem) && !r.routes() && problem.find("greet") != std::string::npos, "A broken pattern is refused at load");
    std::system(("rm -rf " + dir).c_str());
    voice::Rules none;
    problem.clear();
    expect(none.load("/nonexistent", problem) && !none.routes() && none.libraryEntries() == 0,
           "No voice data: the router is off, and models answer as before");
}
} // namespace

int main()
{
    try
    {
        theCorpus();
        normalising();
        tonesAndLines();
        theLibrary();
        badData();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
    std::cout << "voice tests passed (" << checks << " checks)\n";
    return 0;
}
