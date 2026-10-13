// Talk (RatwTalk.h; Docs/Design/62-talk.md).
#include "RatwTalk.h"
#include "RatwJsonDoc.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace ratw::talk
{
namespace
{
constexpr std::size_t LongestLine = 160;      // Characters, not counting the facts filled in.
constexpr int DeepestSymbol = 6;

std::string trim(const std::string& s)
{
    const auto a = s.find_first_not_of(" \t\r");
    if (a == std::string::npos)
        return {};
    return s.substr(a, s.find_last_not_of(" \t\r") - a + 1);
}

std::vector<std::string> split(const std::string& s, char by)
{
    std::vector<std::string> out;
    std::string part;
    std::istringstream in(s);
    while (std::getline(in, part, by))
        if (auto t = trim(part); !t.empty())
            out.push_back(t);
    return out;
}

std::vector<std::string> words(const std::string& s)
{
    std::vector<std::string> out;
    std::istringstream in(s);
    for (std::string w; in >> w;)
        out.push_back(w);
    return out;
}

// SplitMix64: the same seed picks the same alternative.
std::uint64_t mix(std::uint64_t x)
{
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

std::uint64_t hash(const std::string& s)
{
    std::uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : s)
        h = (h ^ c) * 1099511628211ULL;
    return h;
}

double unit(std::uint64_t r) { return double(r % 1000000) / 1000000.0; }

bool readFile(const std::filesystem::path& path, std::string& out)
{
    std::ifstream in(path);
    if (!in)
        return false;
    std::stringstream text;
    text << in.rdbuf();
    out = text.str();
    return true;
}

bool isPersonNs(const std::string& ns) { return ns == "a" || ns == "b" || ns == "self" || ns == "other"; }
bool isSharedNs(const std::string& ns) { return ns == "pair" || ns == "here" || ns == "topic" || ns == "world"; }

Ns personNs(const std::string& ns)
{
    return ns == "a" ? Ns::A : ns == "b" ? Ns::B : ns == "self" ? Ns::Self : Ns::Other;
}

Ns sharedNs(const std::string& ns)
{
    return ns == "pair" ? Ns::Pair : ns == "here" ? Ns::Here : ns == "topic" ? Ns::Topic : Ns::World;
}

const std::set<std::string> Modifiers{"capitalize", "a", "s"};
const std::set<std::string> Pronouns{"they", "them", "their", "theirs", "themself"};

std::string pronoun(const std::string& word, const std::string& sex)
{
    static const std::map<std::string, std::pair<const char*, const char*>> forms{
        {"they", {"he", "she"}}, {"them", {"him", "her"}}, {"their", {"his", "her"}}, {"theirs", {"his", "hers"}},
        {"themself", {"himself", "herself"}}};
    const auto f = forms.find(word);
    if (f == forms.end())
        return {};
    return sex == "male" ? f->second.first : sex == "female" ? f->second.second : word;
}

std::string modify(std::string s, const std::string& how)
{
    if (s.empty())
        return s;
    if (how == "capitalize")
    {
        s[0] = char(std::toupper(static_cast<unsigned char>(s[0])));
    }
    else if (how == "a")
    {
        const char c = char(std::tolower(static_cast<unsigned char>(s[0])));
        s = (std::string("aeiou").find(c) != std::string::npos ? "an " : "a ") + s;
    }
    else if (how == "s")
    {
        const auto ends = [&](const char* e) {
            const std::string t(e);
            return s.size() >= t.size() && s.compare(s.size() - t.size(), t.size(), t) == 0;
        };
        if (ends("y") && s.size() > 1 && std::string("aeiou").find(s[s.size() - 2]) == std::string::npos)
            s = s.substr(0, s.size() - 1) + "ies";
        else if (ends("s") || ends("x") || ends("ch") || ends("sh") || ends("z"))
            s += "es";
        else
            s += "s";
    }
    return s;
}

// A reference inside #...#: its parts, and the modifiers after them.
struct Ref
{
    std::vector<std::string> parts;
    std::vector<std::string> mods;
};

Ref readRef(const std::string& inside)
{
    Ref r;
    std::string part;
    std::istringstream in(inside);
    while (std::getline(in, part, '.'))
        r.parts.push_back(part);
    while (r.parts.size() > 1 && Modifiers.count(r.parts.back()))
    {
        r.mods.insert(r.mods.begin(), r.parts.back());
        r.parts.pop_back();
    }
    return r;
}

// Inserting words into a line: one that opens a sentence starts with a capital ("That grey stranger again?"), and
// "the {festival} bonfire" with "the Greening" says the once.
void insert(std::string& line, std::size_t at, std::string value)
{
    std::size_t back = at;
    while (back > 0 && (line[back - 1] == ' ' || line[back - 1] == '"' || line[back - 1] == '\''))
        --back;
    const bool opens = back == 0 || line[back - 1] == '.' || line[back - 1] == '!' || line[back - 1] == '?';
    const auto endsThe = [&]() {
        return at >= 4 && (line.compare(at - 4, 4, "the ") == 0 || line.compare(at - 4, 4, "The ") == 0) &&
               (at == 4 || line[at - 5] == ' ' || line[at - 5] == '"');
    };
    if (value.rfind("the ", 0) == 0 && endsThe())
        value = value.substr(4);
    if (opens && !value.empty() && value[0] >= 'a' && value[0] <= 'z')
        value[0] = char(value[0] - 'a' + 'A');
    line.replace(at, line.size() - at, value);
}
// "1d", "6h", "30m" in game days; "forever" is -1.
bool duration(const std::string& text, double& days)
{
    if (text == "forever")
        return (days = -1), true;
    if (text.size() < 2)
        return false;
    char* end = nullptr;
    const double n = std::strtod(text.c_str(), &end);
    if (end != text.c_str() + text.size() - 1 || n <= 0)
        return false;
    const char unit = text.back();
    days = unit == 'd' ? n : unit == 'h' ? n / 24 : unit == 'm' ? n / 1440 : -2;
    return days > 0;
}

// A line's punctuation tidied after its words are filled in: a full stop straight after a ? or ! (a tag that asks, ", no?",
// before the line's own stop) is dropped.
void tidy(std::string& line)
{
    // Spaces a symbol that said nothing left: none at the ends, never two together, none before a stop.
    std::string spaced;
    for (const char c : line)
        if (c != ' ' || (!spaced.empty() && spaced.back() != ' '))
            spaced += c;
    while (!spaced.empty() && spaced.back() == ' ')
        spaced.pop_back();
    for (std::size_t i = 1; i < spaced.size(); ++i)
        if (spaced[i - 1] == ' ' && (spaced[i] == ',' || spaced[i] == '?' || spaced[i] == '!' ||
                                     (spaced[i] == '.' && (i + 1 >= spaced.size() || spaced[i + 1] != '.'))))
            spaced.erase(--i, 1);                   // (But " ...Well" keeps its space.)
    line = spaced;
    for (std::size_t i = 1; i < line.size(); ++i)
        if (line[i] == '.' && (line[i - 1] == '?' || line[i - 1] == '!') && (i + 1 >= line.size() || line[i + 1] != '.'))
            line.erase(i--, 1);
}
} // namespace

struct Library::Expansion
{
    const Query& q;
    int speaker = 0;
    std::uint64_t rng = 0;
    std::map<std::string, std::string>& binds;   // Symbols bound for the rest of the conversation ([x:#y#]).
    Trace* trace = nullptr;
    std::uint64_t next() { return rng = mix(rng); }
};

void Library::clear()
{
    entries_.clear();
    byConcept_.clear();
    byId_.clear();
    groups_.clear();
    grammars_.clear();
    person_.clear();
    shared_.clear();
    aliases_.clear();
    settings_ = {};
}

const Entry* Library::find(const std::string& id) const
{
    const auto f = byId_.find(id);
    return f == byId_.end() ? nullptr : &entries_[f->second];
}

std::vector<std::string> Library::grammars() const
{
    std::vector<std::string> out;
    for (const auto& [name, g] : grammars_)
        out.push_back(name);
    return out;
}

std::set<std::string> Library::symbols(const std::string& grammar) const
{
    std::set<std::string> out;
    if (const auto g = grammars_.find(grammar); g != grammars_.end())
        for (const auto& [name, s] : g->second)
            out.insert(name);
    return out;
}

bool Library::readSchema(const std::string& text, std::string& problem)
{
    json::Value doc;
    std::string error;
    if (!json::parse(text, doc, error) || !doc.isObject())
    {
        problem = "facts.json: " + (error.empty() ? "not a JSON object" : error);
        return false;
    }
    std::vector<std::string> errors;
    const auto kindOf = [&](const std::string& key, const json::Value& v, FactKind& k) {
        if (v.isString() && (v.asString("") == "text" || v.asString("") == "number"))
            k.type = v.asString("") == "text" ? FactKind::Type::Text : FactKind::Type::Number;
        else if (v.isArray() && v.size() > 0)
            for (const auto& item : v.items())
                k.values.insert(item.asString(""));
        else
            errors.push_back("facts.json: " + key + " is a list of values, \"text\" or \"number\"");
    };
    for (const auto& [name, v] : doc.object("person").fields())
        kindOf(name, v, person_[name]);
    std::set<std::string> twice;
    for (const char* ns : {"pair", "here", "topic", "world"})
        for (const auto& [name, v] : doc.object(ns).fields())
        {
            const auto full = std::string(ns) + "." + name;
            kindOf(full, v, shared_[full]);
            if (person_.count(name))
                errors.push_back("facts.json: " + full + " has a person's name; call it something else");
            if (aliases_.count(name))
                twice.insert(name);
            aliases_[name] = full;
        }
    for (const auto& name : twice)
        aliases_.erase(name);                     // Written in full where two namespaces have it.
    if (!shared_.count("here.region"))
        errors.push_back("facts.json: here.region is needed (the regions)");
    if (!errors.empty())
    {
        problem = errors.front();
        return false;
    }
    return true;
}

bool Library::readSettings(const std::string& text, std::string& problem)
{
    json::Value doc;
    std::string error;
    if (!json::parse(text, doc, error) || !doc.isObject())
    {
        problem = "settings.json: " + (error.empty() ? "not a JSON object" : error);
        return false;
    }
    const auto& tiers = doc.object("tiers");
    settings_.best = tiers.number("best", settings_.best);
    settings_.next = tiers.number("next", settings_.next);
    settings_.lower = tiers.number("lower", settings_.lower);
    settings_.strict = int(doc.number("strict_priority", settings_.strict));
    settings_.namedPerson = int(doc.number("named_person", settings_.namedPerson));
    for (const auto& [key, set] : {std::pair<const char*, std::set<std::string>*>{"concepts", &settings_.concepts},
                                   {"repeat", &settings_.repeat}, {"alone", &settings_.alone}, {"best_only", &settings_.bestOnly},
                                   {"ends", &settings_.ends}})
        for (const auto& v : doc.array(key))
            set->insert(v.asString(""));
    const double sum = settings_.best + settings_.next + settings_.lower;
    if (settings_.best <= 0 || settings_.next < 0 || settings_.lower < 0 || sum < .99 || sum > 1.01)
    {
        problem = "settings.json: the tiers' chances (best, next, lower) are each 0 to 1, and add up to 1";
        return false;
    }
    if (settings_.concepts.empty())
    {
        problem = "settings.json: no concepts";
        return false;
    }
    return true;
}

bool Library::bareWord(const std::string& word, int speaker, Where where, Criterion& c) const
{
    const Ns who = where == Where::Line ? (speaker == 0 ? Ns::A : Ns::B) : Ns::Self;
    const auto person = [&](const char* key, std::set<std::string> values) {
        c = {};
        c.ns = who;
        c.name = key;
        c.values = std::move(values);
        return true;
    };
    if (word == "child")
        return person("stage", {"young", "adolescent"});
    for (const char* key : {"sex", "stage", "role", "job"})
        if (const auto k = person_.find(key); k != person_.end() && k->second.values.count(word))
            return person(key, {word});
    for (const char* full : {"pair.band", "here.time", "here.weather"})
        if (const auto k = shared_.find(full); k != shared_.end() && k->second.values.count(word))
        {
            c = {};
            const std::string f(full);
            c.ns = sharedNs(f.substr(0, f.find('.')));
            c.name = f.substr(f.find('.') + 1);
            c.full = f;
            c.values = {word};
            return true;
        }
    return false;
}

bool Library::criterion(const std::string& token, Where where, int speaker, Criterion& c, std::string& error) const
{
    c = {};
    const auto at = token.find_first_of("=<>!");
    if (at == std::string::npos || at == 0)
    {
        error = "'" + token + "' is not key=value";
        return false;
    }
    const std::string key = token.substr(0, at);
    std::size_t from = at + 1;
    if (token[at] == '!')
    {
        if (at + 1 >= token.size() || token[at + 1] != '=')
        {
            error = "'" + token + "': ! goes before = (key!=value)";
            return false;
        }
        c.op = Criterion::Op::NotIn, from = at + 2;
    }
    else if (token[at] == '<' || token[at] == '>')
    {
        const bool eq = at + 1 < token.size() && token[at + 1] == '=';
        c.op = token[at] == '<' ? (eq ? Criterion::Op::Le : Criterion::Op::Lt) : (eq ? Criterion::Op::Ge : Criterion::Op::Gt);
        from = at + (eq ? 2 : 1);
    }
    const std::string value = token.substr(std::min(from, token.size()));
    const FactKind* kind = nullptr;
    if (const auto dot = key.find('.'); dot != std::string::npos)
    {
        const auto ns = key.substr(0, dot);
        c.name = key.substr(dot + 1);
        // Something remembered (phase 4): self.mem.x, pair.mem.x, world.mem.x (a town's), any key, any words.
        static const FactKind Remembered{FactKind::Type::Text, {}};
        const bool memory = c.name.rfind("mem.", 0) == 0 && c.name.size() > 4 && (isPersonNs(ns) || ns == "pair" || ns == "world");
        if (isPersonNs(ns))
        {
            c.ns = personNs(ns);
            if (const auto k = person_.find(c.name); k != person_.end())
                kind = &k->second;
        }
        else if (isSharedNs(ns))
        {
            c.ns = sharedNs(ns);
            c.full = key;
            if (const auto k = shared_.find(key); k != shared_.end())
                kind = &k->second;
        }
        if (memory)
            kind = &Remembered;
    }
    else if (const auto k = person_.find(key); k != person_.end())
    {
        c.ns = where == Where::Line ? (speaker == 0 ? Ns::A : Ns::B) : Ns::Self;
        c.name = key;
        kind = &k->second;
    }
    else if (const auto alias = aliases_.find(key); alias != aliases_.end())
    {
        c.full = alias->second;
        c.ns = sharedNs(c.full.substr(0, c.full.find('.')));
        c.name = key;
        kind = &shared_.at(c.full);
    }
    if (!kind)
    {
        error = "unknown fact '" + key + "'";
        return false;
    }
    if (c.op == Criterion::Op::In || c.op == Criterion::Op::NotIn)
    {
        for (const auto& v : split(value, '|'))
            c.values.insert(v);
        if (c.values.empty())
        {
            error = "'" + token + "' tests no value";
            return false;
        }
        if (kind->type == FactKind::Type::Words && c.full != "here.region")   // (Regions may be groups: see parse.)
            for (const auto& v : c.values)
                if (!kind->values.count(v))
                {
                    error = "'" + v + "' is not a " + key + " (" + token + ")";
                    return false;
                }
        if (kind->type == FactKind::Type::Number)
            for (const auto& v : c.values)
            {
                char* end = nullptr;
                std::strtod(v.c_str(), &end);
                if (v.empty() || *end)
                {
                    error = "'" + v + "' is not a number (" + token + ")";
                    return false;
                }
            }
        return true;
    }
    if (kind->type != FactKind::Type::Number)
    {
        error = "'" + key + "' is not a number, so it can't be compared (" + token + ")";
        return false;
    }
    char* end = nullptr;
    c.number = std::strtod(value.c_str(), &end);
    if (value.empty() || *end)
    {
        error = "'" + value + "' is not a number (" + token + ")";
        return false;
    }
    return true;
}

const std::string* Library::fact(Ns ns, const std::string& name, const std::string& full, const Query& q, int speaker) const
{
    const Speaker* p = nullptr;
    switch (ns)
    {
    case Ns::A: p = &q.a; break;
    case Ns::B: p = q.alone ? nullptr : &q.b; break;
    case Ns::Self: p = speaker == 0 ? &q.a : q.alone ? nullptr : &q.b; break;
    case Ns::Other: p = speaker == 0 ? (q.alone ? nullptr : &q.b) : &q.a; break;
    default:
    {
        const auto f = q.facts.find(full);
        return f == q.facts.end() ? nullptr : &f->second;
    }
    }
    if (!p)
        return nullptr;
    const auto f = p->facts.find(name);
    return f == p->facts.end() ? nullptr : &f->second;
}

bool Library::holds(const Criterion& c, const Query& q, int speaker) const
{
    const std::string* v = fact(c.ns, c.name, c.full, q, speaker);
    switch (c.op)
    {
    case Criterion::Op::In: return v && c.values.count(*v) > 0;
    case Criterion::Op::NotIn: return !v || c.values.count(*v) == 0;
    case Criterion::Op::Has: return v && !v->empty();
    case Criterion::Op::Lacks: return !v || v->empty();
    default: break;
    }
    if (!v || v->empty())
        return false;
    char* end = nullptr;
    const double n = std::strtod(v->c_str(), &end);
    if (*end)
        return false;
    switch (c.op)
    {
    case Criterion::Op::Lt: return n < c.number;
    case Criterion::Op::Le: return n <= c.number;
    case Criterion::Op::Gt: return n > c.number;
    default: return n >= c.number;
    }
}

std::size_t Library::longestSymbol(const std::string& name, std::map<std::string, std::size_t>& memo,
                                   std::set<std::string>& open, int depth, std::string& error) const
{
    if (const auto m = memo.find(name); m != memo.end())
        return m->second;
    if (depth > DeepestSymbol)
    {
        error = "#" + name + "# goes more than " + std::to_string(DeepestSymbol) + " symbols deep";
        return 0;
    }
    if (open.count(name))
    {
        error = "#" + name + "# uses itself";
        return 0;
    }
    open.insert(name);
    std::size_t longest = 0;
    for (const auto& [layer, g] : grammars_)
        for (const auto& key : {name, name + ".broad"})
        if (const auto s = g.find(key); s != g.end())
            for (const auto& e : s->second.entries)
                for (const auto& alt : e.alts)
                {
                    // Measured as checkText does, one level deeper.
                    std::size_t length = 0;
                    const auto& t = alt.text;
                    for (std::size_t i = 0; i < t.size() && error.empty(); ++i)
                    {
                        if (t[i] == '#')
                        {
                            const auto end = t.find('#', i + 1);
                            if (end == std::string::npos)
                                break;
                            const auto ref = readRef(t.substr(i + 1, end - i - 1));
                            if (ref.parts.size() == 1)
                                length += longestSymbol(ref.parts[0], memo, open, depth + 1, error);
                            i = end;
                        }
                        else if (t[i] == '[')
                        {
                            const auto end = t.find(']', i);
                            if (end == std::string::npos)
                                break;
                            i = end;
                        }
                        else
                            ++length;
                    }
                    longest = std::max(longest, length);
                }
    open.erase(name);
    if (error.empty())
        memo[name] = longest;
    return longest;
}

bool Library::checkText(const std::string& text, std::vector<std::string>& errors, const std::string& where,
                        std::size_t* longest) const
{
    const auto before = errors.size();
    std::size_t length = 0;
    std::map<std::string, std::size_t> memo;
    const auto& plain = grammars_.count("plain") ? grammars_.at("plain") : Grammar{};
    const auto checkRef = [&](const std::string& inside) {
        const auto ref = readRef(inside);
        if (ref.parts.empty() || std::any_of(ref.parts.begin(), ref.parts.end(), [](const std::string& p) { return p.empty(); }))
        {
            errors.push_back(where + ": #" + inside + "# is not a symbol or a fact");
            return;
        }
        if (ref.parts.size() == 2 && isPersonNs(ref.parts[0]))
        {
            if (!person_.count(ref.parts[1]) && !Pronouns.count(ref.parts[1]))
                errors.push_back(where + ": #" + inside + "#: a person has no fact '" + ref.parts[1] + "'");
            return;
        }
        if (ref.parts.size() == 2 && isSharedNs(ref.parts[0]))
        {
            if (!shared_.count(ref.parts[0] + "." + ref.parts[1]))
                errors.push_back(where + ": #" + inside + "#: there is no fact " + ref.parts[0] + "." + ref.parts[1]);
            return;
        }
        if (ref.parts.size() != 1)
        {
            errors.push_back(where + ": #" + inside + "# is not a symbol or a fact");
            return;
        }
        if (!plain.count(ref.parts[0]))
        {
            errors.push_back(where + ": #" + ref.parts[0] + "# is not in the plain grammar");
            return;
        }
        std::set<std::string> open;
        std::string error;
        length += longestSymbol(ref.parts[0], memo, open, 1, error);
        if (!error.empty())
            errors.push_back(where + ": " + error);
    };
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == '#')
        {
            const auto end = text.find('#', i + 1);
            if (end == std::string::npos)
            {
                errors.push_back(where + ": a # without its closing #");
                break;
            }
            checkRef(text.substr(i + 1, end - i - 1));
            i = end;
        }
        else if (text[i] == '[')
        {
            // [name:#symbol#]: the symbol chosen once, and kept for the rest of the conversation as #name#.
            const auto end = text.find(']', i);
            const auto colon = text.find(':', i);
            if (end == std::string::npos || colon == std::string::npos || colon > end)
            {
                errors.push_back(where + ": [ is for binding a symbol: [name:#symbol#]");
                break;
            }
            const auto name = text.substr(i + 1, colon - i - 1);
            if (!plain.count(name))
                errors.push_back(where + ": [" + name + ":...] binds a symbol the plain grammar doesn't have");
            std::size_t inner = 0;
            checkText(text.substr(colon + 1, end - colon - 1), errors, where, &inner);
            i = end;
        }
        else if (text[i] == ']')
        {
            errors.push_back(where + ": a ] without its [");
            break;
        }
        else
            ++length;
    }
    if (longest)
        *longest = length;
    return errors.size() == before;
}

bool Library::parseGrammar(const std::string& text, const std::string& file, std::string& problem)
{
    std::vector<std::string> errors;
    auto merged = grammars_;
    std::string current;
    std::istringstream in(text);
    std::string raw;
    int number = 0;
    const auto fail = [&](const std::string& message) { errors.push_back(file + ":" + std::to_string(number) + ": " + message); };
    struct Pending
    {
        std::string grammar, symbol, where;
        std::vector<std::string> texts;
    };
    std::vector<Pending> pending;
    while (std::getline(in, raw))
    {
        ++number;
        const std::string line = trim(raw);
        if (line.empty() || line[0] == '#')
            continue;
        if (line.rfind("grammar ", 0) == 0)
        {
            current = trim(line.substr(8));
            if (current.empty() || current.find_first_not_of("abcdefghijklmnopqrstuvwxyz_0123456789") != std::string::npos)
                fail("a grammar's name is lower-case letters and _");
            merged[current];
            continue;
        }
        if (current.empty())
        {
            fail("expected 'grammar <name>' first");
            continue;
        }
        // name[!][criteria] (= | +=) alternative | alternative
        std::size_t at = 0;
        while (at < line.size() && (std::islower(static_cast<unsigned char>(line[at])) || std::isdigit(static_cast<unsigned char>(line[at])) || line[at] == '_'))
            ++at;
        const std::string name = line.substr(0, at);
        if (name.empty())
        {
            fail("expected 'symbol = words | words'");
            continue;
        }
        GrammarEntry entry;
        // name.broad: the dialect's word for the broadest accents only (doc 62, section 3).
        bool broad = false, habit = false, pairHabit = false;
        if (line.compare(at, 6, ".broad") == 0)
            broad = true, at += 6;
        if (at < line.size() && line[at] == '!')
            habit = true, ++at;
        if (habit && at < line.size() && line[at] == '!')
            pairHabit = true, ++at;
        if (at < line.size() && line[at] == '[')
        {
            const auto end = line.find(']', at);
            if (end == std::string::npos)
            {
                fail("a [ without its ]");
                continue;
            }
            for (const auto& token : words(line.substr(at + 1, end - at - 1)))
            {
                Criterion c;
                std::string error;
                if (token.find_first_of("=<>!") == std::string::npos ? !bareWord(token, 0, Where::GrammarWhen, c)
                                                                      : !criterion(token, Where::GrammarWhen, 0, c, error))
                {
                    fail(error.empty() ? "'" + token + "' is not a fact" : error);
                    continue;
                }
                if (c.full == "here.region")
                {
                    std::set<std::string> expanded;
                    for (const auto& v : c.values)
                        if (groups_.count(v))
                            expanded.insert(groups_.at(v).begin(), groups_.at(v).end());
                        else if (shared_.at("here.region").values.count(v))
                            expanded.insert(v);
                        else
                            fail("'" + v + "' is not a region or group");
                    c.values = expanded;
                }
                entry.when.push_back(c);
            }
            at = end + 1;
        }
        const std::string rest = trim(line.substr(at));
        std::string said;
        if (rest.rfind("+=", 0) == 0)
            entry.add = true, said = rest.substr(2);
        else if (rest.rfind("=", 0) == 0)
            said = rest.substr(1);
        else
        {
            fail("expected '=' or '+=' after " + name);
            continue;
        }
        for (auto t : split(said, '|'))
        {
            Alt alt;
            if (t.size() > 2 && t[t.size() - 2] == '^' && (t.back() == 'm' || t.back() == 'f'))
                alt.sex = t.back(), t = trim(t.substr(0, t.size() - 2));
            alt.text = t == "~" ? std::string() : t;   // (~: nothing at all, as an opener may be.)
            entry.alts.push_back(alt);
        }
        if (entry.alts.empty())
        {
            fail(name + " has no words");
            continue;
        }
        if (entry.add && current == "plain")
            fail("plain is the bottom layer: nothing below it to add to (" + name + " +=)");
        if (broad && current == "plain")
            fail("plain has no accent: " + name + ".broad belongs in a dialect");
        entry.score = int(entry.when.size());
        auto& symbol = merged[current][broad ? name + ".broad" : name];
        symbol.habit = symbol.habit || habit;
        symbol.pairHabit = symbol.pairHabit || pairHabit;
        Pending p{current, name, file + ":" + std::to_string(number), {}};
        for (const auto& a : entry.alts)
            p.texts.push_back(a.text);
        pending.push_back(p);
        symbol.entries.push_back(std::move(entry));
    }
    // Checked against the grammars with this file in: every symbol used, and every layer's symbol, is in plain.
    auto kept = std::move(grammars_);
    grammars_ = merged;
    for (const auto& p : pending)
    {
        if (p.grammar != "plain" && (!grammars_.count("plain") || !grammars_.at("plain").count(p.symbol)))
            errors.push_back(p.where + ": " + p.symbol + " is not in the plain grammar (every symbol must be)");
        for (const auto& t : p.texts)
            checkText(t, errors, p.where, nullptr);
    }
    if (errors.empty())
        checkGrammars(errors);
    if (!errors.empty())
    {
        grammars_ = std::move(kept);
        problem.clear();
        for (std::size_t i = 0; i < errors.size() && i < 20; ++i)
            problem += (i ? "\n" : "") + errors[i];
        if (errors.size() > 20)
            problem += "\n(" + std::to_string(errors.size() - 20) + " more)";
        return false;
    }
    return true;
}

bool Library::checkGrammars(std::vector<std::string>& errors) const
{
    const auto before = errors.size();
    std::map<std::string, std::size_t> memo;
    for (const auto& [layer, g] : grammars_)
        for (const auto& [name, s] : g)
        {
            std::set<std::string> open;
            std::string error;
            if (longestSymbol(name, memo, open, 1, error) > LongestLine)
                errors.push_back("grammar " + layer + ": #" + name + "# can say more than " + std::to_string(LongestLine) + " characters");
            if (!error.empty())
                errors.push_back("grammar " + layer + ": " + error);
        }
    return errors.size() == before;
}

bool Library::parse(const std::string& text, const std::string& file, std::string& problem)
{
    std::vector<std::string> errors;
    std::vector<Entry> parsed;
    auto groups = groups_;
    const auto& regions = shared_.count("here.region") ? shared_.at("here.region").values : std::set<std::string>{};
    std::istringstream in(text);
    std::string raw;
    int number = 0;
    Entry* e = nullptr;
    const auto fail = [&](const std::string& message) { errors.push_back(file + ":" + std::to_string(number) + ": " + message); };
    const auto regionsIn = [&](Criterion& c) {
        if (c.full != "here.region")
            return;
        std::set<std::string> expanded;
        for (const auto& v : c.values)
        {
            if (groups.count(v))
                expanded.insert(groups[v].begin(), groups[v].end());
            else if (regions.count(v))
                expanded.insert(v);
            else
                fail("'" + v + "' is not a region or group");
        }
        c.values = expanded;
    };
    const auto test = [&](const std::string& token, Where where, int speaker, std::vector<Criterion>& into) {
        Criterion c;
        std::string error;
        // Something remembered: self.mem.x (it is), !self.mem.x (it isn't). So too any words a speaker may or may not
        // have (phase 7): self.spouse (they have one, to name), !self.child.
        const auto bare = token[0] == '!' ? token.substr(1) : token;
        Criterion probe;
        std::string unused;
        const bool words = bare.find_first_of("=<>!") == std::string::npos && bare.find('.') != std::string::npos &&
                           criterion(bare + "=x", where, speaker, probe, unused);
        if (bare.find_first_of("=<>!") == std::string::npos && (bare.find("mem.") != std::string::npos || words))
        {
            if (!criterion(bare + "=yes", where, speaker, c, error))
            {
                fail(error);
                return;
            }
            c.op = token[0] == '!' ? Criterion::Op::Lacks : Criterion::Op::Has;
            c.values.clear();
        }
        else if (token.find_first_of("=<>!") == std::string::npos)
        {
            if (!bareWord(token, speaker, where, c))
            {
                fail("'" + token + "' is not a sex, stage, role, job, band, time or weather");
                return;
            }
        }
        else if (!criterion(token, where, speaker, c, error))
        {
            fail(error);
            return;
        }
        regionsIn(c);
        into.push_back(c);
    };
    const auto concept = [&](const std::string& name) {
        if (!settings_.concepts.count(name))
            fail("'" + name + "' is not a concept (settings.json)");
        e->concepts.insert(name);
    };
    const auto addText = [&](Variant& v, const std::string& t) {
        std::size_t longest = 0;
        if (checkText(t, errors, file + ":" + std::to_string(number), &longest) && longest > LongestLine)
            fail("a line of more than " + std::to_string(LongestLine) + " characters");
        v.texts.push_back(t);
    };
    while (std::getline(in, raw))
    {
        ++number;
        const std::string line = trim(raw);
        if (line.empty() || line[0] == '#')
            continue;
        if (line.rfind("group ", 0) == 0)
        {
            const auto eq = line.find('=');
            const auto name = trim(line.substr(6, eq == std::string::npos ? std::string::npos : eq - 6));
            if (eq == std::string::npos || name.empty())
            {
                fail("a group is 'group name = region region ...'");
                continue;
            }
            for (const auto& r : words(line.substr(eq + 1)))
            {
                if (!regions.count(r) && !groups.count(r))
                    fail("'" + r + "' is not a region");
                if (groups.count(r))
                    groups[name].insert(groups[r].begin(), groups[r].end());
                else
                    groups[name].insert(r);
            }
            continue;
        }
        if (line.rfind("scene ", 0) == 0 || line.rfind("rule ", 0) == 0)
        {
            const bool scene = line[0] == 's';
            parsed.push_back({});
            e = &parsed.back();
            e->scene = scene;
            e->id = trim(line.substr(scene ? 6 : 5));
            e->file = file;
            if (e->id.empty() || e->id.find_first_of(" \t") != std::string::npos)
                fail(std::string("a ") + (scene ? "scene" : "rule") + " needs one ID");
            if (byId_.count(e->id) ||
                std::count_if(parsed.begin(), parsed.end(), [&](const Entry& x) { return x.id == e->id; }) > 1)
                fail(e->id + " is defined twice");
            continue;
        }
        if (!e)
        {
            fail("expected 'rule <id>' or 'scene <id>' first");
            continue;
        }
        if (line.rfind("when ", 0) == 0)
        {
            for (const auto& token : words(line.substr(5)))
            {
                if (token.rfind("concept=", 0) == 0)
                    for (const auto& v : split(token.substr(8), '|'))
                        concept(v);
                else if (token.rfind("topic=", 0) == 0)
                    for (const auto& v : split(token.substr(6), '|'))
                        concept("talk." + v);
                else
                    test(token, Where::When, 0, e->when);
            }
            continue;
        }
        if (line.rfind("weight ", 0) == 0 || line.rfind("priority ", 0) == 0)
        {
            const bool weight = line[0] == 'w';
            const auto value = trim(line.substr(weight ? 7 : 9));
            char* end = nullptr;
            const double n = std::strtod(value.c_str(), &end);
            if (value.empty() || *end)
                fail(std::string(weight ? "weight" : "priority") + " is a number");
            else if (weight && !(n > 0 && n <= 20))
                fail("weight is a number from 0 to 20");
            else if (!weight && (n != double(int(n)) || n < -10 || n > 100))
                fail("priority is a whole number from -10 to 100");
            else if (weight)
                e->weight = n;
            else
                e->priority = int(n);
            continue;
        }
        // Phase 4: then, remember, once, cooldown.
        if (line.rfind("then ", 0) == 0)
        {
            const auto colon = line.find(':');
            const auto who = colon == std::string::npos ? std::string() : trim(line.substr(5, colon - 5));
            const auto next = colon == std::string::npos ? std::string() : trim(line.substr(colon + 1));
            if ((who != "other" && who != "self") || next.empty() || next.find(' ') != std::string::npos)
                fail("then is 'then other: <concept>' or 'then self: <concept>'");
            else if (e->scene)
                fail("a scene is whole: 'then' is a rule's");
            else if (!settings_.concepts.count(next))
                fail("'" + next + "' is not a concept (settings.json)");
            else
                e->thenWho = who, e->thenConcept = next;
            continue;
        }
        if (line.rfind("remember ", 0) == 0)
        {
            // remember self|other|pair|town: key[=value] [DURATION]
            const auto colon = line.find(':');
            const auto scope = colon == std::string::npos ? std::string() : trim(line.substr(9, colon - 9));
            const auto parts = colon == std::string::npos ? std::vector<std::string>{} : words(line.substr(colon + 1));
            Entry::Remember r;
            r.scope = scope;
            double days = -1;
            if ((scope != "self" && scope != "other" && scope != "pair" && scope != "town") || parts.empty() || parts.size() > 2 ||
                (parts.size() == 2 && !duration(parts[1], days)))
            {
                fail("remember is 'remember self|other|pair|town: key[=value] [1d|6h|30m|forever]'");
                continue;
            }
            const auto eq = parts[0].find('=');
            r.key = parts[0].substr(0, eq);
            r.value = eq == std::string::npos ? "yes" : parts[0].substr(eq + 1);
            r.days = days;
            if (r.key.empty() || r.key.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos || r.value.empty())
                fail("a remembered key is lower-case letters, digits and _");
            else
                e->remember.push_back(r);
            continue;
        }
        if (line.rfind("regard ", 0) == 0)
        {
            // regard self|other: affinity +1 trust -1 (affinity, trust, respect, fear, familiarity; bounded by the game)
            const auto colon = line.find(':');
            Entry::Regard r;
            r.who = colon == std::string::npos ? std::string() : trim(line.substr(7, colon - 7));
            const auto parts = colon == std::string::npos ? std::vector<std::string>{} : words(line.substr(colon + 1));
            static const std::set<std::string> Axes{"affinity", "trust", "respect", "fear", "familiarity"};
            bool good = (r.who == "self" || r.who == "other") && !parts.empty() && parts.size() % 2 == 0;
            for (std::size_t i = 0; good && i + 1 < parts.size(); i += 2)
            {
                char* end = nullptr;
                const double n = std::strtod(parts[i + 1].c_str(), &end);
                good = Axes.count(parts[i]) && !*end && n != 0 && std::abs(n) <= 5;
                r.change.push_back({parts[i], n});
            }
            if (!good)
                fail("regard is 'regard self|other: affinity +1 trust -1' (affinity, trust, respect, fear, familiarity; at most 5)");
            else
                e->regard.push_back(r);
            continue;
        }
        if (line == "once" || line == "once per day" || line == "once per other" || line.rfind("cooldown ", 0) == 0)
        {
            if (e->limit != Entry::Limit::None)
                fail("one of once, once per day, once per other or cooldown");
            else if (line == "once")
                e->limit = Entry::Limit::Once;
            else if (line == "once per day")
                e->limit = Entry::Limit::PerDay;
            else if (line == "once per other")
                e->limit = Entry::Limit::PerOther;
            else if (duration(trim(line.substr(9)), e->cooldownDays) && e->cooldownDays > 0)
                e->limit = Entry::Limit::Cooldown;
            else
                fail("cooldown is 'cooldown 2h' (or 30m, 1d)");
            continue;
        }
        if (line.rfind("say ", 0) == 0 || line == "say")
        {
            if (e->scene)
            {
                fail("a scene's lines are 'a : words' and 'b : words'; 'say' is a rule's");
                continue;
            }
            if (e->turns.empty())
                e->turns.push_back({0, false, {Variant{{}, {}, {}, {}, number}}});
            for (const auto& t : split(line.substr(3), '|'))
                addText(e->turns[0].variants[0], t);
            continue;
        }
        // A scene's line: who[?][tags] : alternative | alternative
        const auto colon = line.find(':');
        if (colon == std::string::npos)
        {
            fail(e->scene ? "expected 'a : words' or 'b : words'" : "expected when, say, weight or priority");
            continue;
        }
        if (!e->scene)
        {
            fail("a rule says its words with 'say'");
            continue;
        }
        const std::string head = trim(line.substr(0, colon));
        if (head.empty() || (head[0] != 'a' && head[0] != 'b'))
        {
            fail("a line starts with a or b");
            continue;
        }
        const int who = head[0] == 'a' ? 0 : 1;
        bool optional = false;
        std::size_t at = 1;
        if (at < head.size() && head[at] == '?')
            optional = true, ++at;
        Variant v;
        v.line = number;
        // a@worried: only after a line that marked the scene so; b[...] #worried: marks it when spoken.
        const auto markName = [&](std::size_t& i) {
            std::size_t j = i;
            while (j < head.size() && (std::islower(static_cast<unsigned char>(head[j])) || std::isdigit(static_cast<unsigned char>(head[j])) || head[j] == '_'))
                ++j;
            auto name = head.substr(i, j - i);
            i = j;
            return name;
        };
        bool badMark = false;
        while (at < head.size() && head[at] == '@')
        {
            ++at;
            const auto name = markName(at);
            badMark |= name.empty();
            v.needs.insert(name);
        }
        std::string tags, rest = head.substr(at);
        if (!rest.empty() && rest[0] == '[')
        {
            const auto end = rest.find(']');
            if (end == std::string::npos)
            {
                fail("tags go in [ ]");
                continue;
            }
            tags = rest.substr(1, end - 1);
            rest = rest.substr(end + 1);
        }
        for (const auto& w : words(rest))
        {
            if (w.size() < 2 || w[0] != '#')
            {
                badMark = true;
                continue;
            }
            const auto name = w.substr(1);
            badMark |= name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos;
            v.marks.insert(name);
        }
        if (badMark)
        {
            fail("expected who[?][@mark][tags] [#mark] before the colon: marks are lower-case words");
            continue;
        }
        if (!tags.empty())
        {
            for (const auto& token : split(tags, ','))
                for (const auto& w : words(token))
                {
                    if (w.rfind("topic=", 0) == 0 || w.rfind("concept=", 0) == 0)
                    {
                        fail("a line can't test the topic");
                        continue;
                    }
                    test(w, Where::Line, who, v.when);
                }
        }
        for (const auto& t : split(line.substr(colon + 1), '|'))
            addText(v, t);
        if (v.texts.empty())
        {
            fail("a line with no words");
            continue;
        }
        // The same speaker's previous line, tagged, chooses with this one (a turn); otherwise a new turn starts.
        auto& turns = e->turns;
        const auto& last = turns.empty() ? Variant{} : turns.back().variants.back();
        const bool joins = !turns.empty() && turns.back().who == who && (!last.when.empty() || !last.needs.empty()) &&
                           turns.back().optional == optional;
        if (!joins)
            turns.push_back({who, optional, {}});
        turns.back().variants.push_back(v);
    }
    for (auto& x : parsed)
    {
        const std::string what = x.file + ": " + (x.scene ? "scene " : "rule ") + x.id;
        if (x.concepts.empty())
            errors.push_back(what + " has no concept (when concept=... or topic=...)");
        if (x.turns.empty())
            errors.push_back(what + (x.scene ? " has no lines" : " says nothing (say ...)"));
        const bool alone = !x.concepts.empty() && std::all_of(x.concepts.begin(), x.concepts.end(), [&](const std::string& c) {
            return settings_.alone.count(c) > 0;
        });
        if (x.scene)
        {
            bool plainA = false, plainB = false;
            for (const auto& t : x.turns)
                if (!t.optional && t.variants.back().when.empty())
                    (t.who == 0 ? plainA : plainB) = true;
            if (!plainA)
                errors.push_back(what + " needs a line a always says (one without tags)");
            if (!alone && !plainB)
                errors.push_back(what + " needs a line b always says (one without tags)");
            if (alone && std::any_of(x.turns.begin(), x.turns.end(), [](const Turn& t) { return t.who == 1; }))
                errors.push_back(what + " is for one speaker (a) only");
        }
        x.score = x.priority;
        for (const auto& c : x.when)
            x.score += (c.name == "id" && c.full.empty()) ? settings_.namedPerson : 1;
        x.repeat = std::any_of(x.concepts.begin(), x.concepts.end(), [&](const std::string& c) { return settings_.repeat.count(c) > 0; });
    }
    if (!errors.empty())
    {
        problem.clear();
        for (std::size_t i = 0; i < errors.size() && i < 20; ++i)
            problem += (i ? "\n" : "") + errors[i];
        if (errors.size() > 20)
            problem += "\n(" + std::to_string(errors.size() - 20) + " more)";
        return false;
    }
    groups_ = groups;
    for (auto& x : parsed)
    {
        byId_[x.id] = entries_.size();
        for (const auto& c : x.concepts)
            byConcept_[c].push_back(entries_.size());
        entries_.push_back(std::move(x));
    }
    return true;
}

namespace
{
bool loadDir(Library& lib, const std::string& dir, bool all, std::string& problem,
             const std::function<bool(const std::string&, std::string&)>& schema,
             const std::function<bool(const std::string&, std::string&)>& settings)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::is_directory(dir, ec))
    {
        problem = "No talk library at " + dir + ".";
        return false;
    }
    std::string text;
    if (!readFile(fs::path(dir) / "facts.json", text))
    {
        problem = dir + "/facts.json is missing";
        return false;
    }
    if (!schema(text, problem))
        return false;
    if (!readFile(fs::path(dir) / "settings.json", text))
    {
        problem = dir + "/settings.json is missing";
        return false;
    }
    if (!settings(text, problem))
        return false;
    std::vector<fs::path> grammars, talks;
    for (fs::recursive_directory_iterator it(dir, ec), end; it != end && !ec; it.increment(ec))
        if (it->is_regular_file())
            (it->path().extension() == ".grammar" ? grammars : talks).push_back(it->path());
    talks.erase(std::remove_if(talks.begin(), talks.end(), [](const fs::path& p) { return p.extension() != ".talk"; }), talks.end());
    // Plain first (every layer stands on it), groups first (every file may use them), then a fixed order.
    const auto first = [](const char* name) {
        return [name](const fs::path& x, const fs::path& y) {
            const bool fx = x.filename() == name, fy = y.filename() == name;
            return fx != fy ? fx : x.generic_string() < y.generic_string();
        };
    };
    std::sort(grammars.begin(), grammars.end(), first("plain.grammar"));
    std::sort(talks.begin(), talks.end(), first("groups.talk"));
    std::string trouble;
    const auto add = [&](const std::string& one) { trouble += (trouble.empty() ? "" : "\n") + one; };
    if (grammars.empty() || grammars.front().filename() != "plain.grammar")
        add(dir + ": no grammar/plain.grammar");
    for (const auto& f : grammars)
    {
        std::string one;
        if (readFile(f, text) && !lib.parseGrammar(text, fs::relative(f, dir, ec).generic_string(), one))
            add(one);
    }
    if (!trouble.empty())
    {
        problem = trouble;
        return false;
    }
    for (const auto& f : talks)
    {
        if (!all && f.filename() != "groups.talk")
            continue;
        std::string one;
        if (readFile(f, text) && !lib.parse(text, fs::relative(f, dir, ec).generic_string(), one))
            add(one);
    }
    if (!trouble.empty())
    {
        problem = trouble;
        return false;
    }
    return true;
}
} // namespace

bool Library::load(const std::string& dir, std::string& problem)
{
    clear();
    const bool ok = loadDir(*this, dir, true, problem, [this](const std::string& t, std::string& p) { return readSchema(t, p); },
                            [this](const std::string& t, std::string& p) { return readSettings(t, p); });
    if (!ok)
        clear();
    return ok;
}

bool Library::loadFoundation(const std::string& dir, std::string& problem)
{
    clear();
    const bool ok = loadDir(*this, dir, false, problem, [this](const std::string& t, std::string& p) { return readSchema(t, p); },
                            [this](const std::string& t, std::string& p) { return readSettings(t, p); });
    if (!ok)
        clear();
    return ok;
}

bool Library::expandSymbol(const std::string& name, Expansion& x, int depth, std::string& out) const
{
    if (depth > DeepestSymbol)
        return false;
    const Speaker& who = x.speaker == 0 || x.q.alone ? x.q.a : x.q.b;
    const Speaker& to = x.q.alone ? x.q.a : x.speaker == 0 ? x.q.b : x.q.a;
    const auto fact = [](const Speaker& s, const char* key) {
        const auto f = s.facts.find(key);
        return f == s.facts.end() ? std::string() : f->second;
    };
    // The speaker's dialect is the layer named for where they're from; their accent says how much of it they use: none
    // (plain's words), light (the dialect's word half the time), broad (always, and its .broad words too).
    const auto dialect = fact(who, "from"), accent = fact(who, "accent");
    std::vector<const Alt*> alts;
    bool habit = false, pairHabit = false;
    std::string from;
    auto layers = who.grammar;
    layers.push_back("plain");
    for (const auto& layer : layers)
    {
        const auto g = grammars_.find(layer);
        if (g == grammars_.end())
            continue;
        const bool spoken = layer == dialect;
        if (spoken && (accent == "none" || (accent == "light" && x.next() % 2)))
            continue;
        std::vector<const Symbol*> symbols;
        if (const auto s = g->second.find(name); s != g->second.end())
            symbols.push_back(&s->second);
        if (spoken && accent == "broad")
            if (const auto s = g->second.find(name + ".broad"); s != g->second.end())
                symbols.push_back(&s->second);
        if (symbols.empty())
            continue;
        // The most specific entry that holds; entries as specific as each other are one list.
        const auto holdsAll = [&](const GrammarEntry& e) {
            return std::all_of(e.when.begin(), e.when.end(), [&](const Criterion& c) { return holds(c, x.q, x.speaker); });
        };
        int best = -1;
        for (const auto* s : symbols)
        {
            habit = habit || s->habit;
            pairHabit = pairHabit || s->pairHabit;
            for (const auto& e : s->entries)
                if (e.score > best && holdsAll(e))
                    best = e.score;
        }
        if (best < 0)
            continue;
        bool adds = true;
        for (const auto* s : symbols)
            for (const auto& e : s->entries)
                if (e.score == best && holdsAll(e))
                {
                    for (const auto& a : e.alts)
                        alts.push_back(&a);
                    adds = adds && e.add;
                }
        from += (from.empty() ? "" : "+") + layer;
        if (!adds)
            break;
    }
    // ^m and ^f: for one sex of the one spoken to (or of the speaker, speaking to no one).
    const auto sex = fact(to, "sex");
    alts.erase(std::remove_if(alts.begin(), alts.end(), [&](const Alt* a) {
                   return a->sex && !((a->sex == 'm' && sex == "male") || (a->sex == 'f' && sex == "female"));
               }),
               alts.end());
    if (alts.empty())
        return false;
    const auto id = !fact(who, "id").empty() ? fact(who, "id") : fact(who, "name");
    const auto toId = &to == &who ? std::string() : !fact(to, "id").empty() ? fact(to, "id") : fact(to, "name");
    const auto seed = pairHabit ? mix(hash(id + "|" + toId + "|" + name)) : habit ? mix(hash(id + "|" + name)) : x.next();
    const auto* alt = alts[std::size_t(seed % alts.size())];
    if (!expandInto(alt->text, x, depth, out))
        return false;
    if (x.trace)
        x.trace->steps.push_back("#" + name + "# (" + (from.empty() ? "plain" : from) + (pairHabit ? ", a habit for the one spoken to" : habit ? ", a habit" : "") +
                                 ") -> " + out);
    return true;
}

bool Library::expandInto(const std::string& text, Expansion& x, int depth, std::string& out) const
{
    out.clear();
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == '[')
        {
            const auto end = text.find(']', i);
            const auto colon = text.find(':', i);
            if (end == std::string::npos || colon == std::string::npos || colon > end)
                return false;
            std::string bound;
            if (!expandInto(text.substr(colon + 1, end - colon - 1), x, depth, bound))
                return false;
            x.binds[text.substr(i + 1, colon - i - 1)] = bound;
            i = end;
            continue;
        }
        if (text[i] != '#')
        {
            out += text[i];
            continue;
        }
        const auto end = text.find('#', i + 1);
        if (end == std::string::npos)
            return false;
        const auto ref = readRef(text.substr(i + 1, end - i - 1));
        i = end;
        std::string value;
        if (ref.parts.size() == 2 && isPersonNs(ref.parts[0]))
        {
            const Ns ns = personNs(ref.parts[0]);
            if (Pronouns.count(ref.parts[1]))
            {
                const std::string* sex = fact(ns, "sex", {}, x.q, x.speaker);
                const std::string* any = fact(ns, "name", {}, x.q, x.speaker);
                if (!any && !sex)
                    return false;
                value = pronoun(ref.parts[1], sex ? *sex : std::string());
            }
            else if (const auto* v = fact(ns, ref.parts[1], {}, x.q, x.speaker))
                value = *v;
        }
        else if (ref.parts.size() == 2)
        {
            if (const auto* v = fact(sharedNs(ref.parts[0]), ref.parts[1], ref.parts[0] + "." + ref.parts[1], x.q, x.speaker))
                value = *v;
        }
        else if (const auto b = x.binds.find(ref.parts[0]); b != x.binds.end())
            value = b->second;
        else if (!expandSymbol(ref.parts[0], x, depth + 1, value))
            return false;
        else if (value.empty())
            continue;                               // (A symbol may say nothing: ~. A fact may not.)
        if (value.empty())
            return false;
        for (const auto& m : ref.mods)
            value = modify(value, m);
        insert(out, out.size(), value);
    }
    return true;
}

bool Library::expand(const std::string& text, const Query& q, int speaker, std::uint64_t seed, std::string& out,
                     Trace* trace) const
{
    std::map<std::string, std::string> binds;
    Expansion x{q, speaker, mix(seed), binds, trace};
    if (!expandInto(text, x, 0, out))
        return false;
    tidy(out);
    return true;
}

bool Library::fits(const Entry& e, const Query& q, int speaker) const
{
    if (!e.concepts.count(q.concept))
        return false;
    // Said already: once (ever), once a day, at most every so often (the speaker's memory), once to each one (the pair's).
    const Speaker& self = speaker == 0 || q.alone ? q.a : q.b;
    if (e.limit == Entry::Limit::PerOther ? q.facts.count("pair.said:" + e.id) > 0
                                          : e.limit != Entry::Limit::None && self.facts.count("said:" + e.id) > 0)
        return false;
    return std::all_of(e.when.begin(), e.when.end(), [&](const Criterion& c) { return holds(c, q, speaker); });
}

void Library::remembered(const Effect& e, Query& q)
{
    if (e.scope == "regard")
        return;                                     // (The game's: Game::regard.)
    const auto said = e.key.rfind("said:", 0) == 0;
    if (e.scope == "person")
    {
        for (auto* p : {&q.a, &q.b})
            if (const auto id = p->facts.find("id"); id != p->facts.end() && id->second == e.owner)
                p->facts[said ? e.key : "mem." + e.key] = e.value;
    }
    else if (e.scope == "pair")
        q.facts[said ? "pair." + e.key : "pair.mem." + e.key] = e.value;
    else if (e.scope == "town")
        q.facts["world.mem." + e.key] = e.value;
}

Said Library::render(const Entry& e, const Query& q, std::uint64_t seed, Trace* trace, int speaker) const
{
    Said out;
    if (!fits(e, q, speaker))
        return out;
    std::map<std::string, std::string> binds;
    std::set<std::string> marks;
    std::uint64_t r = mix(seed ^ hash(e.id));
    // Alternatives are written in parallel: a line with as many alternatives as the line spoken before it answers the
    // same one (the second reply to the second remark), even past a single-line answer in between. Otherwise one at
    // random.
    std::size_t threadIndex = 0, threadCount = 0;
    for (std::size_t t = 0; t < e.turns.size(); ++t)
    {
        const auto& turn = e.turns[t];
        const int who = e.scene ? turn.who : (turn.who + speaker) % 2;   // (A rule speaks as whoever it's asked of.)
        if (who == 1 && q.alone)
            return {};
        const Variant* chosen = nullptr;
        for (const auto& v : turn.variants)
            if (std::all_of(v.needs.begin(), v.needs.end(), [&](const std::string& m) { return marks.count(m) > 0; }) &&
                std::all_of(v.when.begin(), v.when.end(), [&](const Criterion& c) { return holds(c, q, who); }))
            {
                chosen = &v;
                break;
            }
        if (!chosen)
        {
            if (turn.optional)
                continue;
            return {};
        }
        // One alternative at random; another if a fact it needs is missing.
        r = mix(r + t);
        const std::size_t n = chosen->texts.size(), start = n == threadCount ? threadIndex : std::size_t(r % n);
        std::string said;
        bool done = false;
        std::size_t k = 0;
        for (; k < n && !done; ++k)
        {
            auto kept = binds;
            Expansion x{q, who, mix(r ^ 0x7A1CULL ^ k), binds, trace};
            done = expandInto(chosen->texts[(start + k) % n], x, 0, said);
            if (!done)
                binds = std::move(kept);
        }
        if (done && n > 1)                  // A line with no alternatives (most speaker-tagged ones) keeps the thread.
        {
            threadIndex = (start + k - 1) % n;
            threadCount = n;
        }
        if (!done)
        {
            if (turn.optional)
                continue;
            return {};
        }
        tidy(said);
        out.lines.push_back({who, said});
        marks.insert(chosen->marks.begin(), chosen->marks.end());
    }
    out.id = e.id;
    out.repeat = e.repeat;
    out.chain.push_back(e.id);
    // What saying it changes: what it remembers, and that it was said (for once and cooldown).
    const auto idOf = [&](int who) {
        const Speaker& p = who == 0 || q.alone ? q.a : q.b;
        const auto f = p.facts.find("id");
        return f == p.facts.end() ? std::string() : f->second;
    };
    const int selfWho = e.scene ? 0 : speaker;
    const auto self = idOf(selfWho), other = q.alone ? std::string() : idOf(1 - selfWho);
    const auto pair = other.empty() || self.empty() ? std::string() : std::min(self, other) + "|" + std::max(self, other);
    const auto town = q.facts.count("here.region") ? q.facts.at("here.region") : std::string();
    const auto add = [&](const std::string& scope, const std::string& owner, const std::string& key, const std::string& value, double days) {
        if (!owner.empty())
            out.effects.push_back({scope, owner, key, value, days});
    };
    for (const auto& m : e.remember)
        add(m.scope == "pair" ? "pair" : m.scope == "town" ? "town" : "person",
            m.scope == "self" ? self : m.scope == "other" ? other : m.scope == "pair" ? pair : town, m.key, m.value, m.days);
    for (const auto& r : e.regard)
        if (!self.empty() && !other.empty())
            for (const auto& [axis, amount] : r.change)
                add("regard", r.who == "self" ? self + "|" + other : other + "|" + self, axis, std::to_string(amount), -1);
    for (auto& x : out.effects)
        x.why = e.id;
    switch (e.limit)
    {
    case Entry::Limit::Once: add("person", self, "said:" + e.id, "yes", -1); break;
    case Entry::Limit::PerDay: add("person", self, "said:" + e.id, "yes", 1); break;
    case Entry::Limit::Cooldown: add("person", self, "said:" + e.id, "yes", e.cooldownDays); break;
    case Entry::Limit::PerOther: add("pair", pair, "said:" + e.id, "yes", -1); break;
    default: break;
    }
    return out;
}

Said Library::pick(const Query& q, std::uint64_t seed, const std::function<bool(const std::string&)>& heard,
                   const std::deque<std::string>& recent, Trace* trace) const
{
    return pickOne(q, seed, heard, recent, 0, false, trace);
}

Said Library::converse(const Query& q, std::uint64_t seed, const std::function<bool(const std::string&)>& heard,
                       const std::deque<std::string>& recent, Trace* trace) const
{
    auto all = pickOne(q, seed, heard, recent, 0, false, trace);
    if (all.id.empty())
        return all;
    // Each rule's `then` handed on: the next beat is the other's (or the same speaker's) best rule for that concept,
    // with what has been remembered so far.
    auto next = q;
    for (const auto& e : all.effects)
        remembered(e, next);
    const Entry* e = find(all.id);
    int speaker = 0;
    // A shrug ("Mm.") ends it, whatever the rule hands on: the next beat would answer something not said (phase 7).
    const auto shrugged = [&] { return !all.lines.empty() && settings_.ends.count(all.lines.back().second) > 0; };
    for (int beat = 1; e && !e->scene && !e->thenConcept.empty() && !q.alone && beat < 8 && !shrugged(); ++beat)
    {
        speaker = e->thenWho == "self" ? speaker : 1 - speaker;
        next.concept = e->thenConcept;
        auto said = pickOne(next, mix(seed + std::uint64_t(beat) * 7919), heard, {}, speaker, true, nullptr);
        if (said.id.empty())
        {
            if (trace)
                trace->why += "; then " + next.concept + " (" + (speaker ? "b" : "a") + "): nothing fits, the talk ends";
            break;
        }
        if (trace)
            trace->why += "; then " + next.concept + " (" + (speaker ? "b" : "a") + "): " + said.id;
        all.lines.insert(all.lines.end(), said.lines.begin(), said.lines.end());
        all.chain.push_back(said.id);
        for (const auto& x : said.effects)
        {
            remembered(x, next);
            all.effects.push_back(x);
        }
        e = find(said.id);
    }
    return all;
}

Said Library::pickOne(const Query& q, std::uint64_t seed, const std::function<bool(const std::string&)>& heard,
                      const std::deque<std::string>& recent, int speaker, bool chained, Trace* trace) const
{
    const auto list = byConcept_.find(q.concept);
    if (list == byConcept_.end())
    {
        if (trace)
            trace->why = "nothing is written for " + q.concept;
        return {};
    }
    struct Option
    {
        Said said;
        const Entry* entry;
        bool heard;
    };
    std::vector<Option> options;
    for (const auto i : list->second)
    {
        const auto& e = entries_[i];
        if (std::find(recent.begin(), recent.end(), e.id) != recent.end())
        {
            if (trace)
                trace->passed.push_back(e.id + ": said here lately");
            continue;
        }
        if (e.scene && (chained || speaker != 0))
            continue;                               // (A scene is a whole talk: never a chain's later beat.)
        if (!fits(e, q, speaker))
        {
            if (trace)
            {
                bool told = false;
                for (const auto& c : e.when)
                    if (!holds(c, q, speaker))
                    {
                        const std::string* v = fact(c.ns, c.name, c.full, q, speaker);
                        trace->passed.push_back(e.id + ": " + (c.full.empty() ? c.name : c.full) + " is " + (v ? *v : "(nothing)"));
                        told = true;
                        break;
                    }
                if (!told)
                    trace->passed.push_back(e.id + ": said already (once, or its cooldown)");
            }
            continue;
        }
        auto said = render(e, q, seed, nullptr, speaker);
        if (said.id.empty())
        {
            if (trace)
                trace->passed.push_back(e.id + ": a turn can't be spoken (no line holds, or a fact is missing)");
            continue;
        }
        options.push_back({std::move(said), &e, !e.repeat && heard && heard(e.id)});
    }
    if (options.empty())
    {
        if (trace)
            trace->why = "nothing fits";
        return {};
    }
    std::string why;
    // What must be said, if it can be; filler only when nothing else holds.
    int top = -1000;
    for (const auto& o : options)
        top = std::max(top, o.entry->priority);
    if (top >= settings_.strict)
    {
        options.erase(std::remove_if(options.begin(), options.end(), [&](const Option& o) { return o.entry->priority < top; }), options.end());
        why = "priority " + std::to_string(top) + " must be said; ";
    }
    else if (top >= 0)
        options.erase(std::remove_if(options.begin(), options.end(), [](const Option& o) { return o.entry->priority < 0; }), options.end());
    else
        why = "only filler holds; ";
    // Tiers by score, best first; a tier is chosen, then one within it. A chain's later beats take the best tier, so
    // its answers stay coherent.
    std::vector<int> scores;
    for (const auto& o : options)
        scores.push_back(o.entry->score);
    std::sort(scores.begin(), scores.end(), std::greater<int>());
    scores.erase(std::unique(scores.begin(), scores.end()), scores.end());
    std::size_t tier = 0;
    if (scores.size() > 1 && top < settings_.strict && !chained && !settings_.bestOnly.count(q.concept))
    {
        const double lower = settings_.lower;
        const double best = settings_.best + (scores.size() == 2 ? lower : 0);
        const double roll = unit(mix(seed * 31 + 7));
        if (roll < best)
            tier = 0;
        else if (roll < best + settings_.next || lower <= 0)
            tier = 1;
        else
            tier = 2 + std::min(scores.size() - 3, std::size_t((roll - best - settings_.next) / lower * double(scores.size() - 2)));
    }
    std::vector<Option*> in, fresh;
    const auto gather = [&] {
        in.clear();
        fresh.clear();
        for (auto& o : options)
            if (o.entry->score == scores[tier])
            {
                in.push_back(&o);
                if (!o.heard)
                    fresh.push_back(&o);
            }
    };
    gather();
    // Everything in that tier heard already: the nearest tier with something not heard, rather than a repeat (phase 7:
    // no conversation heard twice in a hundred hours). Not where only the best will do.
    if (fresh.empty() && scores.size() > 1 && top < settings_.strict && !chained && !settings_.bestOnly.count(q.concept))
    {
        const std::size_t chosenTier = tier;
        for (std::size_t step = 1; step < scores.size() && fresh.empty(); ++step)
            for (const std::size_t t : {chosenTier + step, chosenTier >= step ? chosenTier - step : scores.size()})
                if (t < scores.size() && fresh.empty())
                {
                    tier = t;
                    gather();
                }
        if (fresh.empty())
        {
            tier = chosenTier;
            gather();
        }
        else
            why += "all heard in tier " + std::to_string(chosenTier + 1) + "; ";
    }
    auto& from = fresh.empty() ? in : fresh;
    double total = 0;
    for (const auto* o : from)
        total += o->entry->weight;
    double roll = unit(mix(seed * 131 + 17)) * total;
    Option* chosen = from.back();
    for (auto* o : from)
        if ((roll -= o->entry->weight) <= 0)
        {
            chosen = o;
            break;
        }
    if (trace)
    {
        for (const auto& o : options)
        {
            const auto t = std::size_t(std::find(scores.begin(), scores.end(), o.entry->score) - scores.begin());
            trace->held.push_back({o.entry->id, o.entry->score, o.entry->priority, int(t), o.entry->weight, o.heard});
        }
        trace->why = why + "tier " + std::to_string(tier + 1) + " of " + std::to_string(scores.size()) + " (score " +
                     std::to_string(scores[tier]) + "), " + std::to_string(in.size()) + " in it, " +
                     std::to_string(fresh.size()) + " not heard; " + chosen->entry->id + " by weight";
        render(*chosen->entry, q, seed, trace, speaker);    // (Again, for its grammar steps.)
    }
    return std::move(chosen->said);
}

std::string Library::personalLayer(const std::string& npc)
{
    std::string name = "p_" + npc;
    for (auto& c : name)
        if (!(std::islower(static_cast<unsigned char>(c)) || std::isdigit(static_cast<unsigned char>(c))))
            c = '_';
    return name;
}

void Library::reindex()
{
    byConcept_.clear();
    byId_.clear();
    for (std::size_t i = 0; i < entries_.size(); ++i)
    {
        byId_[entries_[i].id] = i;
        for (const auto& c : entries_[i].concepts)
            byConcept_[c].push_back(i);
    }
}

bool Library::setGrammar(const std::string& name, const std::string& text, std::string& problem)
{
    if (name == "plain")
    {
        problem = "plain is the game's own (a file): a dialect or a person's layer may not replace it";
        return false;
    }
    // Only the one it names: a text that declares another grammar is refused.
    std::istringstream lines(text);
    for (std::string raw; std::getline(lines, raw);)
        if (const auto line = trim(raw); line.rfind("grammar ", 0) == 0 && trim(line.substr(8)) != name)
        {
            problem = "db:" + name + ": declares " + line + "; only 'grammar " + name + "' here";
            return false;
        }
    auto kept = grammars_;
    grammars_.erase(name);
    if (!parseGrammar(text, "db:" + name, problem))
    {
        grammars_ = std::move(kept);
        return false;
    }
    return true;
}

bool Library::setCharacter(const std::string& npc, const std::string& text, std::string& problem)
{
    const auto file = "voice:" + npc;
    // Their rules' IDs, made theirs.
    std::string own;
    std::istringstream in(text);
    for (std::string raw; std::getline(in, raw);)
    {
        const auto line = trim(raw);
        if (line.rfind("scene ", 0) == 0)
        {
            problem = file + ": a character's table holds rules, not scenes";
            return false;
        }
        own += (line.rfind("rule ", 0) == 0 ? "rule " + npc + "." + trim(line.substr(5)) : raw) + "\n";
    }
    auto keptEntries = entries_;
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(), [&](const Entry& e) { return e.file == file; }), entries_.end());
    reindex();
    const auto before = entries_.size();
    if (!parse(own, file, problem))
    {
        entries_ = std::move(keptEntries);
        reindex();
        return false;
    }
    for (std::size_t i = before; i < entries_.size(); ++i)
    {
        Criterion c;
        c.ns = Ns::Self;
        c.name = "id";
        c.values = {npc};
        entries_[i].when.insert(entries_[i].when.begin(), c);
        entries_[i].score += settings_.namedPerson;
    }
    return true;
}

std::string Library::voiceCard(const Speaker& speaker) const
{
    // For a model voicing them (phase 5): so its words sound like the same person the library's do.
    const auto fact = [&](const char* key) {
        const auto f = speaker.facts.find(key);
        return f == speaker.facts.end() ? std::string() : f->second;
    };
    static const std::map<std::string, std::string> Glosses{
        {"greet", "hello"}, {"farewell", "goodbye"}, {"agree", "yes"}, {"refuse", "no"}, {"surprise", "surprise"},
        {"annoyed", "annoyance"}, {"very", "very"}, {"little", "a little"}, {"good", "good"}, {"bad", "bad"},
        {"worthless", "worthless"}, {"luck", "luck"}, {"child", "a child"}, {"pet", "a friend"}, {"money", "money"},
        {"cold", "cold"}, {"hot", "hot"}, {"rain", "rain"}, {"tired", "tired"}, {"drunk", "drunk"},
        {"nonsense", "nonsense"}, {"tag", "a question at the end"}, {"home", "home"}, {"thanks", "thanks"},
        {"dear", "name for their spouse"}, {"saying", "saying"}, {"mum", "word for their mother"},
        {"dad", "word for their father"}};
    const auto quoted = [](const std::vector<Alt>& alts, std::size_t most) {
        std::string out;
        for (std::size_t i = 0; i < alts.size() && i < most; ++i)
            if (!alts[i].text.empty() && alts[i].text.find('#') == std::string::npos)
                out += (out.empty() ? "'" : " or '") + alts[i].text + "'";
        return out;
    };
    std::string card;
    const auto from = fact("from"), accent = fact("accent");
    if (!from.empty())
    {
        auto place = from;
        place[0] = char(std::toupper(static_cast<unsigned char>(place[0])));
        const auto layer = grammars_.find(from);
        if (layer == grammars_.end())
            card = "Grew up in " + place + ", and speaks the realm's plain common speech.";
        else if (accent == "none")
            card = "Grew up in " + place + " but has lost the accent: speaks plainly.";
        else
        {
            card = "Grew up in " + place + (accent == "broad" ? " and speaks broad " + place + ", always in its words"
                                                              : " and still uses its words now and then") + ". Their words:";
            // The words that show most first; a word plain says too is no sign of where they're from.
            static const std::vector<std::string> Order{"greet", "farewell", "very", "agree", "refuse", "good", "bad", "worthless",
                                                        "luck", "nonsense", "child", "money", "surprise", "annoyed", "tired",
                                                        "cold", "hot", "rain", "little", "tag", "home", "thanks"};
            std::vector<std::string> names(Order);
            for (const auto& [name, symbol] : layer->second)
                if (std::find(Order.begin(), Order.end(), name) == Order.end() && name.find('.') == std::string::npos)
                    names.push_back(name);
            std::set<std::string> plainWords;
            if (const auto plain = grammars_.find("plain"); plain != grammars_.end())
                for (const auto& [name, symbol] : plain->second)
                    for (const auto& e : symbol.entries)
                        for (const auto& a : e.alts)
                            plainWords.insert(a.text);
            int listed = 0;
            for (const auto& base : names)
                for (const auto& name : {base, base + ".broad"})
                {
                    const auto found = layer->second.find(name);
                    const bool broad = name != base;
                    if (found == layer->second.end() || (broad && accent != "broad") || found->second.habit || listed >= 10)
                        continue;
                    for (const auto& e : found->second.entries)
                        if (e.when.empty())
                        {
                            std::vector<Alt> own;
                            for (const auto& a : e.alts)
                                if (!plainWords.count(a.text))
                                    own.push_back(a);
                            const auto words = quoted(own, 2);
                            const auto gloss = Glosses.count(base) ? Glosses.at(base) : base;
                            if (!words.empty())
                                card += std::string(listed++ ? ";" : "") + " " + (broad ? "broadest, " : "") + gloss + " " + words;
                            break;
                        }
                }
            if (card.size() > 320)                  // (Room left for their habits.)
                card = card.substr(0, card.rfind(';', 316));
            card += ".";
        }
    }
    // Habits: their own oath, chosen once for good, as the library chooses it.
    Query alone;
    alone.a = speaker;
    alone.alone = true;
    std::set<std::string> habits;
    auto layers = speaker.grammar;
    layers.push_back("plain");
    for (const auto& l : layers)
        if (const auto g = grammars_.find(l); g != grammars_.end())
            for (const auto& [name, symbol] : g->second)
                if (symbol.habit && !symbol.pairHabit && name.find('.') == std::string::npos)
                    habits.insert(name);
    // (The oath, the saying and the name for a spouse first; the rest while there's room, each whole.)
    std::vector<std::string> order;
    for (const char* first : {"oath", "saying", "dear"})
        if (habits.erase(first))
            order.push_back(first);
    order.insert(order.end(), habits.begin(), habits.end());
    for (const auto& h : order)
    {
        std::string said;
        if (!expand("#" + h + "#", alone, 0, 1, said) || said.empty())
            continue;
        const auto sentence = std::string(card.empty() ? "" : " ") + "Their own " + (Glosses.count(h) ? Glosses.at(h) : h) +
                              ", always: '" + said + "'.";
        if (card.size() + sentence.size() <= 480)
            card += sentence;
    }
    return card;
}

std::string jobCategory(const std::string& title, const std::string& label, const std::string& role, int age)
{
    if (age < 15)
        return "child";
    // A post's title is its founder's work label: "digs ore at Deepvein", "works at The Wharf Tannery". Read as words
    // (never inside one: "Larkfield" is no field, "male" no ale), the work first, and the place's name only if the work
    // says nothing ("works at"), from its last word ("Tannery", not "Wharf").
    struct Word
    {
        std::string text;
        bool name = false;                  // Capitalised: a place's or a House's name.
    };
    std::vector<Word> work;
    std::string w;
    bool capital = false;
    const auto end = [&]() {
        if (!w.empty())
            work.push_back({w, capital});
        w.clear();
        capital = false;
    };
    for (const char raw : title + " " + label)
    {
        const auto c = static_cast<unsigned char>(raw);
        if (std::isalpha(c) || raw == '\'')
        {
            if (w.empty())
                capital = std::isupper(c) != 0;
            w += char(std::tolower(c));
        }
        else
            end();
    }
    end();
    // A word stem: "bake*" is any word starting so; "ore" is the word (or "ores", "ore's").
    const auto is = [](const std::string& word, const char* stem) {
        const std::string s(stem);
        if (s.back() == '*')
            return word.compare(0, s.size() - 1, s, 0, s.size() - 1) == 0;
        return word == s || word == s + "s" || word == s + "es";
    };
    struct Kind
    {
        const char* job;
        std::vector<const char*> stems;
    };
    static const std::vector<Kind> kinds{
        {"apprentice", {"apprentic*"}},
        {"soldier", {"garrison*", "soldier*"}},
        {"guard", {"watch", "gate", "guard*", "sergeant*", "warden*", "constable*", "gaoler*", "patrol*"}},
        {"smith", {"smith*", "forge*", "anvil*", "armourer*", "armorer*", "farrier*", "ironwork*", "foundr*", "arsenal*"}},
        {"baker", {"bake*", "bread", "oven*", "loaf", "loaves"}},
        {"brewer", {"brew*", "ale", "alehouse*", "malt*"}},
        {"innkeeper", {"inn", "innkeep*", "tavern*", "taproom*", "barkeep*"}},
        {"cook", {"cook*", "kitchen*"}},
        {"farmer", {"farm*", "field", "plough*", "harvest*", "orchard*", "grain", "miller*", "mill", "salt", "salter*", "vine*", "vineyard*", "bee", "hive*"}},
        {"herder", {"shepherd*", "drover*", "herd*", "stable*", "sheep", "cattle", "pig", "rabbit", "goat", "milk*", "dairy*"}},
        {"fisher", {"fish*", "net", "eel*", "boat", "trapper*", "smokehouse*"}},
        {"sailor", {"sail*", "ship", "harbour*", "dock*", "wharf*", "quay*", "pier", "voyage*"}},
        {"miner", {"mine", "miner*", "ore", "quarr*", "pit", "charcoal*", "peat", "stone", "clay"}},
        {"healer", {"heal*", "bonesetter*", "physician*", "apothecar*", "herbalist*", "infirmar*", "mending", "sick", "phial*"}},
        {"priest", {"priest*", "chapel*", "shrine*", "cathedral*", "acolyte*", "clergy", "saint*", "sermon*"}},
        {"scholar", {"scribe*", "scholar*", "record*", "librar*", "clerk*", "teach*", "cartograph*", "copy*", "copying", "roll", "lesson", "account*"}},
        {"woodcutter", {"wood", "logger*", "logging", "timber*", "sawmill*", "reed"}},
        {"carter", {"carter*", "carries", "carry", "porter*", "caravan*", "wagon*", "load"}},
        {"noble", {"lord*", "lady", "ladies", "noble*", "prince*", "heir*", "ruling", "rules"}},
        {"servant", {"servant*", "maid*", "valet*", "footman", "footmen", "steward*", "servic*", "serving", "chamber*", "garden*"}},
        {"beggar", {"beg", "begging", "beggar*"}},
        {"official", {"council*", "magistrate*", "toll", "official*", "tax", "court*", "chancellor*", "plead*", "counting", "office"}},
        {"crafter", {"carpenter*", "cooper*", "tailor*", "weav*", "potter*", "mason*", "tanner*", "chandler*", "tinker*",
                     "jewel*", "fletcher*", "netmak*", "boatwright*", "glass*", "rope*", "dye*", "colour*", "wool*", "candle*", "vat", "kiln*", "plaster*", "needle*", "tile*"}},
        {"laborer", {"labour*", "labor*", "sweep*", "wash*", "messenger*", "message", "lamplighter*", "lamp", "errand*",
                     "haul*", "unload*", "dig", "yard", "rag"}},
        {"merchant", {"shop*", "sell*", "seller*", "trader*", "trade", "merchant*", "stall*", "store*", "goods", "mercantile*", "sundries", "ironmonger*", "buy*", "buying", "trading", "weigh*", "bale"}}};
    const auto category = [&](const std::string& word) -> const char* {
        for (const auto& k : kinds)
            for (const auto* stem : k.stems)
                if (is(word, stem))
                    return k.job;
        return nullptr;
    };
    // Keeping house is no trade (nor is "keeps" a shop, alone).
    const auto has = [&](const char* stem) {
        return std::any_of(work.begin(), work.end(), [&](const Word& x) { return is(x.text, stem); });
    };
    if (has("keep*") && has("house") && !has("inn") && !has("shop*"))
        return role == "merchant" ? "merchant" : "none";
    // The work: the kinds in their order, over its own words.
    for (const auto& k : kinds)
        for (const auto& x : work)
            if (!x.name && std::any_of(k.stems.begin(), k.stems.end(), [&](const char* stem) { return is(x.text, stem); }))
                return role == "guard" && std::string(k.job) != "soldier" ? "guard" : k.job;
    if (role == "guard")
        return "guard";
    // The place's name, from its last word.
    for (auto x = work.rbegin(); x != work.rend(); ++x)
        if (x->name)
            if (const char* job = category(x->text))
                return job;
    if (role == "merchant" || has("keep*") || has("help*") || has("minding"))
        return "merchant";                          // (A shop's keeper or helper, its trade not in its name.)
    return "none";
}
} // namespace ratw::talk
