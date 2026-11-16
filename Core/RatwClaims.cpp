// Claims and who says them (RatwClaims.h; Docs/Design/84 §7.7).
#include "RatwClaims.h"
#include "RatwJsonDoc.h"

#include <cctype>
#include <set>

namespace ratw::claims
{
namespace
{
struct Table
{
    std::vector<Kind> kinds;                      // In the file's order (the free words are matched in it).
    std::map<std::string, std::size_t> byId;
};
Table& table()
{
    static Table t;
    return t;
}
const std::set<std::string>& Points()
{
    static const std::set<std::string> p{"they", "victim", "to_victim", "self", "kin"};
    return p;
}
const std::set<std::string>& Blanks()
{
    static const std::set<std::string> b{"x.name", "x.first", "y.name", "y.first", "place.words", "claim.charge", "claim.text"};
    return b;
}
const std::string* blank(const std::string& name, const Words& w)
{
    return name == "x.name" ? &w.xName : name == "x.first" ? &w.xFirst : name == "y.name" ? &w.yName : name == "y.first" ? &w.yFirst
           : name == "place.words" ? &w.place : name == "claim.charge" ? &w.charge : name == "claim.text" ? &w.text : nullptr;
}
// The wording filled; "" if a blank it uses is empty, or the words look like an id.
std::string fill(const std::string& wording, const Words& w)
{
    std::string out;
    for (std::size_t at = 0; at < wording.size();)
    {
        if (wording[at] != '#')
        {
            out += wording[at++];
            continue;
        }
        const auto end = wording.find('#', at + 1);
        if (end == std::string::npos)
            return {};
        const auto* v = blank(wording.substr(at + 1, end - at - 1), w);
        if (!v || v->empty())
            return {};
        out += *v;
        at = end + 1;
    }
    return rawId(out) ? std::string() : out;
}
} // namespace

bool load(const std::string& text, std::string& problem)
{
    json::Value doc;
    std::string error;
    if (!json::parse(text, doc, error) || !doc.isObject() || !doc["kinds"].isObject())
    {
        problem = "claims.json: " + (error.empty() ? "no \"kinds\" object" : error);
        return false;
    }
    Table t;
    std::vector<std::string> errors;
    for (const auto& [id, j] : doc["kinds"].fields())
    {
        const auto what = "claims.json: " + id;
        if (!j.isObject())
        {
            errors.push_back(what + " is not an object");
            continue;
        }
        Kind k;
        k.id = id;
        k.family = j.string("family");
        k.topic = j.string("topic", "gossip");
        k.topicKind = j.string("topic_kind");
        k.grave = int(j.number("grave"));
        if (k.family.empty())
            errors.push_back(what + " has no family (topic.claim_kind)");
        for (const auto& [key, v] : j.fields())
        {
            if (!Points().count(key))
                continue;
            std::vector<std::string> wordings;
            for (const auto& one : v.isArray() ? v.items() : json::Array{v})
                if (one.isString() && !one.asString("").empty())
                    wordings.push_back(one.asString(""));
                else
                    errors.push_back(what + "." + key + ": a wording is a string of words");
            for (const auto& w : wordings)
                for (std::size_t at = w.find('#'); at != std::string::npos;)
                {
                    const auto end = w.find('#', at + 1);
                    if (end == std::string::npos)
                    {
                        errors.push_back(what + "." + key + ": an unclosed # in \"" + w + "\"");
                        break;
                    }
                    if (const auto name = w.substr(at + 1, end - at - 1); !Blanks().count(name))
                        errors.push_back(what + "." + key + ": #" + name + "# is no claim's blank");
                    at = w.find('#', end + 1);
                }
            k.said[key] = std::move(wordings);
        }
        if (!k.said.count("they"))
            errors.push_back(what + " has no `they` wording");
        for (const auto& [key, v] : j.fields())
            if (!Points().count(key) && key != "family" && key != "topic" && key != "topic_kind" && key != "grave" && key != "text")
                errors.push_back(what + ": unknown key \"" + key + "\"");
        const auto& m = j.object("text");
        for (const auto& e : m.array("exact"))
            k.exact.push_back(e.asString(""));
        for (const auto& s : m.array("starts"))
            k.starts.push_back(s.asString(""));
        t.byId[id] = t.kinds.size();
        t.kinds.push_back(std::move(k));
    }
    if (!errors.empty())
    {
        problem.clear();
        for (std::size_t i = 0; i < errors.size() && i < 20; ++i)
            problem += (i ? "\n" : "") + errors[i];
        return false;
    }
    table() = std::move(t);
    return true;
}

void clear() { table() = {}; }
bool loaded() { return !table().kinds.empty(); }

const Kind* find(const std::string& kind)
{
    const auto& t = table();
    const auto found = t.byId.find(kind);
    return found == t.byId.end() ? nullptr : &t.kinds[found->second];
}

std::string kindOfText(const std::string& claim)
{
    for (const auto& k : table().kinds)
    {
        for (const auto& e : k.exact)
            if (claim == e)
                return k.id;
        for (const auto& s : k.starts)
            if (!s.empty() && claim.rfind(s, 0) == 0)
                return k.id;
    }
    return {};
}

std::string family(const std::string& kind, const std::string& claimText)
{
    if (const auto* k = find(kind.empty() ? kindOfText(claimText) : kind); k && !k->family.empty())
        return k->family;
    if (kind.empty() && !loaded())
    {
        // (No table: the words as the ambient talk and the window both read them before Phase 1.)
        const auto& c = claimText;
        const auto starts = [&](const char* w) { return c.rfind(w, 0) == 0; };
        return c == "died" ? "death" : c == "married" ? "marriage"
               : starts("attacked") || c == "is violent" ? "violence"
               : starts("stole") || starts("tried to steal") ? "theft"
               : starts("was attacked") || starts("was robbed") ? "victim"
               : starts("is wanted") ? "wanted"
               : starts("raids the road") || starts("robs travellers") ? "bandit"
               : c == "keeps promises" ? "promise_kept" : c == "breaks promises" ? "promise_broken"
               : starts("was prowling") ? "prowling" : c == "is new in town" ? "newcomer" : "other";
    }
    return "other";
}

std::string said(const std::string& kind, const std::string& pov, const Words& w)
{
    const auto* k = find(kind);
    if (!k)
        return {};
    const auto found = k->said.find(pov);
    if (found == k->said.end())
        return {};
    for (const auto& wording : found->second)
        if (auto out = fill(wording, w); !out.empty())
            return out;
    return {};
}

std::string predicate(const std::string& kind, const Words& w)
{
    const auto* k = find(kind);
    if (!k || !k->said.count("they"))
        return {};
    static const std::string Lead = "#x.name# ";
    for (const auto& wording : k->said.at("they"))
        if (!fill(wording, w).empty())
            return wording.rfind(Lead, 0) == 0 ? fill(wording.substr(Lead.size()), w) : std::string();
    return {};
}

bool rawId(const std::string& words)
{
    // A run of letters, digits and underscores with an underscore after a letter: an id, not a word.
    for (std::size_t at = 0; at < words.size();)
    {
        const auto c = static_cast<unsigned char>(words[at]);
        if (!(std::isalnum(c) || c == '_'))
        {
            ++at;
            continue;
        }
        std::size_t end = at;
        bool letter = false, under = false;
        while (end < words.size() && (std::isalnum(static_cast<unsigned char>(words[end])) || words[end] == '_'))
        {
            if (words[end] == '_' && letter && end + 1 < words.size() && std::isalnum(static_cast<unsigned char>(words[end + 1])))
                under = true;
            letter |= std::isalpha(static_cast<unsigned char>(words[end])) != 0;
            ++end;
        }
        if (under)
            return true;
        at = end;
    }
    return false;
}
} // namespace ratw::claims
