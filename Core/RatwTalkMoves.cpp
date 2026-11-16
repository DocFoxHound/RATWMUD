// Dialogue moves (RatwTalk.h; Docs/Design/84-living-conversations.md §7.2, Phase 2): Data/Talk/moves.json read and
// checked, and what a move answers and owes.
#include "RatwTalk.h"
#include "RatwJsonDoc.h"

#include <algorithm>
#include <cmath>
#include <deque>

namespace ratw::talk
{
namespace
{
const std::set<std::string> Families{"opening", "inform", "react", "ask", "stance", "social", "requests", "topic", "repair",
                                     "party", "closing", "ground"};
const std::set<std::string> Shapes{"hedge", "account", "remedy"};
const std::set<std::string> Axes{"affinity", "trust", "respect", "fear", "familiarity"};
const std::set<std::string> Fields{"family", "first", "expects", "answers", "fallback", "insertion", "variants", "asserts",
                                   "denies", "dispreferred", "shape", "tone", "effects", "third_party", "ends", "weak",
                                   "optional", "stance", "max_extend", "about", "alias"};

bool lowerName(const std::string& s)
{
    return !s.empty() && s.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_.") == std::string::npos &&
           s.front() != '.' && s.back() != '.' && s.find("..") == std::string::npos;
}
} // namespace

void Moves::clear()
{
    moves_.clear();
    aliases_.clear();
    tones_.clear();
    toneRules_.clear();
}

const Move* Moves::find(const std::string& name) const
{
    if (const auto m = moves_.find(name); m != moves_.end())
        return &m->second;
    // A variant: react.news.known is react.news's.
    const auto dot = name.rfind('.');
    if (dot == std::string::npos)
        return nullptr;
    const auto m = moves_.find(name.substr(0, dot));
    if (m == moves_.end())
        return nullptr;
    const auto& v = m->second.variants;
    return std::find(v.begin(), v.end(), name.substr(dot + 1)) != v.end() ? &m->second : nullptr;
}

std::string Moves::alias(const std::string& concept) const
{
    const auto a = aliases_.find(concept);
    return a == aliases_.end() ? std::string() : a->second;
}

bool Moves::tone(const std::string& t) const { return std::find(tones_.begin(), tones_.end(), t) != tones_.end(); }

std::string Moves::toneOf(const std::string& move) const
{
    const auto* m = find(move);
    return m && !m->tone.empty() ? m->tone : std::string("even");
}

bool Moves::meets(const std::string& done, const std::string& owed)
{
    return done == owed || (done.size() > owed.size() && done.compare(0, owed.size(), owed) == 0 && done[owed.size()] == '.');
}

bool Moves::kindOf(const std::string& done, const std::string& owed) const
{
    return done == owed || (meets(done, owed) && !moves_.count(done));
}

bool Moves::answers(const std::string& answer, const std::string& said) const
{
    const auto* m = find(answer);
    if (!m)
        return false;
    for (const auto& a : m->answers)
        if (a == "*" || kindOf(said, a))
            return true;
    // A variant answers what its move answers, and what is written for the variant itself.
    return false;
}

std::vector<std::string> Moves::chain(const std::string& move) const
{
    std::vector<std::string> out;
    std::deque<std::string> todo{move};
    while (!todo.empty())
    {
        auto m = todo.front();
        todo.pop_front();
        if (std::find(out.begin(), out.end(), m) != out.end())
            continue;
        out.push_back(m);
        if (const auto* found = find(m))
            for (const auto& f : found->fallback)
                todo.push_back(f);
    }
    return out;
}

bool Moves::read(const std::string& text, const std::set<std::string>& concepts, std::string& problem)
{
    clear();
    json::Value doc;
    std::string error;
    if (!json::parseUnique(text, doc, error) || !doc.isObject())
    {
        problem = "moves.json: " + (error.empty() ? "not a JSON object" : error);
        return false;
    }
    std::vector<std::string> errors;
    const auto fail = [&](const std::string& m) { errors.push_back("moves.json: " + m); };
    for (const auto& t : doc.array("tones"))
        if (const auto name = t.asString(""); lowerName(name) && name.find('.') == std::string::npos)
            tones_.push_back(name);
        else
            fail("a tone is a lower-case word");
    if (tones_.empty())
        fail("no tones");
    const auto list = [&](const std::string& move, const json::Value& m, const char* key, std::vector<std::string>& into) {
        const auto* v = m.find(key);
        if (!v)
            return;
        if (!v->isArray())
        {
            fail(move + ": " + key + " is a list");
            return;
        }
        for (const auto& item : v->items())
            if (const auto s = item.asString(""); !s.empty())
                into.push_back(s);
            else
                fail(move + ": " + key + " holds names");
    };
    std::map<std::string, std::string> aliasTo;
    for (const auto& [name, m] : doc.object("moves").fields())
    {
        if (!lowerName(name))
        {
            fail("'" + name + "' is not a move's name (lower-case words and dots)");
            continue;
        }
        if (!m.isObject())
        {
            fail(name + " is an object");
            continue;
        }
        for (const auto& [key, unused] : m.fields())
            if (!Fields.count(key))
                fail(name + ": no field '" + key + "'");
        if (m.has("alias"))
        {
            if (m.size() != 1 && !(m.size() == 2 && m.has("about")))
                fail(name + ": an alias is only {\"alias\": \"<move>\"}");
            if (!concepts.count(name))
                fail(name + ": an alias names a concept (settings.json); '" + name + "' isn't one");
            aliasTo[name] = m.string("alias");
            continue;
        }
        Move mv;
        mv.name = name;
        mv.family = m.string("family");
        if (!Families.count(mv.family))
            fail(name + ": family '" + mv.family + "' is not one of opening, inform, react, ask, stance, social, requests, "
                        "topic, repair, party, closing, ground");
        for (const auto& [key, flag] : {std::pair<const char*, bool*>{"first", &mv.first}, {"asserts", &mv.asserts},
                                        {"denies", &mv.denies}, {"dispreferred", &mv.dispreferred},
                                        {"third_party", &mv.thirdParty}, {"ends", &mv.ends}, {"weak", &mv.weak},
                                        {"optional", &mv.optional}, {"stance", &mv.stance}})
            if (const auto* v = m.find(key))
            {
                if (!v->isBool())
                    fail(name + ": " + key + " is true or false");
                *flag = v->asBool(false);
            }
        list(name, m, "expects", mv.expects);
        list(name, m, "answers", mv.answers);
        list(name, m, "fallback", mv.fallback);
        list(name, m, "insertion", mv.insertion);
        list(name, m, "variants", mv.variants);
        list(name, m, "shape", mv.shape);
        for (const auto& s : mv.shape)
            if (!Shapes.count(s))
                fail(name + ": shape '" + s + "' is not hedge, account or remedy");
        for (const auto& v : mv.variants)
            if (!lowerName(v) || v.find('.') != std::string::npos)
                fail(name + ": a variant is a lower-case word ('" + v + "')");
        mv.tone = m.string("tone");
        mv.maxExtend = int(m.number("max_extend", 0));
        for (const auto& [who, axes] : m.object("effects").fields())
        {
            if (who != "self" && who != "other")
                fail(name + ": effects are the speaker's (self) or the one spoken to's (other)");
            for (const auto& [axis, amount] : axes.fields())
                if (!Axes.count(axis) || !amount.isNumber() || std::abs(amount.asNumber()) > 5)
                    fail(name + ": effect " + who + "." + axis + " (affinity, trust, respect, fear, familiarity; a number, at most 5)");
                else
                    mv.effects[who][axis] = amount.asNumber();
        }
        if (!mv.first && mv.answers.empty() && !mv.thirdParty)
            fail(name + ": a move either opens (first) or answers something (answers)");
        moves_[name] = std::move(mv);
    }
    if (moves_.empty())
        fail("no moves");
    // Every name a move refers to is a move (or a move's variant); every tone known.
    for (const auto& [name, mv] : moves_)
    {
        for (const auto* names : {&mv.expects, &mv.fallback, &mv.insertion})
            for (const auto& other : *names)
                if (!find(other))
                    fail(name + ": '" + other + "' is not a move");
        for (const auto& other : mv.answers)
            if (other != "*" && !find(other))
                fail(name + ": answers '" + other + "', which is not a move");
        if (!mv.tone.empty() && !tone(mv.tone))
            fail(name + ": tone '" + mv.tone + "' is not one of the tones");
        for (const auto& f : mv.fallback)
            if (f == name)
                fail(name + ": falls back on itself");
        // What a move expects must answer it (or anything): an expectation nothing could meet is a slip.
        for (const auto& e : mv.expects)
            if (find(e) && !answers(e, name))
                fail(name + ": expects " + e + ", which doesn't answer it (its answers)");
    }
    for (const auto& [concept, move] : aliasTo)
    {
        if (moves_.count(concept))
            fail(concept + " is both a move and an alias");
        if (!find(move))
            fail(concept + ": alias '" + move + "' is not a move");
        aliases_[concept] = move;
    }
    // Tone rules: {"after": {"grave": {"blocks": ["light"], "beats": 2, "bridge": [...]}}}.
    for (const auto& [after, rule] : doc.object("tone_rules").object("after").fields())
    {
        ToneRule r;
        r.after = after;
        r.beats = int(rule.number("beats", 1));
        if (!tone(after))
            fail("tone_rules: '" + after + "' is not a tone");
        for (const auto& b : rule.array("blocks"))
            if (tone(b.asString("")))
                r.blocks.insert(b.asString(""));
            else
                fail("tone_rules: " + after + " blocks '" + b.asString("") + "', not a tone");
        for (const auto& b : rule.array("bridge"))
            if (find(b.asString("")))
                r.bridge.insert(b.asString(""));
            else
                fail("tone_rules: " + after + "'s bridge '" + b.asString("") + "' is not a move");
        if (r.beats < 1 || r.beats > 6)
            fail("tone_rules: " + after + ": beats is 1 to 6");
        toneRules_.push_back(std::move(r));
    }
    if (!find("ack"))
        fail("no ack: every fallback ends there");
    if (!errors.empty())
    {
        problem.clear();
        for (std::size_t i = 0; i < errors.size() && i < 20; ++i)
            problem += (i ? "\n" : "") + errors[i];
        if (errors.size() > 20)
            problem += "\n(" + std::to_string(errors.size() - 20) + " more)";
        clear();
        return false;
    }
    return true;
}
} // namespace ratw::talk
