// Talk (RatwTalk.h; Docs/Design/62-talk.md).
#include "RatwTalk.h"
#include "RatwJsonDoc.h"
#include "RatwClaims.h"
#include "RatwTopics.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
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

std::string lower(std::string s)
{
    for (auto& c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
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
// A tag's key, a remembered key: lower-case letters, digits and _.
bool lowerKey(const std::string& k)
{
    return !k.empty() && k.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") == std::string::npos;
}
// (Doc 84 §13.4, Phase 13: "player", what the window knows of the player spoken to: player.knows, player.named...,
// looked up by its full key as here.* is.)
bool isSharedNs(const std::string& ns)
{
    return ns == "pair" || ns == "here" || ns == "topic" || ns == "world" || ns == "player";
}

Ns personNs(const std::string& ns)
{
    return ns == "a" ? Ns::A : ns == "b" ? Ns::B : ns == "self" ? Ns::Self : Ns::Other;
}

Ns sharedNs(const std::string& ns)
{
    return ns == "pair" ? Ns::Pair : ns == "here" || ns == "player" ? Ns::Here : ns == "topic" ? Ns::Topic : Ns::World;
}

const std::set<std::string> Modifiers{"capitalize", "a", "s"};
const std::set<std::string> Pronouns{"they", "them", "their", "theirs", "themself"};
// A topic's pronoun (Phase 6): its subject's (they, their...) or its victim's (victim_they...).
bool topicPronoun(const std::string& name)
{
    return Pronouns.count(name) || (name.rfind("victim_", 0) == 0 && Pronouns.count(name.substr(7)));
}
// One of a person's people's pronoun (Phase 6: spouse_their, child_they...): the role ("" if it is no such word).
std::string rolePronoun(const std::string& name)
{
    static const std::set<std::string> Roles{"spouse", "child", "mother", "father", "neighbour", "shopkeeper", "priest", "leader"};
    const auto at = name.rfind('_');
    if (at == std::string::npos || !Pronouns.count(name.substr(at + 1)) || !Roles.count(name.substr(0, at)))
        return {};
    return name.substr(0, at);
}

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
bool hasMove(const Moves& book, const std::vector<std::string>& moves, const std::string& kind);   // (Below.)
} // namespace

struct Library::Expansion
{
    const Query& q;
    int speaker = 0;
    std::uint64_t rng = 0;
    std::map<std::string, std::string>& binds;   // Symbols bound for the rest of the conversation ([x:#y#]).
    Trace* trace = nullptr;
    std::vector<std::string>* layers = nullptr;  // The layers whose words were used (LineInfo), when wanted.
    std::vector<std::string>* habits = nullptr;  // The habit symbols used (LineInfo), when wanted.
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
    trees_.clear();
    byMove_.clear();
    moves_.clear();
    conv_.clear();
    x_.clear();
    frames_.clear();
    budgets_ = {};
    inviteTaken_ = .5;
    goals_.clear();
    steps_.clear();
    moods_.clear();
    accent_ = {};
    issues_.clear();
    likes_.clear();
    temper_.clear();
    temperBy_.clear();
    owned_.clear();
    plain_.clear();
}

const Entry* Library::find(const std::string& id) const
{
    const auto f = byId_.find(id);
    if (f != byId_.end())
        return &entries_[f->second];
    // A mantle's ("<mantle>.<key>") or an NPC's own ("<npc>.<rule>"): doc 84 Phase 9.
    if (const auto dot = id.find('.'); dot != std::string::npos && !owned_.empty())
        for (const auto& key : {"m:" + id.substr(0, dot), "p:" + id.substr(0, dot)})
            if (const auto o = owned_.find(key); o != owned_.end())
                if (const auto e = o->second->byId.find(id); e != o->second->byId.end())
                    return &o->second->entries[e->second];
    return nullptr;
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

bool Library::ledgerFact(const std::string& key)
{
    // The talk ledger's facts (doc 84 §7.11, §8.7): the pair's (pair.last_talk, last_outcome, thread, kept_invite,
    // talked.<topic>, recall.<topic>, was.<k>) and a person's (was.<k>, told_me, promised, promise_what).
    static const std::set<std::string> Pair{"pair.last_talk", "pair.last_outcome", "pair.thread", "pair.kept_invite"};
    if (Pair.count(key) || key.rfind("pair.talked.", 0) == 0 || key.rfind("pair.recall.", 0) == 0 || key.rfind("pair.was.", 0) == 0)
        return true;
    const auto dot = key.find('.');
    if (dot == std::string::npos)
        return false;
    const auto name = key.substr(dot + 1);
    return name.rfind("was.", 0) == 0 || name == "told_me" || name == "promised" || name == "promise_what";
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
        if (v.isString() && v.asString("") == "list")
            k.type = FactKind::Type::List;          // (Phase 6: its words are opinions.json's items.)
        else if (v.isString() && (v.asString("") == "text" || v.asString("") == "number"))
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
    for (const char* ns : {"pair", "here", "topic", "world", "player"})
        for (const auto& [name, v] : doc.object(ns).fields())
        {
            if (name == "about")
                continue;                           // (player's own note: doc 84 Phase 13.)
            const auto full = std::string(ns) + "." + name;
            kindOf(full, v, shared_[full]);
            if (person_.count(name))
                continue;                           // (A person's name too, world.mood beside self.mood: written in full only.)
            if (aliases_.count(name))
                twice.insert(name);
            aliases_[name] = full;
        }
    for (const auto& name : twice)
        aliases_.erase(name);                     // Written in full where two namespaces have it.
    // The conversation's own facts and a bound third party's own (doc 84 §7.11), never by a bare name: conv.move,
    // conv.beat...; x.affinity, x.disposition (the rest of x.* are a person's facts).
    for (const auto& [name, v] : doc.object("conv").fields())
        if (name != "about")
            kindOf("conv." + name, v, conv_[name]);
    for (const auto& [name, v] : doc.object("x").fields())
        if (name != "about")
            kindOf("x." + name, v, x_[name]);
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
    // §37: the hybrid ("hybrid": true; "hybrid_stitch": the lines stitched after a body, goodbyes aside).
    settings_.hybrid = doc.boolean("hybrid", false);
    settings_.hybridStitch = int(doc.number("hybrid_stitch", settings_.hybridStitch));
    settings_.mantleFirst = doc.boolean("mantle_first", false);   // (§43: a wearer's own content first.)
    // Doc 84 §12 (Phase 10): how long a dead wolf's mantle rests, and the pools a dialect with nothing to give borrows.
    settings_.mantleRestDays = doc.number("mantle_rest_days", settings_.mantleRestDays);
    settings_.mantleFallback.clear();
    for (const auto& [from, to] : doc.object("mantle_fallback").fields())
        if (!from.empty() && to.isString() && !to.asString().empty())
            settings_.mantleFallback[from] = to.asString();
    for (const auto& [key, set] : {std::pair<const char*, std::set<std::string>*>{"concepts", &settings_.concepts},
                                   {"repeat", &settings_.repeat}, {"alone", &settings_.alone}, {"best_only", &settings_.bestOnly},
                                   {"ends", &settings_.ends}})
        for (const auto& v : doc.array(key))
            set->insert(v.asString(""));
    // The towns that speak another's dialect (doc 83, Phase 4): {"fenshore": "mirebridge", ...}.
    settings_.dialectOf.clear();
    for (const auto& [town, dialect] : doc.object("dialect_of").fields())
        if (!town.empty() && dialect.isString() && !dialect.asString().empty())
            settings_.dialectOf[town] = dialect.asString();
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
    // Salience (doc 79, 10.3): {"when": "business=failing", "never": ["can't complain", ...]}, the speaker's facts.
    settings_.salience.clear();
    for (const auto& rule : doc.array("salience"))
    {
        Settings::Salience one;
        for (const auto& token : words(rule.string("when")))
        {
            Criterion c;
            std::string error;
            if (!criterion(token, Where::When, 0, c, error))
            {
                problem = "settings.json: salience: " + error;
                return false;
            }
            one.when.push_back(std::move(c));
        }
        for (const auto& v : rule.array("never"))
            if (auto phrase = lower(v.asString("")); !phrase.empty())
                one.never.push_back(std::move(phrase));
        if (one.when.empty() || one.never.empty())
        {
            problem = "settings.json: a salience rule needs its `when` and its `never` words";
            return false;
        }
        settings_.salience.push_back(std::move(one));
    }
    return true;
}

bool Library::readMoves(const std::string& text, std::string& problem)
{
    if (!moves_.read(text, settings_.concepts, problem))
        return false;
    // The conversation's own facts take the move and tone words moves.json gives (facts.json may declare them "text").
    std::set<std::string> names;
    for (const auto& [name, m] : moves_.all())
    {
        names.insert(name);
        for (const auto& v : m.variants)
            names.insert(name + "." + v);
    }
    for (const char* key : {"move", "prev"})
        if (const auto k = conv_.find(key); k != conv_.end())
            k->second = {FactKind::Type::Words, names};
    if (const auto k = conv_.find("tone"); k != conv_.end())
        k->second = {FactKind::Type::Words, {moves_.tones().begin(), moves_.tones().end()}};
    return true;
}

bool Library::readFrames(const std::string& text, std::string& problem)
{
    // Doc 84 §7.4 (Phase 3): {"frames": {"strangers": {"when": "pair.band=strangers", "open": ["greet"], "budget": 0.6,
    // "forbid": ["tease"]}, ...}, "budget": {"chatty": [8, 12], ..., "friends_or_kin": 2, "least": 2, "most": 14},
    // "invite_taken": 0.5}. Checked: every field known, every fact, move and tone.
    frames_.clear();
    budgets_ = {};
    curt_ = {};
    goals_.clear();
    inviteTaken_ = .5;
    json::Value doc;
    std::string error;
    if (!json::parseUnique(text, doc, error) || !doc.isObject())
    {
        problem = "frames.json: " + (error.empty() ? "not a JSON object" : error);
        return false;
    }
    std::vector<std::string> errors;
    const auto fail = [&](const std::string& m) { errors.push_back("frames.json: " + m); };
    if (moves_.empty())
        fail("it names moves: moves.json is needed beside it");
    static const std::set<std::string> Top{"about", "frames", "budget", "invite_taken", "curt", "join", "thread", "goals", "moods", "accent"};
    // ("accent" is a library's from before Phase 6, a frame's step that accommodation (frames.json's accent) replaced: read
    // and ignored, so an older library still loads.)
    static const std::set<std::string> Fields{"about", "when", "open", "budget", "preclose", "forbid", "tones", "hedge", "accent"};
    for (const auto& [key, unused] : doc.fields())
        if (!Top.count(key))
            fail("no field '" + key + "' (about, frames, budget, invite_taken, curt, join, thread, goals, moods, accent)");
    moods_.clear();
    accent_ = {};
    const auto scale = [&](const std::string& what, double v) {
        if (!(v >= .2 && v <= 3))
            fail(what + " is a number from 0.2 to 3");
        return v;
    };
    for (const auto& [name, f] : doc.object("frames").fields())
    {
        Frame fr;
        fr.name = name;
        if (!f.isObject())
        {
            fail(name + " is an object");
            continue;
        }
        for (const auto& [key, unused] : f.fields())
            if (!Fields.count(key))
                fail(name + ": no field '" + key + "'");
        for (const auto& token : words(f.string("when")))
        {
            Criterion c;
            std::string why;
            if (!criterion(token, Where::When, 0, c, why))
                fail(name + ": " + why);
            else
                fr.when.push_back(std::move(c));
        }
        if (fr.when.empty())
            fail(name + ": a frame holds when something does (\"when\")");
        for (const auto& m : f.array("open"))
            if (moves_.known(m.asString("")))
                fr.open.push_back(m.asString(""));
            else
                fail(name + ": opens with '" + m.asString("") + "', not a move");
        for (const auto& m : f.array("forbid"))
            if (moves_.known(m.asString("")))
                fr.forbid.insert(m.asString(""));
            else
                fail(name + ": forbids '" + m.asString("") + "', not a move");
        fr.budget = scale(name + ": budget", f.number("budget", 1));
        fr.preclose = scale(name + ": preclose", f.number("preclose", 1));
        for (const auto& [tone, w] : f.object("tones").fields())
            if (!moves_.tone(tone) || !w.isNumber())
                fail(name + ": tones are moves.json's, each with a weight");
            else
                fr.tones[tone] = scale(name + ": tone " + tone, w.asNumber());
        fr.hedge = f.string("hedge");
        frames_.push_back(std::move(fr));
    }
    if (const auto* b = doc.find("budget"))
    {
        const auto talk = person_.find("talk");
        budgets_.byTalk.clear();
        for (const auto& [key, v] : b->fields())
        {
            if (key == "about")
                continue;
            if (key == "friends_or_kin" || key == "least" || key == "most")
            {
                const double n = v.asNumber(-1);
                if (n != double(int(n)) || n < 0 || n > 20)
                    fail("budget: " + key + " is a whole number of lines, 0 to 20");
                (key == "friends_or_kin" ? budgets_.friendsOrKin : key == "least" ? budgets_.least : budgets_.most) = int(n);
                continue;
            }
            if (talk != person_.end() && !talk->second.values.count(key))
            {
                fail("budget: '" + key + "' is not a talk (facts.json's talk), friends_or_kin, least or most");
                continue;
            }
            if (!v.isArray() || v.size() != 2 || v.items()[0].asNumber(-1) < 1 || v.items()[1].asNumber(-1) < v.items()[0].asNumber(-1))
            {
                fail("budget: " + key + " is [fewest, most] lines");
                continue;
            }
            budgets_.byTalk[key] = {int(v.items()[0].asNumber(0)), int(v.items()[1].asNumber(0))};
        }
        if (budgets_.least < 2 || budgets_.most < budgets_.least || budgets_.most > 20)
            fail("budget: least is 2 or more, most at least that and at most 20");
    }
    inviteTaken_ = doc.number("invite_taken", .5);
    if (!(inviteTaken_ >= 0 && inviteTaken_ <= 1))
        fail("invite_taken is a chance, 0 to 1");
    // Phase 4: curt endings and what they cost, who may join, how long a broken-off thread is kept.
    const auto known = [&](const json::Value& o, const std::set<std::string>& fields, const std::string& what) {
        for (const auto& [key, unused] : o.fields())
            if (!fields.count(key))
                fail(what + ": no field '" + key + "'");
    };
    if (const auto* curt = doc.find("curt"))
    {
        known(*curt, {"about", "chance", "effects"}, "curt");
        curt_.curtChance = curt->number("chance", .5);
        if (!(curt_.curtChance >= 0 && curt_.curtChance <= 1))
            fail("curt: chance is 0 to 1");
        for (const auto& [whose, axes] : curt->object("effects").fields())
        {
            if (whose != "self" && whose != "other")
                fail("curt: effects are \"self\" or \"other\"");
            for (const auto& [axis, v] : axes.fields())
                if (!v.isNumber() || std::abs(v.asNumber()) > 5 ||
                    !(axis == "affinity" || axis == "trust" || axis == "respect" || axis == "fear" || axis == "familiarity"))
                    fail("curt: an effect is an axis (affinity, trust, respect, fear, familiarity) and a change of 5 or less");
                else
                    curt_.curtEffects[whose][axis] = v.asNumber();
        }
    }
    if (const auto* join = doc.find("join"))
    {
        known(*join, {"about", "people", "tiles", "hearing", "rest"}, "join");
        curt_.joinPeople = int(join->number("people", 3));
        curt_.joinTiles = join->number("tiles", 3);
        curt_.joinHearing = join->number("hearing", .5);
        curt_.joinRest = join->number("rest", 60);
        if (curt_.joinPeople < 2 || curt_.joinPeople > 3)
            fail("join: people is 2 (nobody joins) or 3 (this build's most)");
        if (!(curt_.joinTiles > 0 && curt_.joinTiles <= 10) || !(curt_.joinHearing >= 0 && curt_.joinHearing <= 1) ||
            !(curt_.joinRest >= 0 && curt_.joinRest <= 3600))
            fail("join: tiles 0-10, hearing 0-1, rest 0-3600 seconds");
    }
    if (const auto* thread = doc.find("thread"))
    {
        known(*thread, {"about", "days"}, "thread");
        curt_.threadDays = thread->number("days", 2);
        if (!(curt_.threadDays >= 0 && curt_.threadDays <= 30))
            fail("thread: days 0-30");
    }
    // Phase 5 (§7.4, §8.5): the goals, in their order (the first that holds is pursued).
    for (const auto& [name, g] : doc.object("goals").fields())
    {
        if (name == "about")
            continue;
        Goal goal;
        goal.name = name;
        if (!g.isObject() || !lowerKey(name))
        {
            fail("goals: " + name + " is an object, its name lower-case letters, digits and _");
            continue;
        }
        known(g, {"about", "when", "plan", "days"}, "goals: " + name);
        for (const auto& token : words(g.string("when")))
        {
            Criterion c;
            std::string why;
            if (!criterion(token, Where::When, 0, c, why))
                fail("goals: " + name + ": " + why);
            else
                goal.when.push_back(std::move(c));
        }
        if (goal.when.empty())
            fail("goals: " + name + ": a goal holds when something does (\"when\")");
        for (const auto& m : g.array("plan"))
            if (moves_.known(m.asString("")) && moves_.find(m.asString(""))->first)
                goal.plan.push_back(m.asString(""));
            else
                fail("goals: " + name + ": its plan's '" + m.asString("") + "' is not a move that may open (moves.json)");
        if (goal.plan.empty())
            fail("goals: " + name + ": a plan of one move or more");
        goal.days = g.number("days", 7);
        if (!(goal.days >= 0 && goal.days <= 60))
            fail("goals: " + name + ": days 0-60");
        goals_.push_back(std::move(goal));
    }
    // Phase 6 (§7.4, §8.11): the town moods, in their order (the first that holds is the town's).
    const auto criteriaOf = [&](const std::string& tokens, const std::string& what, std::vector<Criterion>& out) {
        for (const auto& token : words(tokens))
        {
            Criterion c;
            std::string why;
            if (!criterion(token, Where::When, 0, c, why))
                fail(what + ": " + why);
            else
                out.push_back(std::move(c));
        }
        if (out.empty())
            fail(what + ": holds when something does");
    };
    for (const auto& [name, m] : doc.object("moods").fields())
    {
        if (name == "about")
            continue;
        Mood mood;
        mood.name = name;
        if (!m.isObject() || !lowerKey(name))
        {
            fail("moods: " + name + " is an object, its name lower-case letters, digits and _");
            continue;
        }
        known(m, {"about", "when", "mutes", "boost", "tones"}, "moods: " + name);
        criteriaOf(m.string("when"), "moods: " + name, mood.when);
        for (const auto& x : m.array("mutes"))
            if (moves_.known(x.asString("")))
                mood.mutes.insert(x.asString(""));
            else
                fail("moods: " + name + ": mutes '" + x.asString("") + "', not a move");
        for (const auto& [move, w] : m.object("boost").fields())
            if (!moves_.known(move) || !w.isNumber())
                fail("moods: " + name + ": boost is moves.json's moves, each with a weight");
            else
                mood.boosts[move] = scale("moods: " + name + ": boost " + move, w.asNumber());
        for (const auto& [tone, w] : m.object("tones").fields())
            if (!moves_.tone(tone) || !w.isNumber())
                fail("moods: " + name + ": tones are moves.json's, each with a weight");
            else
                mood.tones[tone] = scale("moods: " + name + ": tone " + tone, w.asNumber());
        moods_.push_back(std::move(mood));
    }
    // Phase 6 (§10.6): accommodation.
    if (const auto* a = doc.find("accent"))
    {
        known(*a, {"about", "broader", "plainer", "none_rises_to"}, "accent");
        accent_.on = true;
        for (const auto& [list, out] : {std::pair{"broader", &accent_.broader}, std::pair{"plainer", &accent_.plainer}})
            for (const auto& rule : a->array(list))
            {
                out->emplace_back();
                criteriaOf(rule.asString(""), std::string("accent: ") + list, out->back());
            }
        accent_.noneRisesTo = a->string("none_rises_to", "light");
        if (accent_.noneRisesTo != "none" && accent_.noneRisesTo != "light")
            fail("accent: none_rises_to is none or light");
    }
    if (!errors.empty())
    {
        problem.clear();
        for (std::size_t i = 0; i < errors.size() && i < 20; ++i)
            problem += (i ? "\n" : "") + errors[i];
        frames_.clear();
        budgets_ = {};
        curt_ = {};
        goals_.clear();
        moods_.clear();
        accent_ = {};
        return false;
    }
    return true;
}

void Library::deepen(Conversation& c, const Query& q)
{
    // A topic's aspects (topics.json, §7.3: Façade's tiers): a pair that has talked of it before starts one deeper
    // (pair.talked.<topic>, the ledger's), as conv.aspect for the lines that dig.
    c.depth = 0;
    c.conv.erase("aspect");
    if (c.topics.empty())
        return;
    const auto& book = topics::book();
    const auto t = book.topics.find(c.topics.back());
    if (t == book.topics.end() || t->second.aspects.empty())
        return;
    c.depth = q.facts.count("pair.talked." + c.topics.back()) ? 1 : 0;
    c.conv["aspect"] = t->second.aspects[std::min<std::size_t>(std::size_t(c.depth), t->second.aspects.size() - 1)];
}

bool Library::readSteps(std::string& problem)
{
    // Doc 84 §7.3 (Phase 5): the topic graph's steps (topics.json's `next`), their criteria checked against facts.json as
    // a rule's are; and the favourites' traits ("piety=devout": a person's fact).
    steps_.clear();
    std::vector<std::string> errors;
    const auto& book = topics::book();
    for (const auto& [name, t] : book.topics)
        for (const auto& [to, when] : t.next)
        {
            Step step;
            step.to = to;
            for (const auto& token : words(when))
            {
                Criterion c;
                std::string why;
                if (!criterion(token, Where::When, 0, c, why))
                    errors.push_back("topics.json: " + name + " -> " + to + ": " + why);
                else
                    step.when.push_back(std::move(c));
            }
            steps_[name].push_back(std::move(step));
        }
    for (const auto& [trait, list] : book.favourites)
    {
        Criterion c;
        std::string why;
        if (!criterion(trait, Where::When, 0, c, why))
            errors.push_back("topics.json: favourites: " + why);
    }
    if (!errors.empty())
    {
        problem = errors.front();
        steps_.clear();
        return false;
    }
    return true;
}

const std::vector<Library::Step>& Library::steps(const std::string& topic) const
{
    static const std::vector<Step> none;
    const auto found = steps_.find(topic);
    return found == steps_.end() ? none : found->second;
}

bool Library::muted(const std::string& said, const Query& q, int speaker) const
{
    std::string low;
    for (const auto& rule : settings_.salience)
        if (std::all_of(rule.when.begin(), rule.when.end(), [&](const Criterion& c) { return holds(c, q, speaker); }))
        {
            if (low.empty())
                low = lower(said);
            for (const auto& phrase : rule.never)
                if (low.find(phrase) != std::string::npos)
                    return true;
        }
    return false;
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
        // The talk ledger's (doc 84 §7.11, Phase 5): pair.talked.<topic> (days since the two talked of it), pair.recall.<topic>
        // (how that went), pair.was.<k> and a person's was.<k> (kept tags).
        static const FactKind Days{FactKind::Type::Number, {}};
        const bool talked = ns == "pair" && c.name.rfind("talked.", 0) == 0 && lowerKey(c.name.substr(7));
        const bool kept = (ns == "pair" && (c.name.rfind("recall.", 0) == 0 || c.name.rfind("was.", 0) == 0) &&
                           lowerKey(c.name.substr(c.name.find('.') + 1))) ||
                          (isPersonNs(ns) && c.name.rfind("was.", 0) == 0 && lowerKey(c.name.substr(4)));
        // A conversation's tag (doc 84 §7.1, {k=v}): any key, any word; the engine's own keys (facts.json "conv") as
        // they're declared.
        static const FactKind Tagged{FactKind::Type::Text, {}};
        if (ns == "conv")
        {
            c.ns = Ns::Conv;
            c.full = key;
            if (const auto k = conv_.find(c.name); k != conv_.end())
                kind = &k->second;
            else if (lowerKey(c.name))
                kind = &Tagged;
        }
        else if (ns == "x")
        {
            // A bound third party (binds x=...): a person's facts, their own (facts.json "x"), and what talk has said of
            // them (x.said.k, the ledger's: Phase 5).
            c.ns = Ns::X;
            c.full = key;
            if (const auto k = x_.find(c.name); k != x_.end())
                kind = &k->second;
            else if (const auto p = person_.find(c.name); p != person_.end())
                kind = &p->second;
            else if (c.name.rfind("said.", 0) == 0 && lowerKey(c.name.substr(5)))
                kind = &Tagged;
        }
        else if (isPersonNs(ns))
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
        if (memory || kept)
            kind = &Remembered;
        if (talked)
            kind = &Days;
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
        c.list = kind->type == FactKind::Type::List;
        if ((kind->type == FactKind::Type::Words || (c.list && !kind->values.empty())) && c.full != "here.region")   // (Regions may be groups: see parse.)
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
    if (c.list && (c.op == Criterion::Op::In || c.op == Criterion::Op::NotIn))
    {
        // A list fact (Phase 6: self.likes=honey_cakes): holds when the list, words between spaces, has one of them.
        bool has = false;
        if (v)
            for (const auto& w : words(*v))
                has = has || c.values.count(w) > 0;
        return c.op == Criterion::Op::In ? has : !has;
    }
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
    std::vector<const Grammar*> layers;
    for (const auto& [layer, g] : grammars_)
        layers.push_back(&g);
    if (checking_)
        layers.push_back(checking_);
    for (const auto* gp : layers)
        for (const auto& key : {name, name + ".broad"})
        if (const auto s = gp->find(key); s != gp->end())
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
    // A reference, never a copy (the conditional with a Grammar{} branch copied the whole plain grammar for every
    // alternative checked: 76% of the library's load once plain.grammar grew to 44 KB; Slice 2's review).
    static const Grammar none;
    const auto found = grammars_.find("plain");
    const Grammar& plain = found != grammars_.end() ? found->second : none;
    const auto checkRef = [&](const std::string& inside) {
        const auto ref = readRef(inside);
        if (ref.parts.empty() || std::any_of(ref.parts.begin(), ref.parts.end(), [](const std::string& p) { return p.empty(); }))
        {
            errors.push_back(where + ": #" + inside + "# is not a symbol or a fact");
            return;
        }
        if (ref.parts.size() == 2 && (isPersonNs(ref.parts[0]) || ref.parts[0] == "x"))
        {
            if (const auto role = rolePronoun(ref.parts[1]); !role.empty() && person_.count(role))
                return;                             // (#self.spouse_their#: one of their people's pronoun, Phase 6.)
            if (!person_.count(ref.parts[1]) && !Pronouns.count(ref.parts[1]) && !(ref.parts[0] == "x" && x_.count(ref.parts[1])))
                errors.push_back(where + ": #" + inside + "#: a person has no fact '" + ref.parts[1] + "'");
            return;
        }
        if (ref.parts.size() == 2 && ref.parts[0] == "conv")
        {
            if (!lowerKey(ref.parts[1]))
                errors.push_back(where + ": #" + inside + "#: a conversation's tag is lower-case letters, digits and _");
            return;
        }
        if (ref.parts.size() == 2 && ref.parts[0] == "topic" && topicPronoun(ref.parts[1]))
            return;                                 // (#topic.they#, #topic.victim_their#: Phase 6.)
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
        if (!plain.count(ref.parts[0]) && !(checking_ && checking_->count(ref.parts[0])))
        {
            errors.push_back(where + ": #" + ref.parts[0] + "# is not in the plain grammar" + (checking_ ? " nor the mantle's words" : ""));
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
            if (!plain.count(name) && !(checking_ && checking_->count(name)))
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

bool Library::readGrammarText(const std::string& text, const std::string& file, std::map<std::string, Grammar>& merged,
                              std::vector<GrammarLine>& pending, std::vector<std::string>& errors) const
{
    std::string current;
    std::istringstream in(text);
    std::string raw;
    int number = 0;
    const auto fail = [&](const std::string& message) { errors.push_back(file + ":" + std::to_string(number) + ": " + message); };
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
        std::string name = line.substr(0, at);
        if (name.empty())
        {
            fail("expected 'symbol = words | words'");
            continue;
        }
        // mark.<symbol>: a dialect's bank of habits, drawn again for a reused mantle's marks (§12.4, redrawHabit). Never
        // said by name, so in no layer below; plain holds the bank of plain speech.
        bool bank = false;
        if (name == "mark" && at < line.size() && line[at] == '.' && line.compare(at, 6, ".broad") != 0)
        {
            auto end = ++at;
            while (end < line.size() && (std::islower(static_cast<unsigned char>(line[end])) || std::isdigit(static_cast<unsigned char>(line[end])) || line[end] == '_'))
                ++end;
            if (end == at)
            {
                fail("a mark bank is mark.<symbol>");
                continue;
            }
            name += "." + line.substr(at, end - at), at = end, bank = true;
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
        if (bank && (broad || habit))
        {
            fail("a mark bank (" + name + ") has no .broad and no ! (its values become the wearer's own habit)");
            continue;
        }
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
        GrammarLine p{current, name, file + ":" + std::to_string(number), {}};
        for (const auto& a : entry.alts)
            p.texts.push_back(a.text);
        pending.push_back(p);
        symbol.entries.push_back(std::move(entry));
    }
    return errors.empty();
}

bool Library::parseGrammar(const std::string& text, const std::string& file, std::string& problem)
{
    std::vector<std::string> errors;
    auto merged = grammars_;
    std::vector<GrammarLine> pending;
    readGrammarText(text, file, merged, pending, errors);
    // Checked against the grammars with this file in: every symbol used, and every layer's symbol, is in plain.
    auto kept = std::move(grammars_);
    grammars_ = merged;
    for (const auto& p : pending)
    {
        if (p.grammar != "plain" && p.symbol.rfind("mark.", 0) != 0 && (!grammars_.count("plain") || !grammars_.at("plain").count(p.symbol)))
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
        if (!settings_.concepts.count(name) && !(mantleParse_ && name.rfind("graft.", 0) == 0))
            fail("'" + name + "' is not a concept (settings.json)");
        e->concepts.insert(name);
    };
    const auto addText = [&](Variant& v, const std::string& written) {
        // {k=v ...} before the words (doc 84 §7.1): the conversation's tags; `keep 7d` keeps them in the ledger too.
        std::string t = written;
        Tag tag;
        if (!t.empty() && t[0] == '{')
        {
            const auto close = t.find('}');
            if (close == std::string::npos)
                fail("a tag is {key=value} before the words");
            else
            {
                const auto parts = words(t.substr(1, close - 1));
                for (std::size_t i = 0; i < parts.size(); ++i)
                {
                    if (parts[i] == "keep" && i + 1 < parts.size())
                    {
                        if (!duration(parts[i + 1], tag.keep) || tag.keep <= 0)
                            fail("keep is a time ({k=v keep 7d})");
                        ++i;
                        continue;
                    }
                    const auto eq = parts[i].find('=');
                    const auto key = parts[i].substr(0, std::min(eq, parts[i].size()));
                    const auto value = eq == std::string::npos ? std::string() : parts[i].substr(eq + 1);
                    if (eq == std::string::npos || !lowerKey(key) || value.empty() ||
                        value.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_.-") != std::string::npos)
                        fail("a tag is {key=value}: lower-case words ('" + parts[i] + "')");
                    else if (conv_.count(key))
                        fail("conv." + key + " is the engine's own: a tag can't set it");
                    else
                        tag.set[key] = value;
                }
                if (tag.set.empty())
                    fail("a tag sets something: {key=value}");
                t = trim(t.substr(close + 1));
            }
        }
        if (t.find('{') != std::string::npos || t.find('}') != std::string::npos)
            fail("{ } hold a tag, before an alternative's words: {key=value} words");
        std::size_t longest = 0;
        if (checkText(t, errors, file + ":" + std::to_string(number), &longest) && longest > LongestLine)
            fail("a line of more than " + std::to_string(LongestLine) + " characters");
        v.texts.push_back(t);
        v.tags.push_back(std::move(tag));
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
            // remember self|other|pair|town|x: key[=value] [DURATION] (x: the third party the line binds, what talk has said
            // of them, kept in the talk ledger and read as x.said.key: doc 84 §7.1, §8.7, Phase 5)
            const auto colon = line.find(':');
            const auto scope = colon == std::string::npos ? std::string() : trim(line.substr(9, colon - 9));
            const auto parts = colon == std::string::npos ? std::vector<std::string>{} : words(line.substr(colon + 1));
            Entry::Remember r;
            r.scope = scope;
            double days = -1;
            if ((scope != "self" && scope != "other" && scope != "pair" && scope != "town" && scope != "x") || parts.empty() ||
                parts.size() > 2 || (parts.size() == 2 && !duration(parts[1], days)))
            {
                fail("remember is 'remember self|other|pair|town|x: key[=value] [1d|6h|30m|forever]'");
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
        // Doc 84 §7.1 (Phase 5): an invitation the world checks, and a talk-only promise.
        if (line.rfind("invite ", 0) == 0)
        {
            // invite here <hour> <days>: to meet again in this place at that hour, within so many days.
            const auto parts = words(line.substr(7));
            char* end = nullptr;
            const long hour = parts.size() == 3 ? std::strtol(parts[1].c_str(), &end, 10) : -1;
            const bool hourOk = end && !*end && hour >= 0 && hour <= 23;
            end = nullptr;
            const double days = parts.size() == 3 ? std::strtod(parts[2].c_str(), &end) : 0;
            if (parts.size() != 3 || parts[0] != "here" || !hourOk || !end || *end || days <= 0 || days > 14)
                fail("invite is 'invite here <hour 0-23> <days 1-14>'");
            else
                e->invitePlace = parts[0], e->inviteHour = int(hour), e->inviteDays = days;
            continue;
        }
        if (line.rfind("promise ", 0) == 0)
        {
            // promise self: <thing> <days>: a talk-only promise (never a world promise); let lapse, it is brought up.
            const auto colon = line.find(':');
            const auto who = colon == std::string::npos ? std::string() : trim(line.substr(8, colon - 8));
            const auto parts = colon == std::string::npos ? std::vector<std::string>{} : words(line.substr(colon + 1));
            char* end = nullptr;
            const double days = parts.size() == 2 ? std::strtod(parts[1].c_str(), &end) : 0;
            if (who != "self" || parts.size() != 2 || !end || *end || days <= 0 || days > 30 || !lowerKey(parts[0]))
                fail("promise is 'promise self: <thing, lower-case> <days 1-30>'");
            else
                e->promiseWhat = parts[0], e->promiseDays = days;
            continue;
        }
        // Doc 84 §7.1 (Phase 2): what a line does in the talk.
        const auto keyword = [&](const char* k) {
            const auto n = std::strlen(k);
            return line.compare(0, n, k) == 0 && (line.size() == n || line[n] == ' ');
        };
        if (keyword("does") || keyword("owes") || keyword("invites") || keyword("tone") || keyword("asserts") ||
            keyword("denies") || keyword("gist") || keyword("binds"))
        {
            const auto space = line.find(' ');
            const auto head = line.substr(0, space);
            const auto rest = space == std::string::npos ? std::string() : trim(line.substr(space + 1));
            if (moves_.empty() && head != "binds")
            {
                fail(head + ": there is no moves.json to say what lines do");
                continue;
            }
            if (head == "does")
            {
                if (!e->does.empty())
                {
                    fail("one `does` line (a scene names each turn's moves in it, / between them)");
                    continue;
                }
                if (!e->scene && rest.find('/') != std::string::npos)
                {
                    fail("a rule is one turn: its `does` names its moves, with no /");
                    continue;
                }
                std::vector<std::string> groups;
                {
                    std::string part;
                    std::istringstream in(rest);
                    while (std::getline(in, part, '/'))
                        groups.push_back(trim(part));
                }
                for (const auto& g : groups)
                {
                    std::vector<std::string> turn;
                    const auto ws = words(g);
                    if (ws.empty())
                        fail("does names a move for each turn (- for one not yet known)");
                    for (const auto& w : ws)
                        if (w == "-" && ws.size() == 1)
                            continue;
                        else if (!moves_.known(w))
                            fail("'" + w + "' is not a move (moves.json)");
                        else if (std::find(turn.begin(), turn.end(), w) == turn.end())
                            turn.push_back(w);
                    e->does.push_back(std::move(turn));
                }
            }
            else if (head == "owes" || head == "invites")
            {
                const auto colon = rest.find(':');
                const auto who = colon == std::string::npos ? std::string() : trim(rest.substr(0, colon));
                const auto list = colon == std::string::npos ? std::string() : trim(rest.substr(colon + 1));
                if ((who != "other" && who != "self" && who != "any") || list.empty() || list.find(' ') != std::string::npos)
                {
                    fail(head + " is '" + head + " other|self|any: <move>[|<move>]'");
                    continue;
                }
                Entry::Owed o;
                o.who = who;
                o.soft = head == "invites";
                for (const auto& m : split(list, '|'))
                    if (!moves_.known(m))
                        fail("'" + m + "' is not a move (moves.json)");
                    else
                        o.moves.push_back(m);
                e->owes.push_back(std::move(o));
            }
            else if (head == "tone")
            {
                if (!e->tones.empty())
                {
                    fail("one `tone` line (a scene's may name each turn's, / between them)");
                    continue;
                }
                std::vector<std::string> groups;
                std::string part;
                std::istringstream in(rest);
                while (std::getline(in, part, '/'))
                    groups.push_back(trim(part));
                if (!e->scene && groups.size() != 1)
                    fail("a rule is one turn: one tone");
                for (const auto& g : groups)
                    if (!moves_.tone(g))
                        fail("'" + g + "' is not a tone (moves.json: light, even, serious, grave, tender, cross)");
                    else
                        e->tones.push_back(g);
            }
            else if (head == "asserts" || head == "denies" || head == "gist")
            {
                // A rule's one turn; in a scene the turn's number may come first (asserts 1, denies 2, gist 3): without
                // it a scene asserts in its first turn, denies in its second and makes its gist in its last.
                auto ws = words(rest);
                int turn = e->scene ? (head == "asserts" ? 0 : head == "denies" ? 1 : -2) : 0;
                if (!ws.empty() && ws[0].find_first_not_of("0123456789") == std::string::npos)
                {
                    if (!e->scene)
                        fail(head + ": a rule is one turn; only a scene's names one");
                    turn = std::atoi(ws[0].c_str()) - 1;
                    if (turn < 0)
                        fail(head + ": turns count from 1");
                    ws.erase(ws.begin());
                }
                const auto claim = ws.empty() ? std::string() : ws[0];
                if (head == "asserts" && claim == "none" && ws.size() == 1)
                {
                    // Phase 8 (§33.2): a line that answers without passing anything on ("Now there's a question."), on a
                    // move that would assert the topic's claim by default (answer.detail).
                    e->asserts = Entry::NeverAsserts;
                    e->assertsClaim.clear();
                    continue;
                }
                if (ws.size() > 1 || (head == "gist" && !claim.empty()))
                    fail(head == "gist" ? "gist is 'gist' (in a scene, 'gist <turn>')" : head + " is '" + head + " [turn] [claim kind]'");
                else if (!claim.empty() && claims::loaded() && !claims::find(claim))
                    fail("'" + claim + "' is not a claim kind (claims.json)");
                (head == "asserts" ? e->asserts : head == "denies" ? e->denies : e->gist) = turn;
                (head == "asserts" ? e->assertsClaim : e->deniesClaim) = head == "gist" ? std::string() : claim;
            }
            else    // binds x=<slot>
            {
                const auto eq = rest.find('=');
                const auto name = eq == std::string::npos ? std::string() : trim(rest.substr(0, eq));
                const auto slot = eq == std::string::npos ? std::string() : trim(rest.substr(eq + 1));
                if (name != "x" || slot.empty())
                    fail("binds is 'binds x=<slot>' (self.spouse, other.child, topic.subject...)");
                else if (!bindSlots().count(slot))
                    fail("'" + slot + "' is not a slot binds may name (self. or other. spouse, child, mother, father, "
                         "neighbour, shopkeeper, priest, leader; topic.subject)");
                else if (e->binds.count(name))
                    fail("x is bound once");
                else
                    e->binds[name] = slot;
            }
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
                e->turns.push_back({0, false, {Variant{{}, {}, {}, {}, {}, number}}});
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
    // Doc 84 §7.1-7.2 (Phase 2): each entry's moves settled once its turns are known.
    const auto settle = [&](Entry& x, const std::string& what, std::vector<std::string>& errs) {
        const auto turns = x.turns.size();
        if (turns == 0)
            return;
        if (!x.does.empty() && x.does.size() != turns)
            errs.push_back(what + ": does names " + std::to_string(x.does.size()) + " turns' moves; it has " +
                           std::to_string(turns) + " (a / between turns, - for one not yet known)");
        if (!x.tones.empty() && x.tones.size() != turns && x.tones.size() != 1)
            errs.push_back(what + ": tone names " + std::to_string(x.tones.size()) + " turns' tones; it has " + std::to_string(turns));
        // A rule with no `does` makes its answer concept's move (an alias in moves.json).
        if (x.does.empty() && !x.scene && !moves_.empty())
        {
            std::vector<std::string> aliased;
            for (const auto& c : x.concepts)
                if (const auto a = moves_.alias(c); !a.empty() && std::find(aliased.begin(), aliased.end(), a) == aliased.end())
                    aliased.push_back(a);
            if (!aliased.empty())
            {
                x.does.push_back(aliased);
                x.aliasMoves = true;
            }
        }
        x.does.resize(turns);
        if (x.tones.size() == 1 && turns > 1)
            x.tones.assign(turns, x.tones.front());
        x.tones.resize(turns);
        if (x.gist == -2)
            x.gist = int(turns) - 1;
        for (const auto* t : {&x.asserts, &x.denies, &x.gist})
            if (*t >= int(turns))
                errs.push_back(what + ": names turn " + std::to_string(*t + 1) + " of " + std::to_string(turns));
        // What is owed must answer what the line does: an owed move that can't is a slip.
        const auto& last = x.does.back();
        for (const auto& o : x.owes)
        {
            if (last.empty())
            {
                errs.push_back(what + ": " + (o.soft ? "invites" : "owes") + " needs the line's own move first (does)");
                continue;
            }
            for (const auto& m : o.moves)
                if (o.who != "self" && std::none_of(last.begin(), last.end(), [&](const std::string& d) { return moves_.answers(m, d); }))
                    errs.push_back(what + ": owes " + m + ", which doesn't answer " + last.front() + " (moves.json: its answers)");
        }
        // A `then` hands on its concept's move: the answer it makes due. (A hand-on to a topic is a new topic, owed by
        // nobody: no alias.)
        if (!x.thenConcept.empty() && !moves_.empty())
        {
            if (const auto a = moves_.alias(x.thenConcept); a.empty())
            {
                if (x.thenConcept.rfind("talk.", 0) != 0)
                    errs.push_back(what + ": hands on to " + x.thenConcept + ", an answer concept with no move (moves.json: an alias)");
            }
            else if (std::none_of(x.owes.begin(), x.owes.end(), [](const Entry::Owed& o) { return !o.soft; }))
            {
                Entry::Owed o;
                o.who = x.thenWho;
                o.moves = {a};
                o.derived = true;
                x.owes.push_back(o);
            }
        }
        // A line that speaks of x needs someone bound as x.
        bool usesX = std::any_of(x.when.begin(), x.when.end(), [](const Criterion& c) { return c.ns == Ns::X; });
        for (const auto& t : x.turns)
            for (const auto& v : t.variants)
            {
                usesX = usesX || std::any_of(v.when.begin(), v.when.end(), [](const Criterion& c) { return c.ns == Ns::X; });
                for (const auto& text : v.texts)
                    usesX = usesX || text.find("#x.") != std::string::npos;
            }
        if (usesX && !x.binds.count("x"))
            errs.push_back(what + ": speaks of x with nobody bound (binds x=<slot>)");
        if (std::any_of(x.remember.begin(), x.remember.end(), [](const Entry::Remember& r) { return r.scope == "x"; }) && !x.binds.count("x"))
            errs.push_back(what + ": remembers of x with nobody bound (binds x=<slot>)");
        if ((!x.invitePlace.empty() || !x.promiseWhat.empty()) && x.scene)
            errs.push_back(what + ": an invitation or a promise is a rule's (one speaker's)");
        // A callback: its criteria read the talk ledger (doc 84 §8.7, the scorecard's C9).
        const auto ledgerOne = [](const Criterion& c) { return Library::ledgerFact(c.full.empty() ? "self." + c.name : c.full) ||
                                                               (c.ns == Ns::X && c.name.rfind("said.", 0) == 0); };
        x.ledger = std::any_of(x.when.begin(), x.when.end(), ledgerOne);
        for (const auto& t : x.turns)
            for (const auto& v : t.variants)
                x.ledger = x.ledger || std::any_of(v.when.begin(), v.when.end(), ledgerOne);
    };
    for (auto& x : parsed)
    {
        const std::string what = x.file + ": " + (x.scene ? "scene " : "rule ") + x.id;
        // A rule with no concept says what it does (doc 84 §7.1, Phase 3): a move line, found by its move alone.
        if (x.concepts.empty() && (x.scene || x.does.empty()))
            errors.push_back(what + " has no concept (when concept=... or topic=...), nor a move (does ...)");
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
        settle(x, what, errors);
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
    if (parseInto_)
    {
        // (A mantle's items, or one wolf's own lines: kept by their owner, never the shared library's. Doc 84 Phase 9.)
        for (auto& x : parsed)
            parseInto_->push_back(std::move(x));
        return true;
    }
    groups_ = groups;
    for (auto& x : parsed)
    {
        byId_[x.id] = entries_.size();
        for (const auto& c : x.concepts)
            byConcept_[c].push_back(entries_.size());
        entries_.push_back(std::move(x));
        indexMoves(entries_.size() - 1);
    }
    return true;
}

namespace
{
bool loadDir(Library& lib, const std::string& dir, bool all, std::string& problem,
             const std::function<bool(const std::string&, std::string&)>& schema,
             const std::function<bool(const std::string&, std::string&)>& settings,
             const std::function<bool(const std::string&, std::string&)>& moves,
             const std::function<bool(const std::string&, std::string&)>& frames,
             const std::function<bool(const std::string&, std::string&)>& opinions,
             const std::function<bool(const std::string&, std::string&)>& temperaments)
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
    // The claims and their words (doc 84 §7.7): the process's table, for the world's talk of what residents believe. A
    // library from before Phase 1 has none, and leaves the table as it was.
    if (readFile(fs::path(dir) / "claims.json", text) && !claims::load(text, problem))
        return false;
    // The moves lines make (doc 84 §7.2, Phase 2). A library from before Phase 2 has none: its lines make no moves.
    if (readFile(fs::path(dir) / "moves.json", text) && !moves(text, problem))
        return false;
    // The frames of a meeting and a talk's budget (doc 84 §7.4, Phase 3). A library from before Phase 3 has none: the
    // budgets' defaults, no frames.
    if (readFile(fs::path(dir) / "frames.json", text) && !frames(text, problem))
        return false;
    // Opinions, likes and how a trait leans (doc 84 §7.5, §7.6, Phase 6): facts of their own (op.<issue>, likes...), read
    // before the lines that test them. A library from before Phase 6 has neither.
    if (readFile(fs::path(dir) / "opinions.json", text) && !opinions(text, problem))
        return false;
    if (readFile(fs::path(dir) / "temperaments.json", text) && !temperaments(text, problem))
        return false;
    // The topic director's numbers (doc 84 §7.3, Phase 5): the process's book. A library from before Phase 5 has none:
    // today's, the C++ scores as they were.
    if (readFile(fs::path(dir) / "topics.json", text))
    {
        if (!topics::load(text, problem))
            return false;
    }
    else
        topics::reset();
    std::vector<fs::path> grammars, talks, trees;
    for (fs::recursive_directory_iterator it(dir, ec), end; it != end && !ec; it.increment(ec))
        if (it->is_regular_file())
            (it->path().extension() == ".grammar" ? grammars : it->path().extension() == ".tree" ? trees : talks).push_back(it->path());
    talks.erase(std::remove_if(talks.begin(), talks.end(), [](const fs::path& p) { return p.extension() != ".talk"; }), talks.end());
    std::sort(trees.begin(), trees.end());
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
    // The dialogue trees (doc 83) after the groups and before the rest, so any rule may speak a tree's concepts.
    bool treesRead = false;
    const auto readTrees = [&] {
        if (treesRead || !all)
            return;
        treesRead = true;
        for (const auto& f : trees)
        {
            std::string one;
            if (readFile(f, text) && !lib.parseTree(text, fs::relative(f, dir, ec).generic_string(), one))
                add(one);
        }
    };
    for (const auto& f : talks)
    {
        if (f.filename() != "groups.talk")
            readTrees();
        if (!all && f.filename() != "groups.talk")
            continue;
        std::string one;
        if (readFile(f, text) && !lib.parse(text, fs::relative(f, dir, ec).generic_string(), one))
            add(one);
    }
    readTrees();
    // A tree's `do open <entry>` goes on in the tree that answers that entry: some tree must.
    for (const auto& [id, t] : lib.trees())
        for (const auto& [nodeId, n] : t.nodes)
            for (const auto& c : n.choices)
                if (c.action == "open" && !c.args.empty() &&
                    std::none_of(lib.trees().begin(), lib.trees().end(), [&](const auto& o) { return o.second.entries.count(c.args[0]) > 0; }))
                    add(t.file + ":" + std::to_string(c.line) + ": no tree has the entry '" + c.args[0] + "'");
    // No dead ends (doc 84 §7.2): whatever a line hands on to or owes can always be answered.
    if (all && trouble.empty())
        if (const auto dead = lib.deadEnds(); !dead.empty())
            add(dead);
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
                            [this](const std::string& t, std::string& p) { return readSettings(t, p); },
                            [this](const std::string& t, std::string& p) { return readMoves(t, p); },
                            [this](const std::string& t, std::string& p) { return readFrames(t, p); },
                            [this](const std::string& t, std::string& p) { return readOpinions(t, p); },
                            [this](const std::string& t, std::string& p) { return readTemperaments(t, p); });
    if (ok && !readSteps(problem))
        return clear(), false;
    if (!ok)
        clear();
    return ok;
}

bool Library::loadFoundation(const std::string& dir, std::string& problem)
{
    clear();
    const bool ok = loadDir(*this, dir, false, problem, [this](const std::string& t, std::string& p) { return readSchema(t, p); },
                            [this](const std::string& t, std::string& p) { return readSettings(t, p); },
                            [this](const std::string& t, std::string& p) { return readMoves(t, p); },
                            [this](const std::string& t, std::string& p) { return readFrames(t, p); },
                            [this](const std::string& t, std::string& p) { return readOpinions(t, p); },
                            [this](const std::string& t, std::string& p) { return readTemperaments(t, p); });
    if (ok && !readSteps(problem))
        return clear(), false;
    if (!ok)
        clear();
    return ok;
}

// A greeting or goodbye that names the hour, said at another (Slice 3: "Morning to you" at eight in the evening, from a
// resident's own words, a personal layer that tests no hour): its words are passed over where they don't fit the moment,
// as truth.json's morning_greeting, day_greeting and night_greeting judge them.
static bool wrongHour(const std::string& text, const Query& q)
{
    const auto f = [&](const char* k) {
        const auto it = q.facts.find(k);
        return it == q.facts.end() ? std::string() : it->second;
    };
    const auto time = f("here.time");
    if (time.empty())
        return false;
    std::string t;
    for (const char ch : text)
        t += char(std::tolower(static_cast<unsigned char>(ch)));
    const auto at = [&](const std::string& w) {     // At the start of the text or of a sentence in it.
        for (std::size_t p = t.find(w); p != std::string::npos; p = t.find(w, p + 1))
        {
            std::size_t b = p;
            while (b > 0 && t[b - 1] == ' ')
                --b;
            if (b == 0 || t[b - 1] == '.' || t[b - 1] == '!' || t[b - 1] == '?' || t[b - 1] == ';')
                return true;
        }
        return false;
    };
    const bool night = time == "dusk" || time == "night";
    int hour = 12;
    try
    {
        hour = std::stoi(f("here.hour"));
    }
    catch (...)
    {
    }
    const bool morning = time == "dawn" || (time == "day" && hour < 12);
    if (!morning && (at("good morning") || at("morning to you") || at("morning,") || at("morning.") || at("morning!") ||
                     t == "morning"))
        return true;
    if (!night && at("good night"))
        return true;
    if (night && at("good day") && !at("good day today") && !at("good day for"))
        return true;
    if (!night && (at("good evening") || at("evening to you") || at("evening,") || at("evening.")))
        return true;
    return false;
}

namespace
{
// Lower-case words (letters, apostrophes, hyphens), one space apart: how a phrase is matched in a text.
std::string phraseWords(const std::string& t)
{
    std::string out;
    bool space = true;
    for (const char ch : t)
    {
        const auto c = static_cast<unsigned char>(ch);
        if (std::isalpha(c) || ch == '\'' || ch == '-')
            out += static_cast<char>(std::tolower(c)), space = false;
        else if (!space)
            out += ' ', space = true;
    }
    while (!out.empty() && out.back() == ' ')
        out.pop_back();
    return out;
}
}

const std::vector<std::string>& Library::dialectWords(const std::string& dialect) const
{
    // (Per thread and per library, built again when the grammars change: a dialect saved in Atlas.)
    thread_local const Library* lib = nullptr;
    thread_local std::size_t sig = 0;
    thread_local std::map<std::string, std::vector<std::string>> cache;
    std::size_t now = grammars_.size();
    for (const auto& [n, g] : grammars_)
        now = now * 31 + g.size() + n.size();
    if (lib != this || sig != now)
        cache.clear(), lib = this, sig = now;
    if (const auto c = cache.find(dialect); c != cache.end())
        return c->second;
    std::map<std::string, std::set<std::string>> said;      // phrase -> the grammars that say it
    for (const auto& [gname, g] : grammars_)
        for (const auto& [symbol, s] : g)
            for (const auto& e : s.entries)
                for (const auto& a : e.alts)
                    if (a.text.find('#') == std::string::npos)
                        if (const auto p = phraseWords(a.text); p.size() >= 3)
                            said[p].insert(gname);
    auto& out = cache[dialect];
    for (const auto& [p, gs] : said)
        if (gs.size() == 1 && *gs.begin() == dialect && (p.find(' ') != std::string::npos || p.find('-') != std::string::npos))
            out.push_back(p);
    return out;
}

bool Library::saysDialect(const std::string& dialect, const std::string& text) const
{
    const auto words = " " + phraseWords(text) + " ";
    for (const auto& p : dialectWords(dialect))
        if (words.find(" " + p + " ") != std::string::npos)
            return true;
    return false;
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
    // (plain's words), light (the dialect's word half the time), broad (always, and its .broad words too). Phase 6
    // (§10.6): the accent they speak in toward the one spoken to (`speaks`, accommodation) before their own.
    const auto dialect = dialectOf(fact(who, "from"));
    const auto accent = !fact(who, "speaks").empty() ? fact(who, "speaks") : fact(who, "accent");
    std::vector<const Alt*> alts;
    std::vector<const std::string*> altLayer;       // (The layer each came from, for LineInfo: never for choosing.)
    bool habit = false, pairHabit = false;
    std::string from, habitLayer;
    auto layers = who.grammar;
    layers.push_back("plain");
    for (const auto& layer : layers)
    {
        const Grammar* g = layerOf(layer);
        if (!g)
            continue;
        const bool spoken = layer == dialect;
        if (spoken && (accent == "none" || (accent == "light" && x.next() % 2)))
            continue;
        // (A mantle's words are its wearer's whatever their accent; its .broad words for a broad one: doc 84 Phase 9.)
        const bool worn = layer.rfind("m_", 0) == 0;
        // (Doc 84 §12.3.5, Phase 11: a mantle lent from a neighbouring pool, worn by a wolf of another dialect, keeps its
        // own words but those that are its dialect's: a Saltwell pup in a Southmeadow mantle doesn't say the downs' words.)
        std::string lent;
        if (worn)
            if (const auto o = owned_.find("m:" + layer.substr(2)); o != owned_.end() && o->second->card &&
                o->second->card->dialect != "plain" && o->second->card->dialect != dialect)
                lent = o->second->card->dialect;
        std::vector<const Symbol*> symbols;
        if (const auto s = g->find(name); s != g->end())
            symbols.push_back(&s->second);
        if ((spoken || worn) && accent == "broad")
            if (const auto s = g->find(name + ".broad"); s != g->end())
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
        std::size_t added = 0;
        for (const auto* s : symbols)
            for (const auto& e : s->entries)
                if (e.score == best && holdsAll(e))
                {
                    for (const auto& a : e.alts)
                        if (!wrongHour(a.text, x.q) && (lent.empty() || !saysDialect(lent, a.text)))
                            alts.push_back(&a), altLayer.push_back(&layer), ++added;
                    adds = adds && e.add;
                }
        if (added == 0)
            continue;                               // (Only words for another hour here: the layer below has the hour's.)
        from += (from.empty() ? "" : "+") + layer;
        if (worn && habitLayer.empty())
            habitLayer = layer;
        if (!adds)
            break;
    }
    // ^m and ^f: for one sex of the one spoken to (or of the speaker, speaking to no one).
    const auto sex = fact(to, "sex");
    {
        std::size_t kept = 0;
        for (std::size_t i = 0; i < alts.size(); ++i)
            if (!(alts[i]->sex && !((alts[i]->sex == 'm' && sex == "male") || (alts[i]->sex == 'f' && sex == "female"))))
                alts[kept] = alts[i], altLayer[kept++] = altLayer[i];
        alts.resize(kept);
        altLayer.resize(kept);
    }
    if (alts.empty())
        return false;
    const auto id = !fact(who, "id").empty() ? fact(who, "id") : fact(who, "name");
    const auto toId = &to == &who ? std::string() : !fact(to, "id").empty() ? fact(to, "id") : fact(to, "name");
    // A habit defined in a mantle is seeded by the mantle, not its wearer (doc 84 §11): its oath survives into its next
    // wearer, unless their own layer (re-drawn marks, §12.4) says otherwise.
    const auto& habitOf = habitLayer.empty() || *altLayer.front() != habitLayer ? id : habitLayer;
    const auto seed = pairHabit ? mix(hash(habitOf + "|" + toId + "|" + name)) : habit ? mix(hash(habitOf + "|" + name)) : x.next();
    const auto chosenAt = std::size_t(seed % alts.size());
    const auto* alt = alts[chosenAt];
    if (x.layers && std::find(x.layers->begin(), x.layers->end(), *altLayer[chosenAt]) == x.layers->end())
        x.layers->push_back(*altLayer[chosenAt]);
    if (x.habits && (habit || pairHabit) && std::find(x.habits->begin(), x.habits->end(), name) == x.habits->end())
        x.habits->push_back(name);
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
        if (ref.parts.size() == 2 && (ref.parts[0] == "x" || ref.parts[0] == "conv"))
        {
            // A bound third party's (x.name, x.they) or the conversation's own (conv.k): kept in the query by full key.
            const auto at = [&](const std::string& k) {
                const auto f = x.q.facts.find(ref.parts[0] + "." + k);
                return f == x.q.facts.end() ? nullptr : &f->second;
            };
            if (ref.parts[0] == "x" && Pronouns.count(ref.parts[1]))
            {
                if (!at("sex") && !at("name"))
                    return false;
                value = pronoun(ref.parts[1], at("sex") ? *at("sex") : std::string());
            }
            else if (const auto* v = at(ref.parts[1]))
                value = *v;
        }
        else if (ref.parts.size() == 2 && isPersonNs(ref.parts[0]))
        {
            const Ns ns = personNs(ref.parts[0]);
            if (const auto role = rolePronoun(ref.parts[1]); !role.empty())
            {
                // One of their people (#self.spouse_their#, #other.child_they#: Phase 6), by that one's sex: never "they"
                // for one known wolf. Not without them.
                const std::string* who = fact(ns, role, {}, x.q, x.speaker);
                const std::string* sex = fact(ns, role + "_sex", {}, x.q, x.speaker);
                if (!who || who->empty() || !sex)
                    return false;
                value = pronoun(ref.parts[1].substr(role.size() + 1), *sex);
            }
            else if (Pronouns.count(ref.parts[1]))
            {
                const std::string* sex = fact(ns, "sex", {}, x.q, x.speaker);
                const std::string* any = fact(ns, "name", {}, x.q, x.speaker);
                if (!any && !sex)
                    return false;
                value = pronoun(ref.parts[1], sex ? *sex : std::string());
            }
            else if (const auto* v = fact(ns, ref.parts[1], {}, x.q, x.speaker))
            {
                value = *v;
                // (Doc 84 Phase 11: one's people by name, never to that very one's face. "#self.boss# counts every cup"
                // said to the boss, "I can trust #self.hand#" to the hand: the line isn't said to them. The pilot's
                // mantles brought it out, T3's listener_third.)
                static const std::set<std::string> People{"spouse", "child", "mother", "father", "neighbour", "friend",
                                                          "rival", "boss", "hand", "priest", "shopkeeper", "leader"};
                if (People.count(ref.parts[1]) && !x.q.alone)
                {
                    const int whose = ns == Ns::A ? 0 : ns == Ns::B ? 1 : ns == Ns::Self ? x.speaker : 1 - x.speaker;
                    const auto& them = whose == 0 ? x.q.b : x.q.a;
                    const auto name = them.facts.find("name");
                    if (name != them.facts.end() && !name->second.empty() && name->second == value)
                        return false;
                }
            }
            else if (ref.parts[1] == "called" && !expandSymbol("address", x, depth + 1, value))
                return false;                       // (What one calls a stranger: their #address#. Doc 84 §11.)
        }
        else if (ref.parts.size() == 2 && ref.parts[0] == "topic" && topicPronoun(ref.parts[1]))
        {
            // The topic's subject or victim as a pronoun, by their sex (doc 84, Phase 6: never "they" for one wolf):
            // #topic.they#, #topic.their#...; #topic.victim_they#... Not without the one it stands for.
            const bool victim = ref.parts[1].rfind("victim_", 0) == 0;
            const auto who = x.q.facts.find(victim ? "topic.victim" : "topic.subject");
            const auto sex = x.q.facts.find(victim ? "topic.victim_sex" : "topic.subject_sex");
            if (who == x.q.facts.end() || who->second.empty())
                return false;
            value = pronoun(victim ? ref.parts[1].substr(7) : ref.parts[1], sex == x.q.facts.end() ? std::string() : sex->second);
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
        // The punctuation join (doc 84, defect 14): a value that ends its own sentence (". " or ";") before the line's
        // comma or stop gives up its mark: "the loaves are honest., neighbour" is "the loaves are honest, neighbour".
        if (i + 1 < text.size() && (text[i + 1] == ',' || text[i + 1] == '.') && !value.empty() &&
            (value.back() == ';' || (value.back() == '.' && (value.size() < 2 || value[value.size() - 2] != '.'))))
            value.pop_back();
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

bool Library::bindThird(const Entry& e, const Query& q, int speaker, Query& out) const
{
    // A third party (doc 84 §7.1, binds x=<slot>): the game says who the slot names for the speaker, and their facts go
    // in as x.*. Nobody there, or nobody to ask: the line doesn't fit.
    out = q;
    for (auto it = out.facts.begin(); it != out.facts.end();)
        it = it->first.rfind("x.", 0) == 0 ? out.facts.erase(it) : std::next(it);
    for (const auto& [name, slot] : e.binds)
    {
        Facts f;
        if (!q.bind || !q.bind(slot, speaker, f) || f.empty())
            return false;
        for (auto& [k, v] : f)
            out.facts[name + "." + k] = std::move(v);
    }
    return true;
}

bool Library::fits(const Entry& e, const Query& q, int speaker) const
{
    if (!e.concepts.empty() && !e.concepts.count(q.concept))   // (A move line, with no concept, is any concept's.)
        return false;
    if (!e.binds.empty())
    {
        Query b;
        return bindThird(e, q, speaker, b) && fitsWith(e, b, speaker);
    }
    return fitsWith(e, q, speaker);
}

bool Library::fitsWith(const Entry& e, const Query& q, int speaker) const
{
    if (!e.concepts.empty() && !e.concepts.count(q.concept))
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

std::string Library::redrawHabit(const std::string& mantle, const std::string& symbol, const std::string& wearer,
                                 const std::string& bank) const
{
    const auto alternatives = [&](const Grammar* g, const std::string& name) {
        std::vector<std::string> out;
        if (const auto s = g ? g->find(name) : Grammar::const_iterator(); g && s != g->end())
            for (const auto& e : s->second.entries)
                for (const auto& a : e.alts)
                    if (!a.sex && std::find(out.begin(), out.end(), a.text) == out.end())
                        out.push_back(a.text);
        return out;
    };
    if (const auto banked = alternatives(layerOf(bank.empty() ? std::string("plain") : bank), "mark." + symbol); !banked.empty())
        return banked[mix(hash(wearer + "|mark|" + symbol)) % banked.size()];
    const auto layer = mantleLayer(mantle);
    const auto own = alternatives(layerOf(layer), symbol);
    if (own.size() < 2)
        return {};
    const auto was = mix(hash(layer + "|" + symbol)) % own.size();   // (As a habit of the mantle's is seeded.)
    return own[(was + 1 + mix(hash(wearer + "|" + symbol)) % (own.size() - 1)) % own.size()];
}

std::string Library::dialectOf(const std::string& from) const
{
    const auto mapped = settings_.dialectOf.find(from);
    return mapped == settings_.dialectOf.end() ? from : mapped->second;
}

Said Library::render(const Entry& e, const Query& q, std::uint64_t seed, Trace* trace, int speaker) const
{
    if (!e.binds.empty())
    {
        Query b;
        return bindThird(e, q, speaker, b) ? renderWith(e, b, seed, trace, speaker) : Said{};
    }
    return renderWith(e, q, seed, trace, speaker);
}

Said Library::renderWith(const Entry& e, const Query& q, std::uint64_t seed, Trace* trace, int speaker) const
{
    Said out;
    if (!fitsWith(e, q, speaker))
        return out;
    std::vector<std::string> before;                // (The moves of the line spoken before: what a line answers.)
    std::map<std::string, std::string> binds;
    std::set<std::string> marks;
    std::uint64_t r = mix(seed ^ hash(e.id));
    // Alternatives are written in parallel: a line with as many alternatives as the line spoken before it answers the
    // same one (the second reply to the second remark), even past a single-line answer in between. Otherwise one at
    // random.
    std::size_t threadIndex = 0, threadCount = 0;
    bool kindLine = false;                  // (§37: the turn before was a speaker's own line, by their kind: b[old] ...)
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
        // §37, the hybrid: a parallel line answers the line of its own thread, so after a speaker's own line chosen by
        // their kind (one alternative, no mark: "When I was young we hanged thieves at the gate.") the next parallel line
        // would answer what was said before it ("Right on their own doorstep."). The scene ends on the kind's line.
        if (settings_.hybrid && e.scene && kindLine && threadCount > 1 && chosen->needs.empty() && chosen->texts.size() == threadCount &&
            out.lines.size() >= 2)
        {
            out.info.back().owes.clear();           // (Its end, as a scene's last line: it owes only what is written.)
            out.info.back().invites.clear();
            break;
        }
        kindLine = chosen->texts.size() == 1 && !chosen->when.empty() && chosen->marks.empty();
        // One alternative at random; another if a fact it needs is missing.
        r = mix(r + t);
        const std::size_t n = chosen->texts.size(), start = n == threadCount ? threadIndex : std::size_t(r % n);
        std::string said;
        bool done = false;
        std::size_t k = 0;
        std::vector<std::string> used, habits;
        for (; k < n && !done; ++k)
        {
            auto kept = binds;
            used.clear();
            habits.clear();
            Expansion x{q, who, mix(r ^ 0x7A1CULL ^ k), binds, trace, &used, &habits};
            done = expandInto(chosen->texts[(start + k) % n], x, 0, said) && !muted(said, q, who);   // (Salience: 10.3.)
            // Never the same question twice in one exchange (C6): an echo in a dialect whose backchannels are tag
            // questions ("I know, no?" / "I know, no?", Mirebridge's) is said again with other words where it has them.
            // Slice 2's review.
            const auto askedBefore = [&](const std::string& text) {
                auto tidied = text;
                tidy(tidied);
                return !tidied.empty() && tidied.back() == '?' &&
                       std::any_of(out.lines.begin(), out.lines.end(), [&](const auto& l) { return l.second == tidied; });
            };
            for (std::uint64_t again = 1; done && again < 16 && askedBefore(said); ++again)
            {
                auto keptAgain = binds;
                std::vector<std::string> usedAgain, habitsAgain;
                Expansion y{q, who, mix(r ^ 0x7A1CULL ^ k ^ (again * 0x9E3779B97F4A7C15ULL)), binds, trace, &usedAgain, &habitsAgain};
                std::string other;
                if (expandInto(chosen->texts[(start + k) % n], y, 0, other) && !muted(other, q, who) && !askedBefore(other))
                {
                    said = std::move(other);
                    used = std::move(usedAgain);
                    habits = std::move(habitsAgain);
                    break;
                }
                binds = std::move(keptAgain);
            }
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
        SaidLine info;
        info.entry = e.id;
        info.alt = int((start + k - 1) % n);
        info.layers = std::move(used);
        info.habits = std::move(habits);
        info.who = who;
        info.to = q.alone ? -1 : 1 - who;
        info.text = said;
        info.turn = int(t);
        info.variant = int(chosen - turn.variants.data());
        {
            const Speaker& by = who == 0 || q.alone ? q.a : q.b;
            const auto speaks = by.facts.find("speaks");
            const auto accent = by.facts.find("accent");
            info.accent = speaks != by.facts.end() ? speaks->second : accent != by.facts.end() ? accent->second : std::string();
        }
        if (std::size_t(info.alt) < chosen->tags.size())
        {
            info.tags = chosen->tags[std::size_t(info.alt)].set;
            info.keep = chosen->tags[std::size_t(info.alt)].keep;
        }
        describe(e, t, q, before, info);
        before = info.moves;
        out.info.push_back(std::move(info));
        marks.insert(chosen->marks.begin(), chosen->marks.end());
    }
    out.id = e.id;
    out.repeat = settings_.repeat.count(q.concept) > 0;
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
        if (m.scope == "x")
        {
            // What talk says of the third party bound (doc 84 §8.7): the ledger's person facts, "subject|by".
            if (const auto x = q.facts.find("x.id"); x != q.facts.end() && !x->second.empty())
                add("said", x->second + "|" + self, m.key, m.value, m.days);
        }
        else
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

std::vector<std::string> Library::answersOf(const std::vector<std::string>& before, const std::vector<std::string>& moves) const
{
    std::vector<std::string> out;
    for (const auto& b : before)
        if (std::any_of(moves.begin(), moves.end(), [&](const std::string& m) { return moves_.answers(m, b); }))
            out.push_back(b);
    return out;
}

void Library::describe(const Entry& e, std::size_t turn, const Query& q, const std::vector<std::string>& before, SaidLine& line) const
{
    if (moves_.empty() || turn >= e.turns.size())
        return;
    line.moves = turn < e.does.size() ? e.does[turn] : std::vector<std::string>{};
    if (turn < e.tones.size())
        line.tone = e.tones[turn];
    // What the last turn makes due as written (or its `then`'s move); the rest from its moves (describeMoves).
    if (turn + 1 == e.turns.size())
        for (const auto& o : e.owes)
        {
            std::string alts;
            for (const auto& m : o.moves)
                alts += (alts.empty() ? "" : "|") + m;
            (o.soft ? line.invites : line.owes).push_back((o.who == "other" ? std::string() : o.who + ":") + alts);
        }
    // The claim it names, else the topic's (topic.claim_kind), else "topic".
    const auto claim = [&](const std::string& named) {
        const auto kind = q.facts.find("topic.claim_kind");
        return !named.empty() ? named : kind != q.facts.end() ? kind->second : std::string("topic");
    };
    if (e.asserts == int(turn))
        line.asserts.push_back(claim(e.assertsClaim));
    if (e.denies == int(turn))
        line.denies.push_back(claim(e.deniesClaim));
    line.gist = e.gist == int(turn);
    // Phase 5: an invitation or a promise the line makes; a callback (its criteria read the talk ledger).
    line.callback = e.ledger;
    if (turn + 1 == e.turns.size())
    {
        line.invitePlace = e.invitePlace;
        line.inviteHour = e.inviteHour;
        line.inviteDays = e.inviteDays;
        line.promiseWhat = e.promiseWhat;
        line.promiseDays = e.promiseDays;
    }
    // A scene is written whole: its last line owes only what is written (a set piece handing on into open talk, §8.2),
    // never what its moves expect by default.
    // §37, the hybrid: and a scene's turns answer each other as written, so none owes by default (its next turn is the
    // answer, whatever its move is called); a scene ended early on a speaker's own line ends there owing nothing.
    describeMoves(line, before, q.facts, !(e.scene && (turn + 1 == e.turns.size() || settings_.hybrid)));
    if (e.asserts == Entry::NeverAsserts)
        line.asserts.clear();                       // (`asserts none`: not even by its move's default.)
    // Phase 6 (§8.8): a line airing a stance on the issue a talk is on records the speaker's own side, and the steps events
    // had moved it (the scorecard's T4).
    if (const auto issue = q.facts.find("conv.issue"); issue != q.facts.end())
    {
        static const char* const Stance[] = {"give.opinion", "agree", "disagree", "insist", "concede", "counter", "join.side", "join.correct"};
        if (std::any_of(std::begin(Stance), std::end(Stance), [&](const char* m) { return hasMove(moves_, line.moves, m); }))
        {
            const Speaker& by = line.who == 0 || q.alone ? q.a : q.b;
            const auto at = [&](const std::string& k) {
                const auto f = by.facts.find(k + "." + issue->second);
                return f == by.facts.end() ? std::string() : f->second;
            };
            line.issue = issue->second;
            line.stance = at("op").empty() ? "mixed" : at("op");
            line.drift = std::atoi(at("op_drift").c_str());
            line.basis = at("op_basis");
        }
    }
}

void Library::describeMoves(SaidLine& line, const std::vector<std::string>& before, const Facts& facts, bool expect) const
{
    if (moves_.empty())
        return;
    if (line.tone.empty() && !line.moves.empty())
        line.tone = moves_.toneOf(line.moves.front());
    // What its moves expect, when nothing was written or handed on (soft for a move whose answer is optional): its last
    // move's, the part of the turn that makes the next one due (doc 84 §8.3: "Well. Fair winds." owes a farewell, not
    // an answer to the preclose too).
    if (expect && line.owes.empty() && line.invites.empty() && !line.moves.empty())
        for (const auto& m : {line.moves.back()})
            if (const auto* mv = moves_.find(m); mv && !mv->expects.empty())
            {
                std::string alts;
                for (const auto& x : mv->expects)
                    alts += (alts.empty() ? "" : "|") + x;
                (mv->optional ? line.invites : line.owes).push_back(alts);
            }
    line.answers = answersOf(before, line.moves);
    // A move that asserts (or denies) by default does so only on a topic with a claim.
    const auto kind = facts.find("topic.claim_kind");
    const auto byDefault = [&](bool Move::*flag) {
        return kind != facts.end() && std::any_of(line.moves.begin(), line.moves.end(), [&](const std::string& m) {
                   const auto* mv = moves_.find(m);
                   return mv && mv->*flag;
               });
    };
    if (line.asserts.empty() && byDefault(&Move::asserts))
        line.asserts.push_back(kind->second);
    if (line.denies.empty() && byDefault(&Move::denies))
        line.denies.push_back(kind->second);
}

bool Library::unconditioned(const Entry& e) const
{
    // Holds, and can be said, for a speaker with every fact empty: plain words, no limit, nobody bound.
    if (e.scene || !e.binds.empty() || e.limit != Entry::Limit::None || e.turns.size() != 1)
        return false;
    Query none;
    if (!std::all_of(e.when.begin(), e.when.end(), [&](const Criterion& c) { return holds(c, none, 0); }))
        return false;
    for (const auto& v : e.turns.front().variants)
    {
        if (!std::all_of(v.when.begin(), v.when.end(), [&](const Criterion& c) { return holds(c, none, 0); }) || !v.needs.empty())
            continue;
        for (const auto& text : v.texts)
        {
            std::string out;
            if (expand(text, none, 0, 1, out) && !out.empty())
                return true;
        }
    }
    return false;
}

std::string Library::deadEnds() const
{
    if (moves_.empty())
        return {};
    std::vector<char> plain(entries_.size(), 2);    // (2: not worked out yet.)
    const auto ok = [&](std::size_t i) {
        if (plain[i] == 2)
            plain[i] = unconditioned(entries_[i]) ? 1 : 0;
        return plain[i] == 1;
    };
    std::vector<std::string> problems;
    // 1. Every concept a line hands on to (`then`) has a plain rule of its own: the talk as it runs today asks for the
    // concept, so its own rules must never all fail (defect 13).
    std::map<std::string, std::string> handed;
    for (const auto& e : entries_)
        if (!e.thenConcept.empty())
            handed.emplace(e.thenConcept, e.id);
    for (const auto& [concept, by] : handed)
    {
        const auto list = byConcept_.find(concept);
        if (list == byConcept_.end() || std::none_of(list->second.begin(), list->second.end(), ok))
            problems.push_back("dead end: " + concept + " (handed on by " + by +
                               ") has no rule that holds and speaks with nothing known: a talk that reaches it ends there");
    }
    // 2. Every answer a line can owe (written, handed on, or what its moves expect) has a plain line doing it, or doing
    // one of its fallbacks, walked down to ack.
    std::map<std::string, std::string> owed;        // "a|b" -> the first entry owing it
    const auto owe = [&](const std::vector<std::string>& alts, const std::string& by) {
        std::string key;
        for (const auto& m : alts)
            key += (key.empty() ? "" : "|") + m;
        owed.emplace(key, by);
    };
    for (const auto& e : entries_)
    {
        for (const auto& o : e.owes)
            if (!o.soft)
                owe(o.moves, e.id);
        for (const auto& turn : e.does)
            for (const auto& m : turn)
                if (const auto* mv = moves_.find(m); mv && !mv->optional && !mv->expects.empty())
                    owe(mv->expects, e.id);
    }
    for (const auto& [key, by] : owed)
    {
        bool answered = false;
        for (const auto& alt : split(key, '|'))
            for (const auto& m : moves_.chain(alt))
                if (const auto* list = byMove(m); list && std::any_of(list->begin(), list->end(), ok))
                    answered = true;
        if (!answered)
            problems.push_back("dead end: " + key + " (owed by " + by +
                               ") has no plain line doing it, or any of its fallbacks, that holds with nothing known");
    }
    std::string out;
    for (std::size_t i = 0; i < problems.size() && i < 30; ++i)
        out += (i ? "\n" : "") + problems[i];
    if (problems.size() > 30)
        out += "\n(" + std::to_string(problems.size() - 30) + " more dead ends)";
    return out;
}

namespace
{
// A rule or scene that holds and could be said, and how it is weighed (Library::pickOne, Library::answer).
struct Option
{
    Said said;
    const Entry* entry = nullptr;
    bool heard = false;
    double weight = 1;
    int fit = 0;
};
struct Choice
{
    std::size_t chosen = 0, tier = 0, in = 0, fresh = 0;
    std::vector<int> scores;
    int top = 0;
    std::string why;
};

// The choice among what holds (doc 62 §2): what must be said if it can be (a strict priority), filler only when nothing
// else holds; tiers by score, best first, a tier chosen by settings.json's chances (only the best when `bestOnly`),
// then within it one the listeners haven't heard (else the nearest tier with one), by weight. `options` is pruned to
// what the priority leaves.
Choice chooseOption(std::vector<Option>& options, const Settings& settings, std::uint64_t seed, bool bestOnly)
{
    Choice out;
    int top = -1000;
    for (const auto& o : options)
        top = std::max(top, o.entry->priority);
    out.top = top;
    if (top >= settings.strict)
    {
        options.erase(std::remove_if(options.begin(), options.end(), [&](const Option& o) { return o.entry->priority < top; }), options.end());
        out.why = "priority " + std::to_string(top) + " must be said; ";
    }
    else if (top >= 0)
        options.erase(std::remove_if(options.begin(), options.end(), [](const Option& o) { return o.entry->priority < 0; }), options.end());
    else
        out.why = "only filler holds; ";
    auto& scores = out.scores;
    for (const auto& o : options)
        scores.push_back(o.entry->score);
    std::sort(scores.begin(), scores.end(), std::greater<int>());
    scores.erase(std::unique(scores.begin(), scores.end()), scores.end());
    std::size_t tier = 0;
    const bool tiers = scores.size() > 1 && top < settings.strict && !bestOnly;
    if (tiers)
    {
        const double lower = settings.lower;
        const double best = settings.best + (scores.size() == 2 ? lower : 0);
        const double roll = unit(mix(seed * 31 + 7));
        if (roll < best)
            tier = 0;
        else if (roll < best + settings.next || lower <= 0)
            tier = 1;
        else
            tier = 2 + std::min(scores.size() - 3, std::size_t((roll - best - settings.next) / lower * double(scores.size() - 2)));
    }
    std::vector<std::size_t> in, fresh;
    const auto gather = [&] {
        in.clear();
        fresh.clear();
        for (std::size_t i = 0; i < options.size(); ++i)
            if (options[i].entry->score == scores[tier])
            {
                in.push_back(i);
                if (!options[i].heard)
                    fresh.push_back(i);
            }
    };
    gather();
    // Everything in that tier heard already: the nearest tier with something not heard, rather than a repeat (doc 62
    // phase 7: no conversation heard twice in a hundred hours). Not where only the best will do.
    if (fresh.empty() && tiers)
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
            out.why += "all heard in tier " + std::to_string(chosenTier + 1) + "; ";
    }
    const auto& from = fresh.empty() ? in : fresh;
    double total = 0;
    for (const auto i : from)
        total += options[i].weight;
    double roll = unit(mix(seed * 131 + 17)) * total;
    std::size_t chosen = from.back();
    for (const auto i : from)
        if ((roll -= options[i].weight) <= 0)
        {
            chosen = i;
            break;
        }
    out.chosen = chosen;
    out.tier = tier;
    out.in = in.size();
    out.fresh = fresh.size();
    return out;
}

bool hasMove(const Moves& book, const std::vector<std::string>& moves, const std::string& kind)
{
    return std::any_of(moves.begin(), moves.end(), [&](const std::string& m) { return book.kindOf(m, kind); });
}

std::string joined(const std::vector<std::string>& xs, const char* by = "|")
{
    std::string out;
    for (const auto& x : xs)
        out += (out.empty() ? "" : by) + x;
    return out;
}

// The most lines a conversation has (doc 84 §8.4: 2-14; a library before moves.json: 8 beats).
constexpr int LegacyBeats = 8;
} // namespace

Said Library::pick(const Query& q, std::uint64_t seed, const std::function<bool(const std::string&)>& heard,
                   const std::deque<std::string>& recent, Trace* trace) const
{
    return pickOne(q, seed, heard, recent, 0, false, trace);
}

Said Library::whole(const Conversation& c)
{
    Said out;
    if (c.chain.empty())
        return out;
    out.id = c.chain.front();
    out.repeat = c.repeat;
    out.effects = c.effects;
    out.chain = c.chain;
    out.ended = c.ended;
    for (const auto& l : c.lines)
        out.lines.push_back({l.who, l.text});
    out.info = c.lines;
    return out;
}

Said Library::converse(const Query& q, std::uint64_t seed, const std::function<bool(const std::string&)>& heard,
                       const std::deque<std::string>& recent, Trace* trace) const
{
    // Over one query (barks, reactions, talk_check, the tests; the ambient exchanges until Phase 4 asks each beat its
    // own): begin, then next until it ends, each beat seeing what the ones before it remembered.
    Conversation c;
    c.seed = seed;
    const Freshness fresh{heard, recent};
    if (!begin(c, q, fresh, trace))
        return {};
    auto now = q;
    std::size_t applied = 0;
    const auto facts = [&](int, int) {
        for (; applied < c.effects.size(); ++applied)
            remembered(c.effects[applied], now);
        return now;
    };
    while (next(c, facts, fresh, trace))
    {
    }
    return whole(c);
}

std::vector<std::string> Library::framesFor(const Query& q) const
{
    std::vector<std::string> out;
    for (const auto& f : frames_)
        if (std::all_of(f.when.begin(), f.when.end(), [&](const Criterion& c) { return holds(c, q, 0); }))
            out.push_back(f.name);
    return out;
}

int Library::budgetFor(const Query& q, const std::vector<std::string>& frames, std::uint64_t seed) const
{
    // The smaller of the two speakers' ranges by their talk (plain when not known), drawn; +2 for friends and kin;
    // scaled by the frames (§8.4), within 2-14.
    const auto range = [&](const Speaker& s) {
        const auto t = s.facts.find("talk");
        const auto r = budgets_.byTalk.find(t == s.facts.end() ? "plain" : t->second);
        return r == budgets_.byTalk.end() ? std::pair<int, int>{5, 8} : r->second;
    };
    auto r = range(q.a);
    if (!q.alone)
        if (const auto rb = range(q.b); rb.second < r.second || (rb.second == r.second && rb.first < r.first))
            r = rb;
    int n = r.first + int(mix(seed ^ 0xB0D6E7ULL) % std::uint64_t(std::max(1, r.second - r.first + 1)));
    const auto band = q.facts.find("pair.band");
    const auto kin = q.facts.find("pair.kin");
    if ((band != q.facts.end() && (band->second == "friends" || band->second == "family" || band->second == "spouses")) ||
        (kin != q.facts.end() && kin->second != "no"))
        n += budgets_.friendsOrKin;
    double scale = 1;
    for (const auto& name : frames)
        for (const auto& f : frames_)
            if (f.name == name)
                scale *= f.budget;
    return std::clamp(int(std::lround(n * scale)), budgets_.least, budgets_.most);
}

std::string Library::toneOf(const Entry& e) const
{
    if (!e.tones.empty() && !e.tones.front().empty())
        return e.tones.front();
    if (!e.does.empty() && !e.does.front().empty())
        return moves_.toneOf(e.does.front().front());
    return {};
}

bool Library::toneBlocked(const Conversation& c, const std::string& tone) const
{
    // §8.11: light is blocked for 2 lines after a grave one, cross for 1 after a tender one, unless a bridging move
    // (change_subject, assess, preclose) came between.
    if (tone.empty())
        return false;
    for (const auto& r : moves_.toneRules())
    {
        if (!r.blocks.count(tone))
            continue;
        const int n = int(c.lines.size());
        for (int i = n - 1; i >= 0 && i >= n - r.beats; --i)
        {
            if (std::any_of(c.lines[std::size_t(i)].moves.begin(), c.lines[std::size_t(i)].moves.end(),
                            [&](const std::string& m) {
                                return std::any_of(r.bridge.begin(), r.bridge.end(), [&](const std::string& b) { return moves_.kindOf(m, b); });
                            }))
                break;                              // (Bridged since.)
            if (c.lines[std::size_t(i)].tone == r.after)
                return true;
        }
    }
    return false;
}

bool Library::forbidden(const Conversation& c, const Entry& e, const Query& q, int speaker) const
{
    if (e.does.empty() || e.does.front().empty())
        return false;
    const auto& moves = e.does.front();
    for (const auto& name : c.frames)
        for (const auto& f : frames_)
            if (f.name == name)
                for (const auto& m : f.forbid)
                    if (hasMove(moves_, moves, m))
                        return true;
    // §8.11: no teasing one who mourns.
    const auto& other = speaker == 0 ? q.b : q.a;
    if (const auto mourning = other.facts.find("mourning"); mourning != other.facts.end() && mourning->second != "no" &&
                                                           (hasMove(moves_, moves, "tease") || hasMove(moves_, moves, "join.tease")))
        return true;
    return false;
}

Query Library::beatQuery(const Conversation& c, const std::function<Query(int, int)>& facts, int speaker, int to, int& side) const
{
    // The opener and the one first spoken to keep a and b as begin had them; a query with the third in it has the
    // speaker as a (next's contract).
    side = speaker < 2 && to < 2 ? speaker : 0;
    auto q = facts(speaker, to);
    for (const auto& [k, v] : c.conv)
        q.facts["conv." + k] = v;
    overlay(c, q);
    accommodate(q);                                 // (Phase 6: each one's accent toward the other; the town's mood.)
    stanceFacts(c, q);
    return q;
}

void Library::overlay(const Conversation& c, Query& q)
{
    // Phase 5: the topic the talk turned to (a pivot) over the one it began on, and who knew its claim.
    if (!c.topicFacts.empty())
    {
        for (auto it = q.facts.begin(); it != q.facts.end();)
            it = it->first.rfind("topic.", 0) == 0 ? q.facts.erase(it) : std::next(it);
        for (const auto& [k, v] : c.topicFacts)
            q.facts[k] = v;
        for (auto* s : {&q.a, &q.b})
            if (const auto id = s->facts.find("id"); id != s->facts.end())
            {
                if (const auto k = c.knows.find(id->second); k != c.knows.end())
                    s->facts["knows"] = k->second;
                else
                    s->facts.erase("knows");
            }
    }
    if (!c.topics.empty())
        q.facts["conv.topic"] = c.topics.back();
}

Query Library::beatQuery(const Conversation& c, const std::function<Query(int, int)>& facts, int speaker) const
{
    int side = 0;
    return beatQuery(c, facts, speaker, c.people.size() < 2 ? -1 : 1 - speaker, side);
}

int Library::partner(const Conversation& c, int who)
{
    for (auto it = c.lines.rbegin(); it != c.lines.rend(); ++it)
        if (it->who == who && it->to >= 0 && it->to != who)
            return it->to;
    for (auto it = c.lines.rbegin(); it != c.lines.rend(); ++it)
        if (it->who != who)
            return it->who;
    return who == 0 ? 1 : 0;
}

void Library::place(Said& s, int speaker, int to)
{
    for (auto& l : s.lines)
        l.first = speaker;
    for (auto& l : s.info)
    {
        l.who = speaker;
        l.to = to;
    }
}

namespace
{
// The facts of the moment (§8.2: here.*, the hour and the weather; a speaker hurt, fighting, at work, mourning), which a
// scene's later turn is checked against again when it comes due. Never what is remembered (mem.*, said:), which the
// scene's own first lines change, nor what the talk itself may move (a bond).
bool momentCriterion(const Criterion& c)
{
    if (c.name.rfind("mem.", 0) == 0 || c.name.rfind("said:", 0) == 0)
        return false;
    static const std::set<std::string> Now{"injured", "fighting", "working", "shift", "on_watch", "tired", "hungry", "mourning"};
    switch (c.ns)
    {
    case Ns::Here: return true;
    case Ns::A:
    case Ns::B:
    case Ns::Self:
    case Ns::Other: return Now.count(c.name) > 0;
    default: return false;
    }
}
} // namespace

bool Library::untrueScene(const Entry& e, const Query& q) const
{
    // Doc 84 §8.6 (Phase 4): a scene whose turn says "I'd heard" for one who didn't know (their `knows`, the game's, on
    // a topic with a claim), or "Never!" for one who did, is never chosen.
    if (moves_.empty() || e.does.empty())
        return false;
    for (std::size_t t = 0; t < e.turns.size() && t < e.does.size(); ++t)
    {
        const auto& s = e.turns[t].who == 0 ? q.a : q.b;
        const auto k = s.facts.find("knows");
        if (k == s.facts.end())
            continue;
        const auto& does = e.does[t];
        const auto has = [&](const char* m) { return std::find(does.begin(), does.end(), m) != does.end(); };
        if ((k->second == "no" && (has("react.news.known") || has("react.news.again"))) || (k->second == "yes" && has("react.news.new")))
            return true;
    }
    return false;
}

bool Library::stillHolds(const SaidLine& line, const Query& q) const
{
    const Entry* e = find(line.entry);
    if (!e || line.turn < 0 || std::size_t(line.turn) >= e->turns.size())
        return true;                                // (Written outside the library: nothing to check.)
    if (!std::all_of(e->when.begin(), e->when.end(), [&](const Criterion& c) { return !momentCriterion(c) || holds(c, q, 0); }))
        return false;
    const auto& turn = e->turns[std::size_t(line.turn)];
    if (line.variant < 0 || std::size_t(line.variant) >= turn.variants.size())
        return true;
    const auto& v = turn.variants[std::size_t(line.variant)];
    return std::all_of(v.when.begin(), v.when.end(), [&](const Criterion& c) { return !momentCriterion(c) || holds(c, q, turn.who); });
}

void Library::take(Conversation& c, Said said) const
{
    // A scene's turns stop before a player's (§13: the engine never writes a player's line; Slice 2's review). The first
    // line taken is always said: no caller asks a player to speak.
    bool first = true;
    for (auto& line : said.info)
    {
        if (!first && c.players.count(line.who))
        {
            ++c.dropped;
            continue;
        }
        first = false;
        c.queued.push_back(std::move(line));
    }
    for (auto& e : said.effects)
        c.effects.push_back(std::move(e));
    for (auto& id : said.chain)
        c.chain.push_back(std::move(id));
}

SaidLine Library::sayNext(Conversation& c) const
{
    auto line = std::move(c.queued.front());
    c.queued.pop_front();
    const bool last = c.queued.empty();
    // What it answers is the line before it (Phase 2's `answers`), said by another.
    if (!c.lines.empty() && line.answers.empty() && c.lines.back().who != line.who)
        line.answers = answersOf(c.lines.back().moves, line.moves);
    const Entry* e = find(line.entry);
    if (!c.legacy && !moves_.empty() && settings_.ends.count(line.text))
    {
        // A shrug ("Mm.") is an ack (§8.4): it owes nothing, whatever its rule hands on, and the other may carry on.
        line.moves = {"ack"};
        line.owes.clear();
        line.invites.clear();
        line.tone = moves_.toneOf("ack");
        line.answers = c.lines.empty() || c.lines.back().who == line.who ? std::vector<std::string>{}
                                                                         : answersOf(c.lines.back().moves, line.moves);
    }
    // The conversation's own facts for the lines after (§7.11): every tag, the move just made and the one before, its
    // tone, the lines said. A question that doesn't say what it asks ({ask=...}, shared8_detail.talk) leaves no earlier
    // question's kind behind it (Slice 3).
    if (!line.tags.count("ask") && std::any_of(line.moves.begin(), line.moves.end(), [&](const std::string& m) { return moves_.kindOf(m, "ask.detail"); }))
        c.conv.erase("ask");
    for (const auto& [k, v] : line.tags)
        c.conv[k] = v;
    line.topic = c.topics.empty() ? c.topic : c.topics.back();
    if (!line.moves.empty())
    {
        if (const auto m = c.conv.find("move"); m != c.conv.end())
            c.conv["prev"] = m->second;
        c.conv["move"] = line.moves.front();
    }
    if (!line.tone.empty())
    {
        c.conv["tone"] = line.tone;
        c.tone = line.tone;
    }
    c.conv["beat"] = std::to_string(++c.beat);
    if (!c.legacy && !moves_.empty())
    {
        // Who owes what now (§8.3): whatever its speaker owed is answered or passed over by speaking; a scene's own
        // lines answer each other, so only its last line's (and a rule's) obligations stand. A third cutting in owed
        // nothing and answers nothing: what the others owed is still theirs to answer (Slice 3, H1).
        // (One who adds a word, sides or teases; one who objects, defends or corrects turns the talk to themselves.)
        const bool cutsIn = std::any_of(line.moves.begin(), line.moves.end(), [&](const std::string& m) {
            return moves_.kindOf(m, "join.add") || moves_.kindOf(m, "join.side") || moves_.kindOf(m, "join.tease");
        });
        std::vector<Conversation::Open> stillOwed;
        if (cutsIn)
            for (const auto& o : c.open)
                if (!o.soft && (o.to >= 0 ? o.to : partner(c, o.by)) != line.who)
                    stillOwed.push_back(o);
        c.open = std::move(stillOwed);
        // Owed by the one it was said to (a third speaker's line names its own: Phase 4).
        const auto toOf = [&](const SaidLine& l) {
            return c.people.size() < 2 ? -1 : l.to >= 0 && l.to != l.who ? l.to : partner(c, l.who);
        };
        if (last)
        {
            const bool shrug = settings_.ends.count(line.text) > 0;
            const auto push = [&](const std::vector<std::string>& list, bool soft) {
                for (const auto& o : list)
                {
                    Conversation::Open open;
                    open.by = line.who;
                    open.soft = soft;
                    auto alts = o;
                    if (const auto colon = o.find(':'); colon != std::string::npos)
                    {
                        const auto who = o.substr(0, colon);
                        alts = o.substr(colon + 1);
                        open.to = who == "self" ? line.who : -1;
                    }
                    else
                        open.to = toOf(line);
                    open.moves = split(alts, '|');
                    // A `then` hand-on: its concept's own rules answer it best (its owes were derived from it).
                    if (e && !e->scene && !e->thenConcept.empty() &&
                        std::any_of(e->owes.begin(), e->owes.end(), [&](const Entry::Owed& w) { return w.derived && joined(w.moves) == alts; }))
                        open.concept = e->thenConcept;
                    c.open.push_back(std::move(open));
                }
            };
            if (!shrug)
            {
                push(line.invites, true);
                push(line.owes, false);
                // A hand-on to a topic (then other: talk.day) owes no move: only its concept, taken if a rule holds.
                if (e && !e->scene && !e->thenConcept.empty() && moves_.alias(e->thenConcept).empty())
                {
                    Conversation::Open open;
                    open.by = line.who;
                    open.to = e->thenWho == "self" ? line.who : toOf(line);
                    open.concept = e->thenConcept;
                    c.open.push_back(std::move(open));
                }
            }
        }
        // The phase (§8.4): a preclose or farewell closes; a move that ends (farewell.return, leave, break) ends it.
        for (const auto& m : line.moves)
            if (moves_.kindOf(m, "extend"))
            {
                ++c.extended;                       // ("Oh, before you go...": back to the talk, once.)
                c.phase = Conversation::Phase::Body;
            }
            else if (const auto* mv = moves_.find(m); mv && mv->family == "closing")
            {
                if (c.phase != Conversation::Phase::Closing)
                    c.phase = Conversation::Phase::Closing;
                if (mv->ends && last)
                {
                    c.phase = Conversation::Phase::Ended;
                    c.ended = "farewell";
                }
            }
        if (c.phase == Conversation::Phase::Opening)
            c.phase = Conversation::Phase::Body;
    }
    // §37: what part of the talk it is (the scorecard's H1 counts the share of whole scenes against the edges).
    if (!c.legacy && !moves_.empty())
    {
        const bool closing = std::any_of(line.moves.begin(), line.moves.end(), [&](const std::string& m) {
            const auto* mv = moves_.find(m);
            return mv && mv->family == "closing" && !moves_.kindOf(m, "extend") && !moves_.kindOf(m, "break");
        });
        if (closing)
            line.part = "close";
        else if (e && e->scene && line.turn >= 0)
            line.part = "scene";
        else if (c.body == "written" && line.entry.empty())
            line.part = "written";                  // (The game's authored exchange, its body.)
        else if (c.lines.empty() || (c.body == "exchange" && c.lines.size() == 1 && c.lines.back().who != line.who && line.who < 2))
            line.part = "exchange";
        else
            line.part = "edge";
        if (line.part == "edge" || (line.part == "exchange" && !c.lines.empty()))
            ++c.stitched;
    }
    c.said.insert(line.entry);
    if (!line.text.empty() && line.text.back() == '?')
        c.asked.insert(lower(line.text));
    c.lines.push_back(line);
    return line;
}

void Library::end(Conversation& c, const std::string& why, Trace* trace) const
{
    if (c.phase != Conversation::Phase::Ended && trace && !c.legacy)
        trace->why += "; ended: " + why;
    c.phase = Conversation::Phase::Ended;
    if (c.ended.empty())
        c.ended = why;
    c.queued.clear();
    c.open.clear();
}

void Library::setUp(Conversation& c, const Query& q) const
{
    c.concept = q.concept;
    c.legacy = moves_.empty();
    c.people.clear();
    c.players.clear();
    for (const auto* s : {&q.a, &q.b})
    {
        if (s == &q.b && q.alone)
            break;
        const auto id = s->facts.find("id");
        const auto player = s->facts.find("player");
        if (s->player || (player != s->facts.end() && player->second == "yes"))
            c.players.insert(int(c.people.size()));
        c.people.push_back(id == s->facts.end() ? std::string() : id->second);
    }
    if (const auto kind = q.facts.find("topic.kind"); kind != q.facts.end())
        c.topic = kind->second;
    else if (q.concept.rfind("talk.", 0) == 0)
        c.topic = q.concept.substr(5);
    c.topics.clear();
    if (q.concept.rfind("talk.", 0) == 0)
        c.topics.push_back(q.concept.substr(5));
    deepen(c, q);
    if (!c.legacy)
    {
        c.frames = framesFor(q);
        c.budget = budgetFor(q, c.frames, c.seed);
    }
}

void Library::preface(Conversation& c, Said& said, const Query& q, const Freshness& fresh, Trace* trace) const
{
    // Said in the opener's breath, one line (the listener's first line stays the opener's words), before an opener that
    // says what it does: the frames' opening (§8.4: strangers greet properly, a host offers a seat), unless it opens
    // itself; and a return to a talk left unfinished on this topic (§8.10: "As I was saying..."), unless it returns
    // itself. An opener that says nothing of what it does (Phase 7 tags it) is left as it is.
    if (c.legacy || c.concept.rfind("talk.", 0) != 0 || said.info.empty() || said.info.front().moves.empty() ||
        c.people.size() < 2 || (said.info.front().who != 0 && c.cast.empty()) || said.info.front().who > 1)
        return;
    const int opener = said.info.front().who;       // (§43: 1 when the one spoken to opens a scene of theirs, cast.)
    const auto does = [&](const std::string& what) {
        const auto& first = said.info.front().moves;
        return std::any_of(first.begin(), first.end(), [&](const std::string& m) {
            const auto* mv = moves_.find(m);
            return mv && (what == "opening" ? mv->family == "opening" : moves_.kindOf(m, what));
        });
    };
    // (No word of four letters or more said twice in one breath: "Oh, you're back. I miss Marketday back home." The
    // breath goes without what was to come before. Slice 3.)
    const auto words = [](const std::string& t) {
        std::set<std::string> out;
        std::string w;
        for (const char ch : t + " ")
            if (std::isalpha(static_cast<unsigned char>(ch)))
                w += char(std::tolower(static_cast<unsigned char>(ch)));
            else
            {
                if (w.size() >= 4)
                    out.insert(w);
                w.clear();
            }
        return out;
    };
    const auto echoes = [&](const Said& before) {
        const auto mine = words(before.info.front().text);
        const auto theirs = words(said.info.front().text);
        return std::any_of(mine.begin(), mine.end(), [&](const std::string& w) { return theirs.count(w) > 0; });
    };
    const auto prepend = [&](Said& before, const std::string& why) {
        if (echoes(before))
        {
            if (trace)
                trace->why += "; " + why + " left out (" + before.id + " says a word the opener says)";
            return;
        }
        auto& g = before.info.front();
        auto& line = said.info.front();
        // Phase 7 (§8.12.3): what is said in the opener's breath makes a longer line; if another said those very words
        // here within the hour (the same greeting before the same short opener), the opener goes without it.
        if (fresh.collides)
        {
            Said whole;
            whole.info.push_back(line);
            whole.info.front().text = g.text + " " + line.text;
            if (collides(whole, q, fresh))
            {
                if (trace)
                    trace->why += "; " + why + " left out (another said those words here)";
                return;
            }
        }
        if (line.bare.empty())
            line.bare = line.text;
        line.text = g.text + " " + line.text;
        said.lines.front().second = line.text;
        line.moves.insert(line.moves.begin(), g.moves.begin(), g.moves.end());
        // (Phase 8: a word that stands on the ledger is a callback, C9; "Back again?" isn't one: the raters' worst frame,
        // it recalls nothing said. Slice 2's review.)
        line.callback = line.callback || (g.callback && std::find(g.moves.begin(), g.moves.end(), "again.open") == g.moves.end());
        line.layers.insert(line.layers.end(), g.layers.begin(), g.layers.end());
        line.habits.insert(line.habits.end(), g.habits.begin(), g.habits.end());
        said.chain.push_back(before.id);
        said.effects.insert(said.effects.begin(), before.effects.begin(), before.effects.end());
        c.said.insert(before.id);
        if (trace)
            trace->why += "; " + why + ": " + before.id;
    };
    const auto one = [&](const std::string& move) {
        Conversation::Open o;
        o.moves = {move};
        auto s = answer(c, o, q, opener, fresh, nullptr);
        return s.id.empty() || s.info.size() != 1 ? Said{} : s;
    };
    // A thread left unfinished on this topic (Phase 4: pair.thread, kept two game days; the ledger's from Phase 5).
    Said back;
    if (const auto thread = q.facts.find("pair.thread"); thread != q.facts.end() && !c.topic.empty() && thread->second == c.topic &&
                                                         !does("return"))
        back = one("return");
    // Phase 8 (§8.7, §33.3): else, half the time, a word that the two were on this lately, or how it went then (a
    // `recall` move line reading the ledger: pair.talked.<topic>, pair.recall.<topic>), on the talk's own topic.
    const auto onLedger = [&] {                     // (Asked only where the ledger has something to recall: §16, B1.)
        if (c.topics.empty())
            return false;
        const auto last = q.facts.find("pair.last_outcome");
        return q.facts.count("pair.talked." + c.topics.back()) > 0 || (last != q.facts.end() && last->second == "broken");
    };
    // (Never before an opener that stands on the ledger itself: "Prices again. ... Prices again." Slice 2's review.)
    if (back.id.empty() && !does("return") && !does("recall") && !said.info.front().callback && moves_.find("recall") && onLedger() &&
        unit(mix(c.seed * 0x9E3779B97F4A7C15ULL + 0x5EC0)) < .5)
    {
        auto onTopic = q;
        onTopic.facts["conv.topic"] = c.topics.back();
        Conversation::Open o;
        o.moves = {"recall"};
        if (auto s = answer(c, o, onTopic, opener, fresh, nullptr); !s.id.empty() && s.info.size() == 1)
            back = std::move(s);
    }
    // (§37: nor a greeting before a goodbye: "Evening, neighbour. Mind how you go." A talk that is only its leave-taking.)
    const bool leaving = std::any_of(said.info.front().moves.begin(), said.info.front().moves.end(), [&](const std::string& m) {
        const auto* mv = moves_.find(m);
        return mv && (mv->family == "closing" || mv->family == "repair");
    });
    if (leaving)
        back = {};
    // (Nor before a repair: "Good evening, friend. Come again?" asks the other to say again what they never said.)
    const bool opens = does("opening") || leaving;
    Said greet;
    std::vector<std::string> open;
    // (Met already today, the two don't greet, sit down or say kin's first words all over again: the innkeeper's "Sit
    // yourself down." to the same regular at noon and at midnight. Only, now and then, that it's twice. Slice 3.)
    const bool again = std::find(c.frames.begin(), c.frames.end(), "again_today") != c.frames.end();
    for (const auto& name : c.frames)
        for (const auto& f : frames_)
            if (f.name == name && (!again || name == "again_today"))
                for (const auto& m : f.open)
                    if (std::find(open.begin(), open.end(), m) == open.end())
                        open.push_back(m);
    // Slice 3 (H1, the first blind ratings: "Back again?" in a third of the talks rated 2.50 against 3.65 without it):
    // meeting twice in a day is remarked on now and then, before small talk, never before a sorrow, a memory, a crime or
    // anyone's news, never beside a recall or a return (two openers in one breath at most: the opener and one before it),
    // and never when the opener starts with the same word ("Back again? Back home rich and poor...").
    const auto againFits = [&](const Said& g) {
        static const std::set<std::string> Light{"day", "weather", "smalltalk", "food", "pastimes", "animals", "work", "travel",
                                                 "land", "season", "player", "lore", "home", "friends", "festival", "dreams"};
        const auto& line = said.info.front();
        if (!back.id.empty() || !Light.count(c.topics.empty() ? c.topic : c.topics.back()) ||
            (line.tone != "light" && line.tone != "even" && !line.tone.empty()) || unit(mix(c.seed * 0x9E3779B97F4A7C15ULL + 0xA6A1)) >= .25)
            return false;
        const auto firstWord = [](const std::string& t) {
            std::string w;
            for (const char ch : t)
            {
                if (!std::isalpha(static_cast<unsigned char>(ch)))
                {
                    if (!w.empty())
                        break;
                    continue;
                }
                w += char(std::tolower(static_cast<unsigned char>(ch)));
            }
            return w;
        };
        const auto lower = [](std::string t) {
            for (auto& ch : t)
                ch = char(std::tolower(static_cast<unsigned char>(ch)));
            return t;
        };
        // (Nor a word of four letters or more said twice in the breath: "Back again? Swallows are back.")
        const auto said0 = lower(g.info.front().text);
        const auto opener = firstWord(line.text);
        return (opener.empty() || said0.find(opener) == std::string::npos) && !echoes(g);
    };
    for (std::size_t i = 0; !opens && i < open.size() && greet.id.empty(); ++i)
    {
        greet = one(open[i]);
        if (!greet.id.empty() && open[i] == "again.open" && !againFits(greet))
        {
            greet = {};
            continue;
        }
        // §37, the hybrid: nothing asked in the opener's breath that nobody answers ("Morning. Sleep well? Down here
        // they'd rob..."; "Back again? ..."): the raters' muddled openers. A greeting that asks is left out.
        if (!greet.id.empty() && c.hybrid && greet.info.front().text.find('?') != std::string::npos)
        {
            if (trace)
                trace->why += "; " + greet.id + " left out (it asks, and the opener goes on)";
            greet = {};
            continue;
        }
        if (!greet.id.empty() && trace)
            trace->why += "; greeted first (" + open[i] + ")";
    }
    // One said before the opener at most: a greeting the meeting asks for (strangers, kin, a host) goes before a recall.
    if (!back.id.empty() && !greet.id.empty())
    {
        if (trace)
            trace->why += "; " + back.id + " left out (one word before the opener is enough)";
        back = {};
    }
    if (!back.id.empty())
        prepend(back, does("return") || back.info.front().moves.empty() || back.info.front().moves.front() != "recall"
                          ? "returned to the thread" : "recalled their last talk");
    if (!greet.id.empty())
        prepend(greet, "greeting");
}

std::optional<SaidLine> Library::begin(Conversation& c, const Query& q, const Freshness& fresh, Trace* trace, const Query* turned) const
{
    // The opener: the concept's, as pick chooses it (the director's topic; Phase 5's agenda chooses from Phase 5).
    setUp(c, q);
    auto asked = q;
    accommodate(asked);                             // (Phase 6: the opener's accent toward the one spoken to; the mood.)
    // §37: a talk between residents on a topic is carried by a whole scene where one fits.
    const bool hybrid = settings_.hybrid && !c.legacy && c.concept.rfind("talk.", 0) == 0 && c.people.size() >= 2 && c.players.empty();
    // Never what the one spoken to was told before (Phase 6, §8.6: the ledger's told set; a story they heard): not this
    // opener, nor the authored lines (the game leaves a talk "told" unbegun).
    const auto* found = (moves_.empty() || !fresh.told) ? nullptr : &asked;
    auto recent = fresh.recent;
    Said said;
    for (int tries = 0; tries < 6; ++tries)
    {
        // (§37: whole scenes first; where every scene that fits would tell them again, a rule's short exchange may.)
        said = pickOne(asked, c.seed + std::uint64_t(tries) * 0x2545F491ULL, fresh.heard, recent, 0, false, trace, hybrid && tries < 4);
        const auto* e = said.id.empty() || !found ? nullptr : find(said.id);
        const bool told = e && toldAlready(c, *e, asked, 0, fresh);
        if (said.id.empty() || (!told && !collides(said, asked, fresh)))
            break;
        if (trace)
            trace->why += "; " + said.id + (told ? " would tell them again" : " repeats words said here lately");
        recent.push_back(said.id);
        said = {};
        if (told && c.ended.empty())
            c.ended = "told";
    }
    c.cast.clear();
    // §43 (U1): where the opener's own mantle carries nothing here, the one spoken to's own scene does, cast with them
    // opening it: a whole scene of theirs that fits, unheard, said by them as written. Only where the talk would
    // otherwise be a shared scene (or nothing at all), never over a rule that says the topic's facts or a line that must
    // be said, and never on a claim (`knows`: the teller's to tell).
    if (turned && hybrid && settings_.mantleFirst && !q.alone && !turned->alone && !q.b.facts.count("knows") &&
        q.b.facts.count("mantle"))
    {
        const auto* chosen = said.id.empty() ? nullptr : find(said.id);
        const bool shared = !chosen || (chosen->scene && said.info.size() >= 2 && chosen->priority < settings_.strict &&
                                        !ownEntry(*chosen, asked, 0));
        if (shared)
        {
            auto other = *turned;
            accommodate(other);
            auto cast = pickOne(other, mix(c.seed ^ 0xCA57CA57ULL), fresh.heard, recent, 0, false, nullptr, true, true);
            const auto* e = cast.id.empty() ? nullptr : find(cast.id);
            // (Never saying less of the topic's facts than the scene it takes the place of: the director chose them.)
            const bool saysAsMuch = !e || !chosen || factsSaid(*e, cast, other).size() >= factsSaid(*chosen, said, asked).size();
            if (e && e->scene && cast.info.size() >= 2 && saysAsMuch && !toldAlready(c, *e, other, 0, fresh) &&
                !collides(cast, other, fresh))
            {
                for (auto& l : cast.lines)
                    l.first = 1 - l.first;
                for (auto& l : cast.info)
                {
                    l.who = 1 - l.who;
                    if (l.to == 0 || l.to == 1)
                        l.to = 1 - l.to;
                }
                if (trace)
                    trace->why += "; cast with the one spoken to opening their own " + cast.id;
                said = std::move(cast);
                c.cast = said.id;
                c.frames = framesFor(other);        // (Their frames: an innkeeper casting her own is the host.)
                c.ended.clear();
            }
        }
    }
    if (said.id.empty())
    {
        const auto why = c.ended == "told" ? "told" : "nothing";
        c.ended.clear();
        end(c, why, nullptr);
        return std::nullopt;
    }
    c.ended.clear();
    c.repeat = said.repeat;
    c.hybrid = hybrid;
    if (const auto* e = find(said.id); e && !c.legacy)
        c.body = e->scene && said.info.size() >= 2 ? "scene" : "exchange";
    preface(c, said, asked, fresh, trace);
    take(c, std::move(said));
    return sayNext(c);
}

std::optional<SaidLine> Library::beginWith(Conversation& c, const Query& q, Said written, Trace* trace) const
{
    // Lines written whole outside the library (the game's authored exchange): said as they are, then the engine goes on.
    setUp(c, q);
    // (§37, the hybrid: an authored exchange is the talk's body, as a scene is: only its edges after it, never a lull's
    // opinion or invitation stitched on after "A quiet day, for once." / "It is at that.". The ticking listen found it.)
    c.hybrid = settings_.hybrid && !c.legacy && c.concept.rfind("talk.", 0) == 0 && c.people.size() >= 2 && c.players.empty();
    c.body = "written";
    if (written.lines.empty())
    {
        end(c, "nothing", nullptr);
        return std::nullopt;
    }
    if (written.info.size() != written.lines.size())
    {
        written.info.clear();
        for (const auto& [who, text] : written.lines)
        {
            SaidLine l;
            l.who = who;
            l.to = q.alone ? -1 : 1 - who;
            l.text = text;
            written.info.push_back(std::move(l));
        }
    }
    for (auto& l : written.info)
        if (l.to < 0 && !q.alone)
            l.to = 1 - l.who;
    if (trace)
        trace->why += "written: " + std::to_string(written.lines.size()) + " lines";
    take(c, std::move(written));
    return sayNext(c);
}

std::optional<SaidLine> Library::join(Conversation& c, const std::string& id, const std::string& stake,
                                      const std::vector<std::string>& moves, const Query& q, const Freshness& fresh,
                                      Trace* trace) const
{
    // A third voice (§8.9): only into a talk of two in its body, with room for the joining line, its answer and the
    // closing, said to the one who spoke last; the joiner cuts in before whatever was owed, and what was owed is still
    // answered after (Slice 3: H1's lowest, "Don't let me stop you. I'll just listen." and the one spoken to never
    // answered, the talk closed on nothing).
    if (c.legacy || moves_.empty() || c.people.size() != 2 || c.lines.empty() || !c.queued.empty() || moves.empty() ||
        c.phase != Conversation::Phase::Body || int(c.lines.size()) + 4 > budgets_.most ||
        // (Slice 3: one who joins only for the pleasure of the talk, its topic their favourite or the two their friends,
        // does so only where the talk has room to go on after: "Oh, let me in." and then goodbyes, else.)
        ((stake == "favourite" || stake == "friends") && int(c.lines.size()) + 6 > c.budget) || id.empty() ||
        std::find(c.people.begin(), c.people.end(), id) != c.people.end())
        return std::nullopt;
    const int to = c.lines.back().who;
    Conversation::Open o;
    o.by = to;
    o.to = 2;
    o.moves = moves;
    Query asked = q;
    for (const auto& [k, v] : c.conv)
        asked.facts["conv." + k] = v;
    asked.facts["conv.stake"] = stake;
    if (!c.topics.empty() && !asked.facts.count("conv.topic"))
        asked.facts["conv.topic"] = c.topics.back(); // (Phase 8: joins by the topic they cut in on, §33.3.)
    accommodate(asked);
    stanceFacts(c, asked);                          // (Phase 6: a joiner sides with an opinion, or corrects it.)
    auto s = answer(c, o, asked, 0, fresh, trace);
    if (s.id.empty() || s.info.size() != 1)
        return std::nullopt;
    c.people.push_back(id);
    c.stake = stake;
    c.conv["stake"] = stake;
    place(s, 2, to);
    for (auto& line : s.info)
        line.fit = line.best = -1;                  // (It answers nothing owed: it cuts in.)
    if (trace)
        trace->why += "; " + id + " joins (" + stake + "): " + s.id;
    take(c, std::move(s));
    return sayNext(c);
}

std::optional<SaidLine> Library::interrupt(Conversation& c, const std::string& why, int who,
                                           const std::function<Query(int, int)>& facts, const Freshness& fresh,
                                           Trace* trace) const
{
    // §8.10: the talk breaks off, with a line that says so (a `break` move line, by conv.interrupt), and ends.
    if (c.phase == Conversation::Phase::Ended)
        return std::nullopt;
    c.conv["interrupt"] = why;
    c.queued.clear();
    c.open.clear();
    std::optional<SaidLine> said;
    if (!c.legacy && !moves_.empty() && c.people.size() >= 2 && who >= 0 && who < int(c.people.size()))
    {
        // (§37: the one who breaks it off isn't the one who has just spoken, where the other can: nobody twice running.)
        // (Not when one of them is called away: "One moment, Wick." is the called one's to say.)
        if (c.hybrid && why != "called" && !c.lines.empty() && c.lines.back().who == who && !c.players.count(partner(c, who)))
            who = partner(c, who);
        const int to = partner(c, who);
        int side = 0;
        const auto q = beatQuery(c, facts, who, to, side);
        Conversation::Open o;
        o.by = to;
        o.to = who;
        o.moves = {"break"};
        if (auto s = answer(c, o, q, side, fresh, trace); !s.id.empty() && s.info.size() == 1)
        {
            place(s, who, to);
            for (auto& line : s.info)
                line.fit = line.best = -1;
            if (trace)
                trace->why += "; broken off (" + why + "): " + s.id;
            take(c, std::move(s));
            said = sayNext(c);
        }
    }
    c.ended.clear();
    end(c, "break", trace);
    return said;
}

std::optional<SaidLine> Library::legacyNext(Conversation& c, const std::function<Query(int, int)>& facts,
                                            const Freshness& fresh, Trace* trace) const
{
    // Before Phase 3 (a library without moves.json; the golden transcripts): each rule's `then` handed on, the next beat
    // the other's (or the same speaker's) best rule for that concept, up to 8 beats; a shrug ends it.
    if (c.lines.empty() || c.chain.empty())
    {
        end(c, "nothing", nullptr);
        return std::nullopt;
    }
    const Entry* e = find(c.chain.back());
    const int speaker = e && e->thenWho == "self" ? c.lines.back().who : 1 - c.lines.back().who;
    auto q = beatQuery(c, facts, speaker);
    const int beat = c.handOns + 1;
    if (!e || e->scene || e->thenConcept.empty() || q.alone || beat >= LegacyBeats || settings_.ends.count(c.lines.back().text))
    {
        end(c, !e || e->scene || e->thenConcept.empty() || q.alone ? "then" : beat >= LegacyBeats ? "cap" : "shrug", nullptr);
        return std::nullopt;
    }
    q.concept = e->thenConcept;
    auto said = pickOne(q, mix(c.seed + std::uint64_t(beat) * 7919), fresh.heard, {}, speaker, true, nullptr);
    if (said.id.empty())
    {
        if (trace)
            trace->why += "; then " + q.concept + " (" + (speaker ? "b" : "a") + "): nothing fits, the talk ends";
        end(c, "nothing", nullptr);
        return std::nullopt;
    }
    if (trace)
        trace->why += "; then " + q.concept + " (" + (speaker ? "b" : "a") + "): " + said.id;
    c.handOns = beat;
    take(c, std::move(said));
    return sayNext(c);
}

std::optional<SaidLine> Library::next(Conversation& c, const std::function<Query(int, int)>& facts, const Freshness& fresh,
                                      Trace* trace) const
{
    if (c.phase == Conversation::Phase::Ended)
        return std::nullopt;
    if (!c.queued.empty())
    {
        // A scene's next turn (§8.2), checked again now it is due (Valve's rule): the moment it was chosen for may have
        // passed (the fog lifted, one of them hurt). If so, the rest of the scene is dropped and the talk goes on from
        // what was said.
        const auto& due = c.queued.front();
        int side = 0;
        if (c.players.count(due.who))
        {
            c.dropped += int(c.queued.size());       // (A player's turn: theirs to say in the window, never ours: §13.)
            c.queued.clear();
            end(c, "done", trace);
            return std::nullopt;
        }
        if (c.legacy || due.entry.empty() || c.people.size() < 2 || due.who < 0 || due.who > 1)
            return sayNext(c);
        // Phase 8 (§8.12.3): nor if another speaker has said its words in this place since the scene was chosen (two
        // talks there chose the same scene within the hour, and the other reached this turn first).
        Said turn;
        turn.info.push_back(due);
        turn.info.back().who = 0;
        Query by;
        by.a.facts["id"] = c.people[std::size_t(due.who)];
        const bool collided = collides(turn, by, fresh);
        auto now = beatQuery(c, facts, 0, 1, side);
        if (!c.cast.empty() && due.entry == c.cast)
            std::swap(now.a, now.b);                // (§43: a cast scene is checked the scene's way round.)
        if (!collided && stillHolds(due, now))
            return sayNext(c);
        if (trace)
            trace->why += "; " + due.entry + "'s turn " + std::to_string(due.turn + 1) +
                          (collided ? " was said here lately by another" : " no longer holds") + ": the rest dropped";
        c.dropped += int(c.queued.size());
        c.queued.clear();
        if (!collided)
            c.open.clear();                         // (A collision leaves what was owed owed: the engine answers it.)
    }
    if (c.legacy && !c.players.empty())
    {
        end(c, "done", trace);                      // (§13: a player answers for themselves.)
        return std::nullopt;
    }
    if (c.legacy)
        return legacyNext(c, facts, fresh, trace);
    if (c.people.size() < 2 || c.lines.empty())
    {
        end(c, "alone", nullptr);
        return std::nullopt;
    }
    const int n = int(c.lines.size());
    if (n >= budgets_.most)
    {
        end(c, "cap", trace);
        return std::nullopt;
    }
    const int last = c.lines.back().who;
    const auto beatSeed = [&](std::uint64_t salt) { return mix(c.seed + std::uint64_t(n) * 7919 + salt); };
    const auto say = [&](Said s, const char* how, const std::string& what, int who, int to) -> std::optional<SaidLine> {
        if (trace)
            trace->why += std::string("; ") + how + " " + what + " (" + char('a' + who) + "): " + s.id;
        place(s, who, to);
        take(c, std::move(s));
        return sayNext(c);
    };
    // 1. What is owed is answered (§8.3), by the one it is owed by, to the one who made it due: the newest obligation
    // first. Past the most lines a talk may have, less the two of a farewell, it is left: the talk closes.
    while (!c.open.empty())
    {
        auto o = c.open.back();
        c.open.pop_back();
        if (n >= budgets_.most - 2 && c.phase != Conversation::Phase::Closing)
            break;
        // §37, the hybrid: past the body, only so many lines are stitched before the goodbyes (an answer to what the
        // scene's last line asked, a joiner's word and its answer), and an extend ("Oh, before you go...") would open
        // what nothing written carries on.
        if (c.hybrid && c.phase != Conversation::Phase::Closing && c.stitched >= settings_.hybridStitch)
            break;
        if (c.hybrid)
            o.moves.erase(std::remove_if(o.moves.begin(), o.moves.end(), [&](const std::string& m) { return moves_.kindOf(m, "extend"); }),
                          o.moves.end());
        const int who = o.to >= 0 ? o.to : partner(c, o.by);
        const int to = who == o.by ? partner(c, who) : o.by;
        if (c.players.count(who))
            continue;                               // (Owed by a player: theirs to answer, in the window or not at all: §13.)
        if (c.hybrid && who == last)
            continue;                               // (§37: nobody speaks twice running but as a scene is written.)
        // §37: past the body, what a line owes is answered at the edges only: a direct question ("Is she armed?"), a
        // joiner's word, or a rule's opener where no scene fit (its answer is the exchange). A scene's last statement is
        // the end of it: no "There's always something." stitched on after "You didn't." (a goodbye's answer, always).
        if (c.hybrid && c.phase != Conversation::Phase::Closing)
        {
            const auto& made = c.lines.back();
            const bool question = !made.text.empty() && made.text.back() == '?';
            const bool exchange = c.body == "exchange" && c.lines.size() == 1;
            if (!exchange && (o.soft || (!question && made.who < 2 && who < 2)))
                continue;                           // (An invitation to say more, past the body: the scene said its piece.)
        }
        int side = 0;
        auto q = beatQuery(c, facts, who, to, side);
        // An invitation is taken up half the time, as the one invited leans (Phase 6, §7.5: a warm wolf sympathises);
        // always, in the hybrid, by the one spoken to where a rule opened for want of a scene (§37: an exchange, not a
        // line said to nobody).
        if (o.soft && !(c.hybrid && c.body == "exchange" && c.lines.size() == 1))
        {
            double leaning = o.moves.empty() ? 1 : 0;
            for (const auto& m : o.moves)
                leaning = std::max(leaning, lean(side == 0 ? q.a : q.b, m));
            if (unit(beatSeed(0x50F7)) >= std::min(1.0, inviteTaken_ * leaning))
                continue;                           // (Passed over: an invitation, not taken this time.)
        }
        int held = 0;
        auto s = answer(c, o, q, side, fresh, trace, &held);
        const std::string what = o.moves.empty() ? o.concept : joined(o.moves);
        if (!s.id.empty())
            return say(std::move(s), "answers", what, who, to);
        if (o.soft || o.moves.empty())
            continue;                               // (An invitation, or a topic handed on: owed by nobody.)
        // Nothing doing it fits: its fallbacks, walked down to ack (§7.2), by move only.
        for (const auto& alt : o.moves)
        {
            const auto chain = moves_.chain(alt);
            for (std::size_t i = 1; i < chain.size(); ++i)
            {
                Conversation::Open f;
                f.by = o.by;
                f.to = o.to;
                f.moves = {chain[i]};
                auto fb = answer(c, f, q, side, fresh, trace);
                if (!fb.id.empty())
                {
                    for (auto& line : fb.info)
                        line.fit = 0, line.best = held;
                    return say(std::move(fb), "falls back on", chain[i] + " for " + what, who, to);
                }
            }
        }
        ++c.gaps;                                   // (Owed, and nobody could answer: the talk closes.)
        if (trace)
            trace->why += "; nothing answers " + what;
        c.open.clear();
        break;
    }
    if (c.phase == Conversation::Phase::Ended)
        return std::nullopt;
    // Only a conversation between residents on a topic (talk.*, as the director starts one) closes or greets: a reaction,
    // a bark or an answer to a player is said, and what it owes answered, and that is all. With a player in it, what the
    // player owes is never answered for them (§13; Slice 2's review: a keeper's greeting had the customer's wolf greet back).
    if (c.concept.rfind("talk.", 0) != 0 || !c.players.empty())
    {
        end(c, "done", trace);
        return std::nullopt;
    }
    // 2. Nothing owed: a lull (§8.3). While the budget has room, a goal's next step or a turn to something on either's
    // agenda (§8.5, Phase 5); else the talk closes (§8.4).
    // §37, the hybrid: no lull at all. A written scene carried the talk; what follows it is only its edges, never another
    // topic, an opinion out of nowhere or a goal begun mid-talk (the raters' "festival days come out of nowhere").
    if (c.phase != Conversation::Phase::Closing && !c.hybrid)
        if (auto line = lull(c, facts, fresh, trace))
            return line;
    if (c.phase != Conversation::Phase::Closing)
    {
        c.phase = Conversation::Phase::Closing;
        // The one at work closes first (of the two who began it), else the one spoken to last.
        int side = 0;
        const auto q = beatQuery(c, facts, 0, 1, side);
        const auto working = [&](const Speaker& s) {
            const auto w = s.facts.find("working");
            return w != s.facts.end() && w->second == "yes";
        };
        const bool aw = working(q.a), bw = working(q.b);
        c.closer = aw != bw ? (aw ? 0 : 1) : partner(c, last);
        if (c.closer >= 2)
            c.closer = last == 0 ? 1 : 0;           // (One who has just joined doesn't close the talk they joined.)
        if (c.hybrid && c.closer == last)
            c.closer = partner(c, last);            // (§37: not the one who has just spoken: "Lovely, and short." / "Bed for me.")
    }
    // 3. Closing (§8.4): a preclose when there's room for the whole of it (preclose, its answer, the farewell pair),
    // then the farewell, answered (above). The frames' preclose makes the room it needs smaller.
    bool precloseSaid = false, farewellSaid = false;
    for (const auto& l : c.lines)
    {
        precloseSaid = precloseSaid || hasMove(moves_, l.moves, "preclose");
        farewellSaid = farewellSaid || hasMove(moves_, l.moves, "farewell") || hasMove(moves_, l.moves, "farewell.return");
    }
    if (farewellSaid)
    {
        end(c, "farewell", trace);
        return std::nullopt;
    }
    double readily = 1;
    for (const auto& name : c.frames)
        for (const auto& f : frames_)
            if (f.name == name)
                readily *= f.preclose;
    int who = c.closer < 0 ? partner(c, last) : c.closer;
    if (c.hybrid && who == last)
        who = partner(c, last);                     // (§37: nobody speaks twice running but as a scene is written.)
    const int to = partner(c, who);
    int side = 0;
    auto q = beatQuery(c, facts, who, to, side);
    // Slice 3 (H1, the first blind ratings): one exchange and then a goodbye each rated 2.85, a talk left where it ended
    // 3.90. A talk that has had only one exchange (fewer than four lines) ends there, the two going back to what they
    // were doing without a word of ceremony; the farewell pair is for a talk that has run a while.
    if (!precloseSaid && n < 4 && c.extended == 0)
    {
        c.closer = who;
        end(c, "parted", trace);
        return std::nullopt;
    }
    // §37, the hybrid: a scene that ends on a question nobody answers ends there, as written ("Yes. Sometime.
    // Tomorrow?" is never met with "Right. I'm away.").
    if (c.hybrid && !precloseSaid && !c.lines.empty() && !c.lines.back().text.empty() && c.lines.back().text.back() == '?' &&
        c.lines.back().part == "scene")
    {
        c.closer = who;
        end(c, "parted", trace);
        return std::nullopt;
    }
    // Only a cold or short-tempered wolf goes without a word (§8.4), now and then (frames.json's curt chance; Phase 6's
    // temperaments weigh it); it costs regard (the game's, from frames.json's curt effects).
    if (!precloseSaid)
    {
        const auto& s = side == 0 ? q.a : q.b;
        const auto trait = [&](const char* k, const char* v) {
            const auto f = s.facts.find(k);
            return f != s.facts.end() && f->second == v;
        };
        if ((trait("warmth", "cold") || trait("temper", "short")) && unit(beatSeed(0xC0DE)) < curt_.curtChance)
        {
            c.closer = who;
            end(c, "curt", trace);
            return std::nullopt;
        }
    }
    // A preclose first when the talk has run a while and there's room for the whole closing.
    // (§37, the hybrid: the goodbye pair alone after a written scene: four lines of leave-taking after four of talk was
    // the ceremony the raters marked down.)
    if (!precloseSaid && !c.hybrid && n >= 4 && n + 4 <= int(std::lround(c.budget * readily)))
    {
        Conversation::Open o;
        o.by = to;
        o.to = who;
        o.moves = {"preclose"};
        if (auto s = answer(c, o, q, side, fresh, trace); !s.id.empty())
        {
            for (auto& line : s.info)
                line.fit = line.best = -1;          // (It answers nothing: it begins the closing.)
            return say(std::move(s), "closes with", "preclose", who, to);
        }
    }
    Conversation::Open o;
    o.by = to;
    o.to = who;
    o.moves = {"farewell"};
    // After a preclose taken, the goodbye alone: not a line that closes again ("Back to it, me too." / "I'll let you be.").
    Said bye;
    if (precloseSaid)
        if (const auto* list = byMove("farewell"))
        {
            Conversation probe = c;
            for (const auto i : *list)
                if (!entries_[i].does.empty() && hasMove(moves_, entries_[i].does.front(), "preclose"))
                    probe.said.insert(entries_[i].id);
            bye = answer(probe, o, q, side, fresh, trace);
        }
    if (bye.id.empty())
        bye = answer(c, o, q, side, fresh, trace);
    if (auto s = std::move(bye); !s.id.empty())
    {
        for (auto& line : s.info)
            line.fit = line.best = -1;
        return say(std::move(s), "takes leave with", "farewell", who, to);
    }
    end(c, c.gaps ? "nothing" : "no closing", trace);
    return std::nullopt;
}

std::optional<SaidLine> Library::lull(Conversation& c, const std::function<Query(int, int)>& facts, const Freshness& fresh,
                                      Trace* trace) const
{
    // Doc 84 §8.3, §8.5 (Phase 5). Room for a step, its answer and the farewell pair, or nothing (the talk closes). A goal
    // under way takes its next step whatever the budget says, while the talk may still have it (Slice 2's review: "I've a
    // favour to ask." / "What is it?" / "I'd better get on."), and one is begun only with room for all of it.
    if (c.legacy || moves_.empty() || c.people.size() < 2 || c.lines.empty())
        return std::nullopt;
    const int n = int(c.lines.size());
    if (n + 4 > budgets_.most || (n + 4 > c.budget && c.goal.empty()))
        return std::nullopt;
    const bool roomy = n + 4 <= c.budget;
    const int last = c.lines.back().who;
    const auto beatSeed = [&](std::uint64_t salt) { return mix(c.seed + std::uint64_t(n) * 7919 + salt); };
    // Turn-taking (§8.3): the one who didn't speak last first, then the other (a chatty speaker may go on).
    std::vector<int> order{partner(c, last), last};
    if (order[0] == order[1])
        order.pop_back();
    // An opinion aired is done with once nothing more is owed on it (Phase 6): its facts go.
    c.conv.erase("issue");
    c.conv.erase("issue_words");
    // Between two, one query serves both (a and b as begin had them; the speaker is their side): §16, B1.
    int side0 = 0;
    const auto now = beatQuery(c, facts, 0, 1, side0);
    // 1. A goal (frames.json): the one under way, its next step; else the first that holds for either, not pursued in
    // this talk nor done between the two lately (pair.was.goal_<name>).
    for (const int who : order)
    {
        const int to = partner(c, who);
        int side = who;
        const auto q = c.people.size() == 2 ? now : beatQuery(c, facts, who, to, side);
        for (const auto& g : goals_)
        {
            const bool underWay = c.goal == g.name;
            if (!underWay)
            {
                if (!c.goal.empty() || c.conv.count("goal_" + g.name) || !roomy ||
                    n + 2 * int(g.plan.size()) + 2 > std::min(c.budget, budgets_.most))
                    continue;
                if (const auto done = q.facts.find("pair.was.goal_" + g.name); done != q.facts.end() && done->second == "yes")
                    continue;
                if (!holdsAll(g.when, q, side))
                    continue;
            }
            else if (std::size_t(c.goalStep) >= g.plan.size())
                continue;
            Conversation::Open o;
            o.by = to;
            o.to = who;
            o.moves = {g.plan[std::size_t(underWay ? c.goalStep : 0)]};
            auto asked = q;
            asked.facts["conv.goal"] = g.name;
            auto s = answer(c, o, asked, side, fresh, trace);
            if (s.id.empty() || s.info.size() != 1)
            {
                c.conv["goal_" + g.name] = "off";          // (No line for this step: given up.)
                if (underWay)
                    c.goal.clear();
                continue;
            }
            c.goal = g.name;
            c.goalStep = underWay ? c.goalStep + 1 : 1;
            c.conv["goal_" + g.name] = "on";
            for (auto& line : s.info)
            {
                line.fit = line.best = -1;              // (It answers nothing owed: the speaker's own reason.)
                line.goal = g.name;
                line.goalDone = std::size_t(c.goalStep) >= g.plan.size();
                line.goalDays = g.days;
            }
            if (std::size_t(c.goalStep) >= g.plan.size())
            {
                c.conv["goal_" + g.name] = "done";
                c.goal.clear();
                c.goalStep = 0;
            }
            if (trace)
                trace->why += "; a goal (" + g.name + ", " + o.moves.front() + "): " + s.id;
            place(s, who, to);
            take(c, std::move(s));
            return sayNext(c);
        }
    }
    if (!roomy)
        return std::nullopt;                        // (Only a goal under way went past the budget.)
    // 1b. An opinion on the issue the topic raises (Phase 6, §8.8), by the one whose turn it is.
    if (c.people.size() == 2)
        for (const int who : order)
            if (auto line = opinion(c, now, who, fresh, trace))
                return line;
    // 2. A turn to something on either's agenda (§8.5): a thread left unfinished on it is returned to; a step along the
    // topic graph (its criteria holding now) is a pivot; anything else, now and then, a marked change of subject. A talk
    // of three keeps to its topic.
    if (c.people.size() != 2 || c.agenda.empty() || int(c.topics.size()) >= topics::book().topicsPerTalk)
        return std::nullopt;
    struct Pick
    {
        int owner = -1;
        std::size_t at = 0;
        std::string how;
    };
    const auto thread = now.facts.count("pair.thread") ? now.facts.at("pair.thread") : std::string();
    std::vector<std::pair<double, Pick>> candidates;
    for (std::size_t p = 0; p < c.agenda.size() && p < 2; ++p)
        for (std::size_t i = 0; i < c.agenda[p].size(); ++i)
        {
            const auto& item = c.agenda[p][i];
            if (item.done || item.concept.empty() || !has(item.concept))
                continue;
            const auto kind = item.facts.count("topic.kind") ? item.facts.at("topic.kind") : item.topic;
            std::string how;
            if (!thread.empty() && (thread == kind || thread == item.topic))
                how = "return";
            else if (!c.topics.empty() && item.topic == c.topics.back())
                how = "pivot";
            else if (!c.topics.empty())
                for (const auto& step : steps(c.topics.back()))
                    if (step.to == item.topic && holdsAll(step.when, now, int(p)))
                        how = "pivot";
            if (how.empty())
                how = "change_subject";
            const double rank = item.urgency + (how == "return" ? 100 : how == "pivot" ? 10 : 0);
            candidates.push_back({rank, {int(p), i, how}});
        }
    std::stable_sort(candidates.begin(), candidates.end(), [](const auto& x, const auto& y) { return x.first > y.first; });
    for (const auto& [rank, pick] : candidates)
    {
        auto& item = c.agenda[std::size_t(pick.owner)][pick.at];
        const int who = pick.owner, to = partner(c, who);
        // Now and then only (else the talk closes), as the one who'd turn it leans (Phase 6: a chatty wolf pivots more).
        const Speaker& turner = who == 0 ? now.a : now.b;
        if (pick.how == "change_subject" && unit(beatSeed(0x5B1E)) >= std::min(1.0, .5 * lean(turner, "change_subject")))
            break;
        if (pick.how == "pivot" && !temper_.empty() &&
            unit(beatSeed(0x91B0 + std::uint64_t(pick.at))) >= std::min(1.0, lean(turner, "pivot") * lean(turner, "pivot:" + item.topic)))
            continue;
        // Never what the one spoken to was told before (Phase 6, §8.6).
        if (fresh.told && !item.claim.empty())
            if (const auto& listener = to == 0 ? now.a : now.b; listener.facts.count("id") &&
                (item.kind == "tell" || item.kind == "news") && fresh.told(listener.facts.at("id"), "claim:" + item.claim))
            {
                item.done = true;
                continue;
            }
        item.done = true;
        // The talk on the new topic: its facts over the old, who knew its claim.
        const auto oldFacts = c.topicFacts;
        const auto oldKnows = c.knows;
        const auto oldTopic = c.topic;
        const auto oldClaim = c.claim;
        c.topicFacts = item.facts;
        c.knows = item.knows;
        c.claim = item.claim;
        c.topic = item.facts.count("topic.kind") ? item.facts.at("topic.kind") : item.topic;
        const int side = who;
        auto q = now;
        overlay(c, q);
        q.facts["conv.topic"] = item.topic;
        q.concept = item.concept;
        // (Never an entry or a question already said in this talk: the scorecard's C6.)
        auto said = pickLazy(q, beatSeed(0x7091), fresh.heard, fresh.recent, side, trace, [&](const Said& s) {
            return c.said.count(s.id) > 0 || collides(s, q, fresh) || std::any_of(s.info.begin(), s.info.end(), [&](const SaidLine& l) {
                       return !l.text.empty() && l.text.back() == '?' && c.asked.count(lower(l.text));
                   });
        });
        if (said.id.empty() || said.info.empty() || c.said.count(said.id) ||
            (fresh.told && find(said.id) && toldAlready(c, *find(said.id), q, side, fresh)))
        {
            c.topicFacts = oldFacts;
            c.knows = oldKnows;
            c.topic = oldTopic;
            c.claim = oldClaim;
            continue;
        }
        if (said.info.size() == 1)
            place(said, who, to);
        else
            for (auto& l : said.info)
                l.to = 1 - l.who;
        // Marked: a thread's return line, or the words of a pivot or a change of subject, in the opener's breath.
        auto& first = said.info.front();
        std::string words;
        if (pick.how == "return")
        {
            Conversation::Open o;
            o.moves = {"return"};
            if (auto back = answer(c, o, q, side, fresh, nullptr); !back.id.empty() && back.info.size() == 1)
            {
                words = back.info.front().text;
                said.chain.insert(said.chain.begin(), back.id);
                c.said.insert(back.id);
            }
        }
        else
        {
            // (The marked words in front, as another said none of them here within the hour: the collision filter.)
            const auto bare = first.text;
            bool lined = false;
            // Phase 8 (§8.5): a step along the graph is said by a line made for it where one holds (a `pivot` move line;
            // conv.from is the topic it leaves, conv.topic the one it turns to), else by #pivot#.
            if (pick.how == "pivot" && !c.topics.empty())
            {
                auto stepQ = q;
                stepQ.facts["conv.from"] = c.topics.back();
                Conversation::Open o;
                o.moves = {"pivot"};
                if (auto step = answer(c, o, stepQ, side, fresh, nullptr); !step.id.empty() && step.info.size() == 1)
                {
                    first.text = step.info.front().text + " " + bare;
                    if (!collides(said, q, fresh))
                    {
                        words = step.info.front().text;
                        said.chain.insert(said.chain.begin(), step.id);
                        c.said.insert(step.id);
                        lined = true;
                    }
                    first.text = bare;
                }
            }
            for (std::uint64_t tries = 0; !lined && tries < 5; ++tries)
            {
                words.clear();
                expand(pick.how == "pivot" ? "#pivot#" : "#anyway#", q, side, beatSeed(0x91F0 + tries * 0x51), words);
                first.text = words.empty() ? bare : words + " " + bare;
                if (!collides(said, q, fresh))
                    break;
            }
            first.text = bare;
        }
        if (!words.empty())
        {
            first.text = words + " " + first.text;
            said.lines.front().second = first.text;
        }
        first.moves.insert(first.moves.begin(), pick.how);
        c.agendaAt = item.index;
        c.topics.push_back(item.topic);
        deepen(c, q);
        if (trace)
            trace->why += "; turns to " + item.topic + " (" + pick.how + "): " + said.id;
        take(c, std::move(said));
        return sayNext(c);
    }
    return std::nullopt;
}

Said Library::answer(const Conversation& c, const Conversation::Open& owed, const Query& q, int speaker, const Freshness& fresh,
                     Trace* trace, int* bestHeld) const
{
    // The candidates (§8.2): the handed-on concept's own rules, and the rules with no concept doing an owed move (a line
    // tied to a concept answers only for it until Phase 7 says what every line does).
    std::vector<const Entry*> cands;
    std::set<const Entry*> seen;
    if (!owed.concept.empty())
        for (const auto* e : forConcept(owed.concept, q, speaker))
            if (seen.insert(e).second)
                cands.push_back(e);
    // Doc 84 Phase 9: the speaker's own lines (their mantle's, their own) beside the shared ones, while their mantle has
    // said no more than its share of this talk (§8.2: 35%; a whole scene of theirs is a body, not counted against it).
    const auto own = ownersOf(q, speaker);
    std::size_t mine = 0;
    if (!own.empty())
        for (const auto& l : c.lines)
            for (const auto* o : own)
                if (l.part != "scene" && l.entry.size() > o->owner.size() && l.entry.compare(0, o->owner.size(), o->owner) == 0 &&
                    l.entry[o->owner.size()] == '.')
                    ++mine;
    const bool share = double(mine + 1) <= .35 * double(std::max<std::size_t>(c.lines.size() + 1, 3));
    for (const auto& m : owed.moves)
    {
        if (const auto* list = byMove(m))
            for (const auto i : *list)
                if (entries_[i].concepts.empty() && seen.insert(&entries_[i]).second)
                    cands.push_back(&entries_[i]);
        if (share)
            for (const auto* o : own)
                if (const auto list = o->byMove.find(m); list != o->byMove.end())
                    for (const auto i : list->second)
                        if (o->entries[i].concepts.empty() && seen.insert(&o->entries[i]).second)
                            cands.push_back(&o->entries[i]);
    }
    // What the one answering knew before this talk (Phase 4, §8.6: `knows`, the game's, on a topic with a claim): one who
    // knew answers news with the known variant ("I heard."), its exact fit; one who didn't never says they'd heard, and
    // one who knew never says "Never!".
    const Speaker& self = speaker == 0 || q.alone ? q.a : q.b;
    const auto knowsIt = self.facts.find("knows");
    const std::string knows = knowsIt == self.facts.end() ? std::string() : knowsIt->second;
    const auto doesExactly = [&](const Entry& e, const char* move) {
        return !e.does.empty() && std::find(e.does.front().begin(), e.does.front().end(), move) != e.does.front().end();
    };
    const bool owesNews = std::any_of(owed.moves.begin(), owed.moves.end(), [&](const std::string& m) {
        return moves_.kindOf("react.news.known", m);
    });
    const auto untrue = [&](const Entry& e) {
        return (knows == "no" && (doesExactly(e, "react.news.known") || doesExactly(e, "react.news.again"))) ||
               (knows == "yes" && doesExactly(e, "react.news.new"));
    };
    const auto exact = [&](const Entry& e) {
        if (!owed.concept.empty() && e.concepts.count(owed.concept))
            return true;
        if (knows == "yes" && owesNews && doesExactly(e, "react.news.known"))
            return true;
        // The exact variant owed (react.news.known), when what is owed is a variant.
        return !e.does.empty() && std::any_of(owed.moves.begin(), owed.moves.end(), [&](const std::string& m) {
                   const auto* mv = moves_.find(m);
                   return mv && mv->name != m && std::find(e.does.front().begin(), e.does.front().end(), m) != e.does.front().end();
               });
    };
    const auto answersExtend = [&](const Entry& e) { return !e.does.empty() && hasMove(moves_, e.does.front(), "extend"); };
    const auto answers = [&](const Entry& e) {
        return !e.does.empty() && std::any_of(e.does.front().begin(), e.does.front().end(), [&](const std::string& d) {
                   return std::any_of(owed.moves.begin(), owed.moves.end(), [&](const std::string& m) { return moves_.kindOf(d, m); });
               });
    };
    std::vector<Option> options;
    int best = 0;
    double toneWeight = 1;
    // Asked as the concept handed on (its rules hold for it; a move line holds for any).
    Query asked = q;
    if (!owed.concept.empty())
        asked.concept = owed.concept;
    const bool repeats = !owed.concept.empty() && settings_.repeat.count(owed.concept) > 0;
    for (const auto* ep : cands)
    {
        const auto& e = *ep;
        // (A seed for its words: its place in the library, or its id's hash for a speaker's own.)
        const std::less<const Entry*> before;
        const std::uint64_t i = !entries_.empty() && !before(ep, entries_.data()) && before(ep, entries_.data() + entries_.size())
                                    ? std::uint64_t(ep - entries_.data()) : hash(e.id);
        if (e.scene || !fits(e, asked, speaker) || untrue(e))
            continue;                               // (A scene is a whole talk: never a later beat, in this build.)
        const int fit = exact(e) ? 3 : answers(e) ? 1 : 0;
        if (fit == 0)
            continue;
        best = std::max(best, fit);
        const auto tone = toneOf(e);
        if (c.said.count(e.id) || forbidden(c, e, asked, speaker) || toneBlocked(c, tone) ||
            (c.extended >= std::max(1, moves_.find("preclose") ? moves_.find("preclose")->maxExtend : 1) && answersExtend(e)))
            continue;
        // Phase 6: agree or disagree as the stances are (§8.8); nothing a town's mood mutes (§7.4); never told again (§8.6).
        if (!stanceFits(e, asked) || moodMuted(e, asked) || toldAlready(c, e, asked, speaker, fresh))
            continue;
        // An opinion answered with one's own only by one who differs, and once (one who agrees says so). The first to air
        // it gives it whoever they're talking to (Slice 2's review: two of a side never aired it to each other, so nobody
        // ever agreed but one with no view).
        if (c.conv.count("issue") && hasMove(moves_, e.does.front(), "give.opinion"))
        {
            const auto agree = asked.facts.find("conv.agree");
            const auto id = self.facts.find("id");
            const auto& issue = c.conv.at("issue");
            bool aired = false, gave = false;
            for (const auto& l : c.lines)
                if (hasMove(moves_, l.moves, "give.opinion") && l.issue == issue)
                {
                    aired = true;
                    gave |= id != self.facts.end() && l.who >= 0 && std::size_t(l.who) < c.people.size() &&
                            c.people[std::size_t(l.who)] == id->second;
                }
            if (gave || (aired && agree != asked.facts.end() && agree->second == "yes"))
                continue;
        }
        // Phase 8 (§33.3): once one of the realm's issues is aired, a move line's everyday view or pushback (one not
        // written for the issue: none of its criteria read the issue, the agreement or a stance) is left unsaid, so the
        // shared lines' views never speak against a speaker's own side (T4); and nor is an everyday tease or piece of
        // advice said straight back to a stance (it would answer what wasn't said: "Don't pay the first price asked." to
        // a view on the dole).
        if (e.concepts.empty())
        {
            const auto& does = e.does.front();
            const bool view = hasMove(moves_, does, "give.opinion") || hasMove(moves_, does, "counter");
            const bool reply = hasMove(moves_, does, "tease") || hasMove(moves_, does, "give.advice");
            const bool afterStance = !c.lines.empty() && !c.lines.back().issue.empty();
            if (((view && c.conv.count("issue")) || ((view || reply) && afterStance)) &&
                std::none_of(e.when.begin(), e.when.end(), [](const Criterion& k) {
                    static const std::set<std::string> Stance{"issue", "agree", "agreed_before", "stance", "stance_strength", "stance_why"};
                    return Stance.count(k.name) > 0 || k.name.rfind("op.", 0) == 0 || k.name.rfind("op_", 0) == 0;
                }))
                continue;
        }
        auto said = render(e, asked, mix(c.seed + std::uint64_t(c.beat) * 7919 + i), nullptr, speaker);
        if (said.id.empty() || said.info.empty())
            continue;
        // Never the same words twice in one talk (C6), nor the same question; nor another's words here within the hour.
        const auto& text = said.info.front().text;
        if (std::any_of(c.lines.begin(), c.lines.end(), [&](const SaidLine& l) { return l.text == text; }) ||
            (!text.empty() && text.back() == '?' && c.asked.count(lower(text))) || collides(said, asked, fresh))
            continue;
        toneWeight = 1;
        for (const auto& name : c.frames)
            for (const auto& f : frames_)
                if (f.name == name)
                    if (const auto w = f.tones.find(tone.empty() ? "even" : tone); w != f.tones.end())
                        toneWeight *= w->second;
        // How the speaker leans (§7.5: temperaments.json, a dispreferred answer's hedge too) and the town's mood (§7.4).
        double leaning = e.does.empty() || e.does.front().empty() ? 1 : lean(self, e.does.front().front());
        if (e.hedged)
            leaning = std::clamp(leaning * lean(self, "hedge"), .3, 2.0);
        options.push_back({std::move(said), &e, !repeats && fresh.heard && fresh.heard(e.id),
                           e.weight * toneWeight * leaning * moodWeight(e, asked, tone), fit});
    }
    if (bestHeld)
        *bestHeld = best;
    if (options.empty())
        return {};
    // The best fit only (coherence); tiers within it (variety: no longer only the best tier).
    int top = 0;
    for (const auto& o : options)
        top = std::max(top, o.fit);
    options.erase(std::remove_if(options.begin(), options.end(), [&](const Option& o) { return o.fit < top; }), options.end());
    // §43 (U1): at the edges of a talk of residents (a greeting, an answer, a joiner's word, the goodbyes), the speaker's
    // own line first where one fits as well as any (within its share, above) and those who'd hear it haven't heard it:
    // a wearer's one farewell isn't every goodbye they say (heard, it takes its chances with the rest).
    if (settings_.mantleFirst && c.hybrid && !own.empty())
    {
        const auto mine = [&](const Option& o) { return ownEntry(*o.entry, q, speaker) && !(fresh.heard && fresh.heard(o.entry->id)); };
        if (std::any_of(options.begin(), options.end(), mine))
            options.erase(std::remove_if(options.begin(), options.end(), [&](const Option& o) { return !mine(o); }), options.end());
    }
    const bool bestOnly = !owed.concept.empty() && settings_.bestOnly.count(owed.concept);
    const auto choice = chooseOption(options, settings_, mix(c.seed ^ (std::uint64_t(c.beat + 1) * 0x9E3779B97F4A7C15ULL)), bestOnly);
    auto& chosen = options[choice.chosen];
    for (auto& line : chosen.said.info)
    {
        line.fit = chosen.fit;
        line.best = best;
    }
    if (trace)                                      // ("held" stays the opener's candidates; the beats are in `why`.)
        trace->steps.push_back("beat " + std::to_string(c.lines.size() + 1) + ": " + chosen.entry->id + " (fit " +
                               std::to_string(chosen.fit) + ", tier " + std::to_string(choice.tier + 1) + " of " +
                               std::to_string(choice.scores.size()) + ", " + std::to_string(choice.in) + " in it)");
    return std::move(chosen.said);
}

std::set<std::string> Library::factsSaid(const Entry& e, const Said& said, const Query& q) const
{
    std::set<std::string> out;
    for (const auto& c : e.when)                    // (A flag the topic carries, tested: topic.bare=yes.)
        if (const auto f = q.facts.find("topic." + c.name); c.ns == Ns::Topic && f != q.facts.end() && f->second == "yes")
            out.insert(c.name);
    for (const auto& [k, v] : q.facts)
        if (k.rfind("topic.", 0) == 0 && k != "topic.kind" && v.size() >= 4 && v != "yes")
            for (const auto& l : said.info)
                if (lower(l.text).find(lower(v)) != std::string::npos)   // ("A dun wolf" begins a sentence.)
                {
                    out.insert(k.substr(6));
                    break;
                }
    return out;
}

Said Library::pickOne(const Query& q, std::uint64_t seed, const std::function<bool(const std::string&)>& heard,
                      const std::deque<std::string>& recent, int speaker, bool chained, Trace* trace, bool scenesFirst,
                      bool ownOnly) const
{
    const auto cands = forConcept(q.concept, q, speaker);
    if (cands.empty())
    {
        if (trace)
            trace->why = "nothing is written for " + q.concept;
        return {};
    }
    std::vector<Option> options;
    const bool repeats = settings_.repeat.count(q.concept) > 0;
    for (const auto* ep : cands)
    {
        const auto& e = *ep;
        if (std::find(recent.begin(), recent.end(), e.id) != recent.end())
        {
            if (trace)
                trace->passed.push_back(e.id + ": said here lately");
            continue;
        }
        if (e.scene && (chained || speaker != 0))
            continue;                               // (A scene is a whole talk: never a chain's later beat.)
        if (ownOnly && (!e.scene || !ownEntry(e, q, speaker)))
            continue;                               // (§43: the one spoken to's own scenes, to cast.)
        if (e.scene && untrueScene(e, q))
        {
            if (trace)
                trace->passed.push_back(e.id + ": an answer untrue to what they knew (knows)");
            continue;
        }
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
        // (§43: a scene rendered so that one turn says another's words again, the same question asked back word for
        // word, isn't said: never the same words twice in one talk, C6. Mantle-first plays the mantles' scenes far
        // more, and a writer's slip shows: "Was it really safer?" / "Was it really safer?")
        if (e.scene && std::any_of(said.info.begin(), said.info.end(), [&](const LineInfo& l) {
                return !l.text.empty() && std::count_if(said.info.begin(), said.info.end(),
                                                        [&](const LineInfo& o) { return o.text == l.text; }) > 1;
            }))
        {
            if (trace)
                trace->passed.push_back(e.id + ": one of its turns says another's words again");
            continue;
        }
        if (moodMuted(e, q))
            continue;                               // (Phase 6: muted town-wide by its mood: no teasing in grief.)
        // (Whether it may be heard again is the concept asked's, not any of the entry's: an entry that is also a weather
        // line is not fresh at every telling of gossip. Doc 84, defect 4.)
        const Speaker& by = speaker == 0 || q.alone ? q.a : q.b;
        const double leaning = temper_.empty() || e.does.empty() || e.does.front().empty() ? 1 : lean(by, e.does.front().front());
        options.push_back({std::move(said), &e, !repeats && heard && heard(e.id), e.weight * leaning, 0});
    }
    if (options.empty())
    {
        if (trace)
            trace->why = "nothing fits";
        return {};
    }
    // §37, the hybrid: a whole written scene carries the talk wherever one fits (two turns or more, as rendered for these
    // two), whatever a single line's score; a line must be said (a strict priority: a death) only where no scene of that
    // priority holds. Only where no scene fits does a rule open it, as a short exchange.
    if (scenesFirst)
    {
        // (A scene the listeners have heard is told with another of its parallel threads where it has one they haven't:
        // the same scene in other words, never the same talk twice. The game keeps "entry#alt" of a talk's first line.)
        if (heard)
            for (auto& o : options)
                if (o.entry->scene && o.heard && !o.said.info.empty() && heard(o.entry->id + "#" + std::to_string(o.said.info.front().alt)))
                    for (std::uint64_t again = 1; again <= 6; ++again)
                    {
                        auto other = render(*o.entry, q, mix(seed + again * 0xA24BAED4963EE407ULL), nullptr, speaker);
                        if (!other.id.empty() && other.info.size() >= 2 &&
                            !heard(o.entry->id + "#" + std::to_string(other.info.front().alt)))
                        {
                            o.said = std::move(other);
                            o.heard = false;        // (Heard, but not this way: as fresh as an unheard scene.)
                            break;
                        }
                    }
        int top = -1000;
        for (const auto& o : options)
            top = std::max(top, o.entry->priority);
        const auto whole = [&](const Option& o) {
            return o.entry->scene && o.said.info.size() >= 2 &&
                   (top >= settings_.strict ? o.entry->priority == top : top < 0 || o.entry->priority >= 0);
        };
        // What the director chose to have said is the topic's facts: where a rule says one of them that no scene that
        // fits does (the shelf bought bare, a price's cause), the rule's short exchange says it, not a scene of the
        // topic in general (market_behaviors: "and it's said: the shelf bought bare").
        // (A topic fact is said where its lines, as rendered, say its words: "fourteen pence the dozen", "a broken
        // axle"; or a flag the topic carries is tested. Not its kind, nor a short word that could stand for anything.)
        const auto topicFacts = [&](const Option& o) { return factsSaid(*o.entry, o.said, q); };
        // The most of the topic's facts any line that holds would say; a scene that says as much carries the talk, else
        // a rule that does, as a short exchange.
        std::size_t most = 0;
        std::vector<std::size_t> says;
        for (const auto& o : options)
        {
            const bool can = whole(o) || (!o.entry->scene && (top >= settings_.strict ? o.entry->priority == top : top < 0 || o.entry->priority >= 0));
            says.push_back(can ? topicFacts(o).size() : 0);
            most = std::max(most, says.back());
        }
        bool sceneSaysAll = false;
        for (std::size_t i = 0; i < options.size(); ++i)
            sceneSaysAll = sceneSaysAll || (whole(options[i]) && says[i] == most);
        if (!sceneSaysAll && most > 0)
        {
            std::vector<Option> kept;
            for (std::size_t i = 0; i < options.size(); ++i)
                if (!options[i].entry->scene && says[i] == most)
                    kept.push_back(std::move(options[i]));
            options = std::move(kept);
            if (trace)
                trace->why += "a rule says what no scene does; ";
        }
        else if (most > 0)
        {
            std::vector<Option> kept;
            for (std::size_t i = 0; i < options.size(); ++i)
                if (whole(options[i]) && says[i] == most)
                    kept.push_back(std::move(options[i]));
            options = std::move(kept);
        }
        else if (std::any_of(options.begin(), options.end(), whole))
            options.erase(std::remove_if(options.begin(), options.end(), [&](const Option& o) { return !whole(o); }), options.end());
        else if (trace)
            trace->why += "no whole scene fits; ";
        // §43 (U1): of what is left, the speaker's own (their mantle's) has first claim, while one of it is unheard: a
        // wearer's own scene carries the talk where one fits, before the shared library's.
        // (Unheard: neither the entry, where it may not be heard again, nor this thread of it, even where the concept
        // repeats, as the weather's do: a wearer's one weather scene isn't their every word on the weather.)
        const auto fresh = [&](const Option& o) {
            return !o.heard && !(heard && !o.said.info.empty() && heard(o.entry->id + "#" + std::to_string(o.said.info.front().alt)));
        };
        if (settings_.mantleFirst && !ownOnly)
        {
            const auto mine = [&](const Option& o) { return fresh(o) && ownEntry(*o.entry, q, speaker); };
            if (std::any_of(options.begin(), options.end(), mine))
            {
                options.erase(std::remove_if(options.begin(), options.end(), [&](const Option& o) { return !mine(o); }), options.end());
                if (trace)
                    trace->why += "the speaker's own first; ";
            }
        }
        if (ownOnly)                                // (Only an unheard whole one is cast: else the shared library's.)
            options.erase(std::remove_if(options.begin(), options.end(), [&](const Option& o) { return !whole(o) || !fresh(o); }),
                          options.end());
        if (options.empty())
            return {};
    }
    // A chain's later beats take the best tier, so its answers stay coherent (before Phase 3's fit: legacyNext).
    const auto choice = chooseOption(options, settings_, seed, chained || settings_.bestOnly.count(q.concept) > 0);
    auto& chosen = options[choice.chosen];
    if (trace)
    {
        for (const auto& o : options)
        {
            const auto t = std::size_t(std::find(choice.scores.begin(), choice.scores.end(), o.entry->score) - choice.scores.begin());
            trace->held.push_back({o.entry->id, o.entry->score, o.entry->priority, int(t), o.entry->weight, o.heard});
        }
        trace->why = choice.why + "tier " + std::to_string(choice.tier + 1) + " of " + std::to_string(choice.scores.size()) +
                     " (score " + std::to_string(choice.scores[choice.tier]) + "), " + std::to_string(choice.in) + " in it, " +
                     std::to_string(choice.fresh) + " not heard; " + chosen.entry->id + " by weight";
        render(*chosen.entry, q, seed, trace, speaker);    // (Again, for its grammar steps.)
    }
    return std::move(chosen.said);
}

Said Library::pickLazy(const Query& q, std::uint64_t seed, const std::function<bool(const std::string&)>& heard,
                       const std::deque<std::string>& recent, int speaker, Trace* trace,
                       const std::function<bool(const Said&)>& refuse) const
{
    const auto cands = forConcept(q.concept, q, speaker);
    if (cands.empty())
        return {};
    std::vector<Option> options;
    const bool repeats = settings_.repeat.count(q.concept) > 0;
    for (const auto* ep : cands)
    {
        const auto& e = *ep;
        if (std::find(recent.begin(), recent.end(), e.id) != recent.end() || (e.scene && speaker != 0) ||
            (e.scene && untrueScene(e, q)) || !fits(e, q, speaker) || moodMuted(e, q))
            continue;
        const Speaker& by = speaker == 0 || q.alone ? q.a : q.b;
        const double leaning = temper_.empty() || e.does.empty() || e.does.front().empty() ? 1 : lean(by, e.does.front().front());
        options.push_back({Said{}, &e, !repeats && heard && heard(e.id), e.weight * leaning, 0});
    }
    for (std::uint64_t tries = 0; !options.empty() && tries < 8; ++tries)
    {
        auto work = options;
        const auto choice = chooseOption(work, settings_, seed + tries * 0x9E37, settings_.bestOnly.count(q.concept) > 0);
        const auto* chosen = work[choice.chosen].entry;
        if (auto said = render(*chosen, q, seed, trace, speaker); !said.id.empty() && !(refuse && refuse(said)))
            return said;
        options.erase(std::remove_if(options.begin(), options.end(), [&](const Option& o) { return o.entry == chosen; }), options.end());
    }
    return {};
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
    byMove_.clear();
    for (std::size_t i = 0; i < entries_.size(); ++i)
    {
        byId_[entries_[i].id] = i;
        for (const auto& c : entries_[i].concepts)
            byConcept_[c].push_back(i);
        indexMoves(i);
    }
}

void Library::indexMoves(std::size_t i)
{
    for (const auto& k : movesUnder(entries_[i]))
        byMove_[k].push_back(i);
}

std::set<std::string> Library::movesUnder(Entry& e) const
{
    // A rule by its moves, a scene by its first turn's (a scene taken as a beat starts with it), each under its move and
    // every kind it is of: react.news.known under react.news.known and react.news.
    // (Phase 6: an alternative of its first turn hedges, #hedge#: a temperament weighs a hedged answer, §7.5.)
    e.hedged = !e.turns.empty() && std::any_of(e.turns.front().variants.begin(), e.turns.front().variants.end(), [](const Variant& v) {
        return std::any_of(v.texts.begin(), v.texts.end(), [](const std::string& t) { return t.find("#hedge") != std::string::npos; });
    });
    std::set<std::string> under;
    if (e.does.empty())
        return under;
    for (const auto& m : e.does.front())
        for (std::string k = m;;)
        {
            under.insert(k);
            const auto dot = k.rfind('.');
            // (A variant is under its move; a move of its own, farewell.return, is not under farewell.)
            if (dot == std::string::npos || moves_.all().count(k) || !moves_.find(k.substr(0, dot)))
                break;
            k = k.substr(0, dot);
        }
    return under;
}

const std::vector<std::size_t>* Library::byMove(const std::string& move) const
{
    const auto f = byMove_.find(move);
    return f == byMove_.end() ? nullptr : &f->second;
}

const std::set<std::string>& Library::bindSlots()
{
    static const std::set<std::string> Slots = [] {
        std::set<std::string> out{"topic.subject"};
        for (const char* who : {"self", "other"})
            for (const char* role : {"spouse", "child", "mother", "father", "neighbour", "shopkeeper", "priest", "leader"})
                out.insert(std::string(who) + "." + role);
        return out;
    }();
    return Slots;
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

// (Library::setCharacter: Core/RatwMantle.cpp, beside the mantles, since doc 84 Phase 9.)

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
        const auto layer = grammars_.find(dialectOf(from));
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

// ---------------------------------------------------------------------------------------------------------------------
// Doc 84 §7.5, §7.6, §8.8, §10.6 (Phase 6): opinions, likes, temperaments, town moods, accommodation.

static const std::set<std::string>& driftEvents()
{
    // What the game watches for (Game::talkDrift), once each: a theft from them closed unsolved or charged, the Restday
    // dole feeding them, kin of theirs charged by the Watch, a wedding of their own.
    static const std::set<std::string> Events{"robbed_uncaught", "robbed_caught", "fed_by_dole", "kin_charged", "wed"};
    return Events;
}

bool Library::readOpinions(const std::string& text, std::string& problem)
{
    // {"issues": {"watch": {"words": "the Watch", "topics": [...], "for": [{"when": "...", "why": "guard"}], "against":
    // [...], "reasons": {"guard": "..."}, "drift": {"robbed_uncaught": -1}}}, "likes": {...}, "dislikes": {...}}.
    issues_.clear();
    likes_.clear();
    json::Value doc;
    std::string error;
    if (!json::parseUnique(text, doc, error) || !doc.isObject())
    {
        problem = "opinions.json: " + (error.empty() ? "not a JSON object" : error);
        return false;
    }
    std::vector<std::string> errors;
    const auto fail = [&](const std::string& m) { errors.push_back("opinions.json: " + m); };
    const auto known = [&](const json::Value& o, const std::set<std::string>& fields, const std::string& what) {
        for (const auto& [key, unused] : o.fields())
            if (!fields.count(key))
                fail(what + ": no field '" + key + "'");
    };
    known(doc, {"about", "issues", "likes", "dislikes"}, "the file");
    const auto& book = topics::book();
    const auto criteriaOf = [&](const std::string& tokens, const std::string& what, std::vector<Criterion>& out, bool needed) {
        for (const auto& token : words(tokens))
        {
            Criterion c;
            std::string why;
            if (!criterion(token, Where::When, 0, c, why))
                fail(what + ": " + why);
            else if (c.ns != Ns::Self)
                fail(what + ": a stance or a like stands on the resident's own facts (self.*): " + token);
            else
                out.push_back(std::move(c));
        }
        if (needed && out.empty())
            fail(what + ": holds when something does");
    };
    for (const auto& [name, v] : doc.object("issues").fields())
    {
        if (name == "about")
            continue;
        Issue issue;
        issue.name = name;
        if (!v.isObject() || !lowerKey(name))
        {
            fail(name + " is an object, its name lower-case letters, digits and _");
            continue;
        }
        known(v, {"about", "words", "topics", "for", "against", "reasons", "drift", "who"}, name);
        criteriaOf(v.string("who"), name + ": who", issue.who, false);
        issue.words = v.string("words");
        if (issue.words.empty())
            fail(name + ": words (how it is spoken of: \"the Watch\")");
        for (const auto& t : v.array("topics"))
            if (book.topics.empty() || book.topics.count(t.asString("")))
                issue.topics.insert(t.asString(""));
            else
                fail(name + ": topic '" + t.asString("") + "' is not topics.json's");
        for (const auto& [k, w] : v.object("reasons").fields())
            if (!w.isString() || w.asString("").empty() || !lowerKey(k))
                fail(name + ": a reason is a key and its words");
            else
                issue.reasons[k] = w.asString("");
        for (const auto& [list, side] : {std::pair{"for", 1}, std::pair{"against", -1}})
            for (const auto& l : v.array(list))
            {
                Issue::Lean lean;
                lean.side = side;
                known(l, {"when", "why"}, name + ": " + list);
                criteriaOf(l.string("when"), name + ": " + list, lean.when, true);
                lean.reason = l.string("why");
                if (!lean.reason.empty() && !issue.reasons.count(lean.reason))
                    fail(name + ": " + list + ": why '" + lean.reason + "' is not among its reasons");
                issue.leans.push_back(std::move(lean));
            }
        for (const auto& [event, steps] : v.object("drift").fields())
        {
            const double n = steps.asNumber(0);
            if (!driftEvents().count(event))
                fail(name + ": drift: '" + event + "' is no event the game watches for");
            else if (n != double(int(n)) || n == 0 || std::abs(n) > 2)
                fail(name + ": drift: " + event + " is a step or two, for (+) or against (-)");
            else if (!issue.reasons.count(event))
                fail(name + ": drift: " + event + " needs a reason of its own (the words it gives)");
            else
                issue.drift[event] = int(n);
        }
        issues_.push_back(std::move(issue));
    }
    for (const auto& [list, dislike] : {std::pair{"likes", false}, std::pair{"dislikes", true}})
        for (const auto& [id, v] : doc.object(list).fields())
        {
            if (id == "about")
                continue;
            LikeItem item;
            item.id = id;
            item.dislike = dislike;
            if (!v.isObject() || !lowerKey(id))
            {
                fail(std::string(list) + ": " + id + " is an object, its name lower-case letters, digits and _");
                continue;
            }
            known(v, {"kind", "words", "when"}, std::string(list) + ": " + id);
            item.kind = v.string("kind");
            item.words = v.string("words");
            static const std::set<std::string> Kinds{"pastime", "food", "weather", "thing"};
            if (!Kinds.count(item.kind))
                fail(std::string(list) + ": " + id + ": kind is pastime, food, weather or thing");
            if (item.words.empty())
                fail(std::string(list) + ": " + id + ": words");
            criteriaOf(v.string("when"), std::string(list) + ": " + id, item.when, false);
            likes_.push_back(std::move(item));
        }
    if (!errors.empty())
    {
        problem.clear();
        for (std::size_t i = 0; i < errors.size() && i < 20; ++i)
            problem += (i ? "\n" : "") + errors[i];
        issues_.clear();
        likes_.clear();
        return false;
    }
    // Their facts: each issue's stance, reason and strength (a person's), and the pair's agreement on it once aired; the
    // likes' and dislikes' words.
    for (const auto& i : issues_)
    {
        person_["op." + i.name] = FactKind{FactKind::Type::Words, {"for", "against", "mixed"}};
        person_["op_why." + i.name] = FactKind{FactKind::Type::Text, {}};
        person_["op_whys." + i.name] = FactKind{FactKind::Type::Text, {}};   // (Every reason of their side: Slice 2's review.)
        person_["op_strength." + i.name] = FactKind{FactKind::Type::Number, {}};
        person_["op_drift." + i.name] = FactKind{FactKind::Type::Number, {}};
        person_["op_basis." + i.name] = FactKind{FactKind::Type::Text, {}};
        shared_["pair.agree." + i.name] = FactKind{FactKind::Type::Words, {"yes", "no", "half"}};
    }
    for (const auto& item : likes_)
        if (auto k = person_.find(item.dislike ? "dislikes" : "likes"); k != person_.end())
            k->second.values.insert(item.id);
    return true;
}

bool Library::readTemperaments(const std::string& text, std::string& problem)
{
    // {"temper=short": {"take_offence": 1.8, ...}, ...}: a trait's value and its lean on moves (or "pivot:<topic>").
    temper_.clear();
    json::Value doc;
    std::string error;
    if (!json::parseUnique(text, doc, error) || !doc.isObject())
    {
        problem = "temperaments.json: " + (error.empty() ? "not a JSON object" : error);
        return false;
    }
    std::vector<std::string> errors;
    const auto fail = [&](const std::string& m) { errors.push_back("temperaments.json: " + m); };
    if (moves_.empty())
        fail("it names moves: moves.json is needed beside it");
    for (const auto& [trait, table] : doc.object("leans").fields())
    {
        const auto eq = trait.find('=');
        const auto axis = eq == std::string::npos ? trait : trait.substr(0, eq);
        const auto value = eq == std::string::npos ? std::string() : trait.substr(eq + 1);
        const auto kind = person_.find(axis);
        if (eq == std::string::npos || kind == person_.end() || kind->second.type != FactKind::Type::Words || !kind->second.values.count(value))
        {
            fail("'" + trait + "' is not a trait and one of its values (facts.json: temper=short)");
            continue;
        }
        for (const auto& [move, w] : table.fields())
        {
            const auto colon = move.find(':');
            const auto base = colon == std::string::npos ? move : move.substr(0, colon);
            const bool topic = colon != std::string::npos && (base == "pivot" || base == "change_subject") &&
                               (topics::book().topics.empty() || topics::book().topics.count(move.substr(colon + 1)));
            if (!((moves_.known(base) && (colon == std::string::npos || topic)) || move == "hedge"))
                fail(trait + ": '" + move + "' is not a move (or pivot:<topic>, or hedge: a dispreferred answer's)");
            else if (!w.isNumber() || w.asNumber() < .3 || w.asNumber() > 2)
                fail(trait + ": " + move + ": a lean is 0.3 to 2");
            else
                temper_[trait][move] = w.asNumber();
        }
    }
    for (const auto& [key, unused] : doc.fields())
        if (key != "about" && key != "leans")
            fail("no field '" + key + "' (about, leans)");
    if (!errors.empty())
    {
        problem = errors.front();
        temper_.clear();
        temperBy_.clear();
        return false;
    }
    temperBy_.clear();
    for (const auto& [trait, table] : temper_)
        temperBy_[trait.substr(0, trait.find('='))][trait.substr(trait.find('=') + 1)] = &table;
    return true;
}

const Issue* Library::issue(const std::string& name) const
{
    for (const auto& i : issues_)
        if (i.name == name)
            return &i;
    return nullptr;
}

void Library::stances(Facts& person, const std::map<std::string, std::pair<int, std::string>>& drift) const
{
    // §8.8: a lean that holds moves the stance a step; events a step more (the ledger's drift). Its reason is the
    // strongest of its side: the event's own when an event moved it that way, else the first lean of that side. Strength
    // is how far it leans (0-3); a stance pulled both ways evenly, or not at all, is mixed.
    // (The person's facts are lent to a query and given back: no copy of them, every speaker's every 30 s: §16, B1.)
    Query q;
    q.alone = true;
    q.a.facts.swap(person);
    struct GiveBack
    {
        Facts& to;
        Facts& from;
        ~GiveBack() { to.swap(from); }
    } giveBack{person, q.a.facts};
    std::vector<std::pair<std::string, std::string>> set;
    std::vector<std::string> erased;
    for (const auto& i : issues_)
    {
        if (!holdsAll(i.who, q, 0))
        {
            for (const char* k : {"op.", "op_why.", "op_whys.", "op_strength.", "op_drift.", "op_basis."})
                erased.push_back(k + i.name);
            continue;                               // (No view on it at all: the Sump, to one who never lived by it.)
        }
        int sum = 0, pulls = 0;
        std::string forWhy, againstWhy, basis;
        std::vector<std::string> forWhys, againstWhys;   // (Every reason of each side, the strongest first: op_whys.)
        for (std::size_t li = 0; li < i.leans.size(); ++li)
            if (const auto& l = i.leans[li]; holdsAll(l.when, q, 0))
            {
                basis += (basis.empty() ? "" : ",") + std::to_string(li);
                sum += l.side;
                pulls |= l.side > 0 ? 1 : 2;
                auto& why = l.side > 0 ? forWhy : againstWhy;
                if (why.empty() && !l.reason.empty())
                    why = i.reasons.at(l.reason);
                auto& whys = l.side > 0 ? forWhys : againstWhys;
                if (!l.reason.empty() && std::find(whys.begin(), whys.end(), i.reasons.at(l.reason)) == whys.end())
                    whys.push_back(i.reasons.at(l.reason));
            }
        int moved = 0;
        if (const auto d = drift.find(i.name); d != drift.end() && d->second.first != 0)
        {
            moved = d->second.first;
            sum += moved;
            if (const auto r = i.reasons.find(d->second.second); r != i.reasons.end())
            {
                (moved > 0 ? forWhy : againstWhy) = r->second;
                auto& whys = moved > 0 ? forWhys : againstWhys;
                whys.erase(std::remove(whys.begin(), whys.end(), r->second), whys.end());
                whys.insert(whys.begin(), r->second);
            }
        }
        const char* side = sum > 0 ? "for" : sum < 0 ? "against" : "mixed";
        set.emplace_back("op." + i.name, side);
        // (Mixed and torn, pulled both ways, is a view of its own: strength 1. Mixed with nothing pulling is none: 0.)
        set.emplace_back("op_strength." + i.name, std::to_string(sum == 0 ? (pulls == 3 ? 1 : 0) : std::min(3, std::abs(sum))));
        const auto& why = sum > 0 ? forWhy : sum < 0 ? againstWhy : std::string();
        if (!why.empty())
            set.emplace_back("op_why." + i.name, why);
        else
            erased.push_back("op_why." + i.name);
        std::string whys;
        for (const auto& w : sum > 0 ? forWhys : sum < 0 ? againstWhys : std::vector<std::string>{})
            whys += (whys.empty() ? "" : "\n") + w;
        if (!whys.empty())
            set.emplace_back("op_whys." + i.name, whys);
        else
            erased.push_back("op_whys." + i.name);
        if (moved)
            set.emplace_back("op_drift." + i.name, std::to_string(moved));
        else
            erased.push_back("op_drift." + i.name);
        set.emplace_back("op_basis." + i.name, basis + "|" + std::to_string(moved));
    }
    for (const auto& k : erased)
        q.a.facts.erase(k);
    for (auto& [k, v] : set)
        q.a.facts[k] = std::move(v);
}

void Library::likes(Facts& person) const
{
    // §8.8: one like of each kind and two dislikes, drawn from the resident's id among the items that suit them: the
    // same every day (today's pastime lines drew a new favourite each time).
    if (likes_.empty())
        return;
    const auto id = person.count("id") ? person.at("id") : person.count("name") ? person.at("name") : std::string();
    if (id.empty())
        return;
    Query q;
    q.alone = true;
    q.a.facts.swap(person);                         // (Lent, and given back below: no copy.)
    std::map<std::string, std::vector<const LikeItem*>> byKind;
    std::vector<const LikeItem*> dislikes;
    for (const auto& item : likes_)
        if (holdsAll(item.when, q, 0))
            (item.dislike ? dislikes : byKind[item.kind]).push_back(&item);
    person.swap(q.a.facts);
    std::string liked, disliked;
    for (const auto& [kind, list] : byKind)
    {
        const auto* chosen = list[std::size_t(mix(hash(id + "|like|" + kind)) % list.size())];
        liked += (liked.empty() ? "" : " ") + chosen->id;
        person[kind + "_words"] = chosen->words;
    }
    for (int n = 0; n < 2 && !dislikes.empty(); ++n)
    {
        const auto at = std::size_t(mix(hash(id + "|dislike|" + std::to_string(n))) % dislikes.size());
        disliked += (disliked.empty() ? "" : " ") + dislikes[at]->id;
        if (n == 0)
        {
            person["dislike_words"] = dislikes[at]->words;
            person["dislike_kind"] = dislikes[at]->kind;
        }
        dislikes.erase(dislikes.begin() + std::ptrdiff_t(at));
    }
    if (!liked.empty())
        person["likes"] = liked;
    if (!disliked.empty())
        person["dislikes"] = disliked;
}

double Library::lean(const Speaker& s, const std::string& move) const
{
    if (temper_.empty() || move.empty())
        return 1;
    double m = 1;
    for (const auto& [axis, values] : temperBy_)
    {
        const auto f = s.facts.find(axis);
        if (f == s.facts.end())
            continue;
        const auto t = values.find(f->second);
        if (t == values.end())
            continue;
        for (const auto& [mv, w] : *t->second)
            if (mv == move || (move.size() > mv.size() && move.compare(0, mv.size(), mv) == 0 && mv.find(':') == std::string::npos &&
                               moves_.kindOf(move, mv)))
                m *= w;
    }
    return std::clamp(m, .3, 2.0);
}

std::string Library::moodOf(const Query& q) const
{
    for (const auto& m : moods_)
        if (holdsAll(m.when, q, 0))
            return m.name;
    return {};
}

void Library::accommodate(Query& q) const
{
    if (!moods_.empty())
    {
        if (const auto mood = moodOf(q); !mood.empty())
            q.facts["world.mood"] = mood;
        else
            q.facts.erase("world.mood");
    }
    if (!accent_.on || q.alone)
        return;
    // §10.6: one step broader toward kin, friends and the fond who speak as one does (code-switching home), one step
    // plainer toward one a class above, a stranger from elsewhere, a hostile pair; none rises no higher than light.
    const auto from = [](const Speaker& s) {
        const auto f = s.facts.find("from");
        return f == s.facts.end() ? std::string() : f->second;
    };
    const auto da = dialectOf(from(q.a)), db = dialectOf(from(q.b));
    q.facts["pair.same_speech"] = !from(q.a).empty() && da == db ? "yes" : "no";
    static const char* const Levels[] = {"none", "light", "broad"};
    for (int side = 0; side < 2; ++side)
    {
        auto& s = side == 0 ? q.a : q.b;
        const auto accent = s.facts.find("accent");
        if (const auto d = dialectOf(from(s)); accent == s.facts.end() || from(s).empty() || d == "plain" || !grammars_.count(d))
            continue;                               // (Plain speech, Ridgemere's: no dialect to speak more or less of.)
        const int level = accent->second == "none" ? 0 : accent->second == "broad" ? 2 : 1;
        const auto any = [&](const std::vector<std::vector<Criterion>>& rules) {
            return std::any_of(rules.begin(), rules.end(), [&](const std::vector<Criterion>& r) { return holdsAll(r, q, side); });
        };
        int step = any(accent_.plainer) ? -1 : any(accent_.broader) ? 1 : 0;
        int now = std::clamp(level + step, 0, 2);
        if (level == 0)
            now = std::min(now, accent_.noneRisesTo == "light" ? 1 : 0);
        s.facts["speaks"] = Levels[now];
    }
}

namespace
{
// Whether words were said in the talk already, whatever their first letter's case ("#self.stance_why.capitalize#").
bool saidIn(const Conversation& c, const std::string& words)
{
    if (words.size() < 2)
        return false;
    const auto tail = words.substr(1);
    for (const auto& l : c.lines)
        for (auto at = l.text.find(tail); at != std::string::npos; at = l.text.find(tail, at + 1))
            if (at > 0 && std::tolower(static_cast<unsigned char>(l.text[at - 1])) == std::tolower(static_cast<unsigned char>(words[0])))
                return true;
    return false;
}
} // namespace

void Library::stanceFacts(const Conversation& c, Query& q) const
{
    const auto issue = c.conv.find("issue");
    if (issue == c.conv.end())
        return;
    std::string sides[2];
    for (int side = 0; side < 2 && !(side == 1 && q.alone); ++side)
    {
        auto& s = side == 0 ? q.a : q.b;
        const auto at = [&](const std::string& k) {
            const auto f = s.facts.find(k + "." + issue->second);
            return f == s.facts.end() ? std::string() : f->second;
        };
        sides[side] = at("op").empty() ? "mixed" : at("op");
        s.facts["stance"] = sides[side];
        s.facts["stance_strength"] = at("op_strength").empty() ? "0" : at("op_strength");
        // Their reason: the first of their side's (op_whys, the strongest first) not said in this talk already (Slice 2's
        // review: a joiner, a disagreement or an insist said back the very sentence given two lines before). None left:
        // no reason (the lines without one are said).
        std::string why;
        const auto whys = at("op_whys");
        std::size_t from = 0;
        for (bool any = !whys.empty(); from <= whys.size();)
        {
            const auto bar = whys.find('\n', from);
            const auto one = any ? whys.substr(from, bar == std::string::npos ? std::string::npos : bar - from) : at("op_why");
            if (!one.empty() && !saidIn(c, one))
            {
                why = one;
                break;
            }
            if (!any || bar == std::string::npos)
                break;
            from = bar + 1;
        }
        if (!why.empty())
            s.facts["stance_why"] = why;
        else
            s.facts.erase("stance_why");
    }
    if (!q.alone)
        q.facts["conv.agree"] = sides[0] == sides[1] && sides[0] != "mixed" ? "yes"
                                : (sides[0] == "for" && sides[1] == "against") || (sides[0] == "against" && sides[1] == "for") ? "no"
                                                                                                                               : "half";
    if (const auto before = q.facts.find("pair.agree." + issue->second); before != q.facts.end())
        q.facts["conv.agreed_before"] = before->second;
}

bool Library::stanceFits(const Entry& e, const Query& q) const
{
    // §8.8: "disagree" fits only when the stances differ, so a disagreement is real; "agree" only when they don't.
    const auto agree = q.facts.find("conv.agree");
    if (agree == q.facts.end() || e.does.empty() || e.does.front().empty())
        return true;
    const auto& moves = e.does.front();
    const auto has = [&](const char* m) { return hasMove(moves_, moves, m); };
    if ((has("agree") || has("join.side")) && agree->second == "no")
        return false;
    if ((has("disagree") || has("counter") || has("join.correct")) && agree->second == "yes")
        return false;
    return true;
}

bool Library::toldAlready(const Conversation& c, const Entry& e, const Query& q, int speaker, const Freshness& fresh) const
{
    // §8.6, §8.12.4: nobody is told what they were told before: a claim (or one's own news) passed on to one who was told
    // it already, a story, a memory or a tease to one who heard it.
    if (!fresh.told || q.alone || e.does.empty())
        return false;
    const auto& listener = speaker == 0 ? q.b : q.a;
    const auto id = listener.facts.find("id");
    if (id == listener.facts.end() || id->second.empty())
        return false;
    for (std::size_t t = 0; t < e.does.size() && t < e.turns.size(); ++t)
    {
        const int who = e.scene ? e.turns[t].who : 0;
        if (who != 0)
            continue;                               // (The speaker's own turns: what they tell.)
        const auto& moves = e.does[t];
        const bool tells = int(t) == e.asserts || (e.asserts == -1 && q.facts.count("topic.claim_kind") &&
                                                   (hasMove(moves_, moves, "tell.news") || hasMove(moves_, moves, "tell.gossip")));
        if (tells && !c.claim.empty() && fresh.told(id->second, "claim:" + c.claim))
            return true;
        if ((hasMove(moves_, moves, "tell.story") || hasMove(moves_, moves, "tell.memory") || hasMove(moves_, moves, "tease")) &&
            fresh.told(id->second, "story:" + e.id))
            return true;
    }
    return false;
}

bool Library::collides(const Said& s, const Query& q, const Freshness& fresh)
{
    if (!fresh.collides)
        return false;
    for (const auto& l : s.info)
    {
        int n = 0;
        bool in = false;
        for (const char ch : l.text)
        {
            const bool letter = std::isalnum(static_cast<unsigned char>(ch)) || ch == '\'';
            n += letter && !in;
            in = letter;
        }
        if (n < 6)
            continue;
        const Speaker& by = l.who == 0 || q.alone ? q.a : q.b;
        const auto id = by.facts.find("id");
        if (fresh.collides(l.text, id == by.facts.end() ? std::string() : id->second))
            return true;
    }
    return false;
}

bool Library::moodMuted(const Entry& e, const Query& q) const
{
    const auto mood = q.facts.find("world.mood");
    if (mood == q.facts.end() || e.does.empty() || e.does.front().empty())
        return false;
    for (const auto& m : moods_)
        if (m.name == mood->second)
            return std::any_of(m.mutes.begin(), m.mutes.end(), [&](const std::string& x) { return hasMove(moves_, e.does.front(), x); });
    return false;
}

double Library::moodWeight(const Entry& e, const Query& q, const std::string& tone) const
{
    const auto mood = q.facts.find("world.mood");
    if (mood == q.facts.end())
        return 1;
    double w = 1;
    for (const auto& m : moods_)
        if (m.name == mood->second)
        {
            if (!e.does.empty())
                for (const auto& [move, x] : m.boosts)
                    if (hasMove(moves_, e.does.front(), move))
                        w *= x;
            if (const auto t = m.tones.find(tone.empty() ? "even" : tone); t != m.tones.end())
                w *= t->second;
        }
    return w;
}

std::optional<SaidLine> Library::opinion(Conversation& c, const Query& now, int who, const Freshness& fresh, Trace* trace) const
{
    // §8.8: on a topic an issue comes up on, one with a stance may give it (the stronger, the likelier; a trait leans on
    // it), once an issue a talk. The other answers as their own stance is: agree, disagree (friends and kin may argue it
    // out; strangers steer away), or half.
    if (issues_.empty() || c.people.size() != 2 || c.topics.empty())
        return std::nullopt;
    const auto& topic = c.topics.back();
    const Speaker& self = who == 0 ? now.a : now.b;
    const int n = int(c.lines.size());
    // (One view aired a talk: a second, straight after, from the same wolf, read as a speech. Slice 3.)
    for (const auto& [k, v] : c.conv)
        if (k.rfind("aired_", 0) == 0 && v == "yes")
            return std::nullopt;
    for (const auto& i : issues_)
    {
        if (!i.topics.count(topic) || c.conv.count("aired_" + i.name))
            continue;
        if (const auto aired = self.facts.find("was.aired_" + i.name); aired != self.facts.end() && aired->second == "yes")
            continue;                               // (Given today already: the ledger's kept tag.)
        const auto op = self.facts.find("op." + i.name);
        if (op == self.facts.end())
            continue;
        const auto strength = self.facts.find("op_strength." + i.name);
        const int s = strength == self.facts.end() ? 0 : std::atoi(strength->second.c_str());
        if (s < 1)
            continue;                               // (No view to give: nothing pulls them either way.)
        const double chance = (s >= 2 ? .45 : .25) * lean(self, "give.opinion");
        c.conv["aired_" + i.name] = "no";       // (Weighed once a talk, given or not.)
        if (unit(mix(c.seed + std::uint64_t(n) * 104729 + hash(i.name))) >= chance)
            continue;
        auto talk = c;
        talk.conv["issue"] = i.name;
        talk.conv["issue_words"] = i.words;
        auto q = now;
        q.facts["conv.issue"] = i.name;
        q.facts["conv.issue_words"] = i.words;
        stanceFacts(talk, q);
        Conversation::Open o;
        o.by = partner(c, who);
        o.to = who;
        o.moves = {"give.opinion"};
        auto said = answer(talk, o, q, who, fresh, trace);
        if (said.id.empty() || said.info.size() != 1)
            continue;
        c.conv["issue"] = i.name;
        c.conv["issue_words"] = i.words;
        c.conv["aired_" + i.name] = "yes";
        if (const auto agree = q.facts.find("conv.agree"); agree != q.facts.end())
            c.conv["agree_" + i.name] = agree->second;
        for (auto& line : said.info)
            line.fit = line.best = -1;              // (It answers nothing owed: the speaker's own view.)
        if (trace)
            trace->why += "; an opinion (" + i.name + "): " + said.id;
        place(said, who, partner(c, who));
        take(c, std::move(said));
        return sayNext(c);
    }
    return std::nullopt;
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
        {"herder", {"shepherd*", "drover*", "herd*", "stable*", "sheep", "cattle", "pig", "rabbit", "goat", "milk*", "dairy*", "warren*"}},
        {"fisher", {"fish*", "net", "eel*", "boat", "trapper*", "smokehouse*"}},
        {"sailor", {"sail*", "ship", "harbour*", "dock*", "wharf*", "quay*", "pier", "voyage*", "skipper*", "deckhand*", "boatm*"}},
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


