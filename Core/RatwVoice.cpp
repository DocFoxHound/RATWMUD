// Cheaper NPC voices (RatwVoice.h; Docs/Design/28-ai-cost.md).
#include "RatwVoice.h"
#include "RatwJsonDoc.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>

namespace ratw::voice
{
namespace
{
bool readJson(const std::string& path, json::Value& out, std::string& problem)
{
    std::ifstream in(path);
    if (!in)
        return false;
    std::stringstream text;
    text << in.rdbuf();
    std::string error;
    if (!json::parse(text.str(), out, error) || !out.isObject())
    {
        problem = path + ": " + (error.empty() ? "not a JSON object" : error);
        return false;
    }
    return true;
}
std::string lowerAscii(std::string s)
{
    for (auto& c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
} // namespace

bool Rules::load(const std::string& directory, std::string& problem)
{
    intents_.clear();
    json::Value router;
    if (readJson(directory + "/router.json", router, problem))
    {
        maxWords_ = std::size_t(std::max(1.0, router.number("maxWords", 9)));
        for (const auto& intent : router.array("intents"))
            for (const auto& pattern : intent.array("patterns"))
            {
                try
                {
                    // Compiled once; a bad one is refused here, never on a player's words later.
                    intents_.push_back({intent.string("id"), std::regex(pattern.asString(""), std::regex::optimize)});
                }
                catch (const std::regex_error&)
                {
                    problem = "router.json: a pattern of " + intent.string("id") + " is not a regular expression";
                    intents_.clear();
                    return false;
                }
            }
        for (const auto& [tone, list] : router["tones"].fields())
            for (const auto& w : list.items())
                tones_[tone].push_back(lowerAscii(w.asString("")));
    }
    return problem.empty();
}

std::string Rules::normalise(const std::string& heard, const std::string& npcName)
{
    std::string s = lowerAscii(heard);
    for (auto& c : s)
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '\'')
            c = ' ';
    // Contractions spelt out, so the patterns need write each request only once.
    static const std::pair<const char*, const char*> spelt[] = {
        {"what's", "what is"}, {"where's", "where is"}, {"who's", "who is"}, {"how's", "how is"}, {"i'm", "i am"},
        {"you're", "you are"}, {"it's", "it is"}, {"that's", "that is"}, {"there's", "there is"}, {"when's", "when is"}};
    std::istringstream words(s);
    std::vector<std::string> kept;
    std::set<std::string> name;
    std::istringstream names(lowerAscii(npcName));
    for (std::string n; names >> n;)
        name.insert(n);
    for (std::string w; words >> w;)
    {
        while (!w.empty() && w.front() == '\'')
            w.erase(w.begin());
        while (!w.empty() && w.back() == '\'')
            w.pop_back();
        if (w.empty() || name.count(w) || w == "please")
            continue;
        for (const auto& [from, to] : spelt)
            if (w == from)
                w = to;
        kept.push_back(w);
    }
    std::string out;
    for (const auto& w : kept)
        out += (out.empty() ? "" : " ") + w;
    return out;
}

Intent Rules::intent(const std::string& heard, const std::string& npcName) const
{
    const auto text = normalise(heard, npcName);
    if (text.empty())
        return {};
    std::size_t words = 1;
    for (const char c : text)
        words += c == ' ';
    if (words > maxWords_)
        return {};                                  // Too much said for one plain request: a model answers.
    for (const auto& p : intents_)
    {
        std::smatch found;
        if (std::regex_match(text, found, p.regex))
        {
            Intent out{p.intent, {}};
            for (std::size_t g = found.size(); g-- > 1;)
                if (found[g].matched && found[g].length() > 1 && p.intent == "where")
                {
                    out.target = found[g].str();
                    if (out.target == " is")
                        continue;
                    break;
                }
            return out;
        }
    }
    return {};
}

std::string Rules::toneOf(const std::string& personality) const
{
    const auto text = lowerAscii(personality);
    std::string best = "plain";
    std::size_t first = std::string::npos;
    for (const auto& [tone, words] : tones_)
        for (const auto& w : words)
            if (const auto at = text.find(w); at != std::string::npos && at < first)
                first = at, best = tone;     // The first such word in the personality says it.
    return best;
}

} // namespace ratw::voice
