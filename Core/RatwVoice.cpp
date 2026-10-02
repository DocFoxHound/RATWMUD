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
    library_.clear();
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
        for (const auto& [tone, list] : router["openers"].fields())
            for (const auto& o : list.items())
                openers_[tone].push_back(o.asString(""));
        for (const auto& [tone, list] : router["tones"].fields())
            for (const auto& w : list.items())
                tones_[tone].push_back(lowerAscii(w.asString("")));
        for (const auto& [intent, bands] : router["lines"].fields())
            for (const auto& [band, tones] : bands.fields())
                for (const auto& [tone, list] : tones.fields())
                    for (const auto& l : list.items())
                        lines_[intent][band][tone].push_back(l.asString(""));
    }
    if (!problem.empty())
        return false;
    json::Value library;
    if (readJson(directory + "/library.json", library, problem))
        for (const auto& e : library.array("exchanges"))
        {
            Exchange x{e.string("kind"), e.string("band", "any"), e.string("tone", "plain"), {}};
            for (const auto& line : e.array("lines"))
                if (line.items().size() == 2)
                    x.lines.push_back({line.items()[0].asString("") == "a" ? 0 : 1, line.items()[1].asString("")});
            bool both[2] = {false, false};
            for (const auto& [who, text] : x.lines)
                both[who] = true;
            if (both[0] && both[1] && x.lines.size() <= 4 && !x.kind.empty())
                library_.push_back(std::move(x));
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

std::string Rules::fill(const std::string& text, const std::map<std::string, std::string>& facts, bool& complete)
{
    std::string out;
    complete = true;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] != '{')
        {
            out += text[i];
            continue;
        }
        const auto end = text.find('}', i);
        if (end == std::string::npos)
        {
            out += text.substr(i);
            break;
        }
        const auto key = text.substr(i + 1, end - i - 1);
        const auto found = facts.find(key);
        if (found == facts.end() || (found->second.empty() && key != "opener"))
            complete = false;
        else
            out += found->second;
        i = end;
    }
    return out;
}

std::string Rules::line(const std::string& intent, const std::string& band, const std::string& tone,
                        std::map<std::string, std::string> facts, std::uint64_t seed, const std::string& avoid) const
{
    const auto bands = lines_.find(intent);
    if (bands == lines_.end())
        return {};
    if (!facts.count("opener"))
    {
        const auto o = openers_.count(tone) ? openers_.at(tone) : openers_.count("plain") ? openers_.at("plain")
                                                                                           : std::vector<std::string>{""};
        facts["opener"] = o.empty() ? "" : o[seed % o.size()];
    }
    // "Well, " runs on into the answer: "Well, a meal is..." (but "Well, I keep shop...").
    if (const auto& opener = facts["opener"]; opener.size() >= 2 && opener.compare(opener.size() - 2, 2, ", ") == 0)
        if (auto& answer = facts["answer"]; !answer.empty() && answer.rfind("I ", 0) != 0 && answer.rfind("I'", 0) != 0)
            answer[0] = char(std::tolower(static_cast<unsigned char>(answer[0])));
    for (const auto* b : {band.c_str(), "any"})
    {
        const auto tones = bands->second.find(b);
        if (tones == bands->second.end())
            continue;
        for (const auto* t : {tone.c_str(), "plain"})
        {
            const auto list = tones->second.find(t);
            if (list == tones->second.end())
                continue;
            std::vector<std::string> usable;
            for (const auto& l : list->second)
            {
                bool complete = false;
                auto text = fill(l, facts, complete);
                if (complete && !text.empty())
                    usable.push_back(std::move(text));
            }
            if (usable.size() > 1)
                usable.erase(std::remove(usable.begin(), usable.end(), avoid), usable.end());
            if (!usable.empty())
            {
                auto text = usable[seed % usable.size()];
                if (!text.empty())
                    text[0] = char(std::toupper(static_cast<unsigned char>(text[0])));
                return text;
            }
        }
    }
    return {};
}

Exchange Rules::exchange(const std::string& kind, const std::string& band, const std::map<std::string, std::string>& facts,
                         std::uint64_t seed, std::set<std::size_t>& recent) const
{
    std::vector<std::size_t> fitting, fresh;
    for (std::size_t i = 0; i < library_.size(); ++i)
    {
        const auto& x = library_[i];
        if (x.kind != kind || (x.band != band && x.band != "any"))
            continue;
        bool complete = true;
        for (const auto& [who, text] : x.lines)
        {
            bool filled = false;
            fill(text, facts, filled);
            complete &= filled;
        }
        if (!complete)
            continue;
        fitting.push_back(i);
        if (!recent.count(i))
            fresh.push_back(i);
    }
    const auto& from = fresh.empty() ? fitting : fresh;
    if (from.empty())
        return {};
    const auto chosen = from[seed % from.size()];
    recent.insert(chosen);
    if (recent.size() > 8)
        recent.erase(recent.begin());
    Exchange out = library_[chosen];
    for (auto& [who, text] : out.lines)
    {
        bool complete = false;
        text = fill(text, facts, complete);
    }
    return out;
}
} // namespace ratw::voice
