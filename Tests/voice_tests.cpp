// Cheaper NPC voices (Core/RatwVoice.h; Docs/Design/28-ai-cost.md): the speech router against its corpus of player
// lines, and tones (what is said back is the talk library's: Tests/talk_tests.cpp).
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

void tones()
{
    const auto r = rules();
    expect(r.toneOf("Meticulous and unhurried; he feeds the plaza sparrows.") == "formal", "Meticulous is formal");
    expect(r.toneOf("A gruff, kind-hearted old carter.") == "gruff", "The first such word decides");
    expect(r.toneOf("Unremarkable.") == "plain", "Plain when nothing fits");
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
    expect(none.load("/nonexistent", problem) && !none.routes(),
           "No voice data: the router is off, and models answer as before");
}
} // namespace

int main()
{
    try
    {
        theCorpus();
        normalising();
        tones();
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
