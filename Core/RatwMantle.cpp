// Mantles (Docs/Design/84-living-conversations.md §7.9, §9.4, §11, §17; Phase 9): the .mantle syntax read, and a mantle
// compiled into the talk library as a bucket of its own (its rules and scenes, its words as the layer m_<id>), loaded in
// one batch at start and swapped whole on mantle.sync. Also each wolf's own lines (setCharacter: a per-NPC override) in a
// bucket of their own, which ended the quadratic load of defect 16.
#include "RatwTalk.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <set>
#include <sstream>

namespace ratw::talk
{
namespace
{
std::string trim(const std::string& s)
{
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos)
        return {};
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::string rtrim(const std::string& s)
{
    const auto e = s.find_last_not_of(" \t\r\n");
    return e == std::string::npos ? std::string() : s.substr(0, e + 1);
}

std::vector<std::string> tokens(const std::string& s)
{
    std::vector<std::string> out;
    std::istringstream in(s);
    for (std::string w; in >> w;)
        out.push_back(w);
    return out;
}

std::string firstWord(const std::string& t)
{
    std::size_t n = 0;
    while (n < t.size() && !std::isspace(static_cast<unsigned char>(t[n])))
        ++n;
    return t.substr(0, n);
}

bool lowerName(const std::string& s, const char* also = "_")
{
    if (s.empty())
        return false;
    for (const char c : s)
        if (!(std::islower(static_cast<unsigned char>(c)) || std::isdigit(static_cast<unsigned char>(c)) || std::strchr(also, c)))
            return false;
    return true;
}

bool mantleId(const std::string& id)
{
    // ^[a-z]{2}[0-9]{3,4}$ (live.mantles' check)
    if (id.size() < 5 || id.size() > 6)
        return false;
    for (std::size_t i = 0; i < id.size(); ++i)
        if (i < 2 ? !std::islower(static_cast<unsigned char>(id[i])) : !std::isdigit(static_cast<unsigned char>(id[i])))
            return false;
    return true;
}

const std::set<std::string>& headerWords()
{
    static const std::set<std::string> W{"mantle", "revision", "state", "dialect", "accent", "accents", "tier", "written_for",
                                         "written_by", "title", "fits", "prefers", "traits", "quirk", "likes", "dislikes",
                                         "fears", "hopes", "topics", "styles", "marks", "persona", "format"};
    return W;
}
const std::set<std::string>& sectionWords()
{
    static const std::set<std::string> W{"words", "any", "grown", "young", "adolescent", "old"};
    return W;
}
const std::set<std::string>& itemWords()
{
    static const std::set<std::string> W{"line", "rule", "scene", "story", "opinion", "dlg", "graft"};
    return W;
}

// "@name rest" -> name, rest ("" if none). False if an @ names nothing usable.
bool atName(std::string& rest, std::string& name)
{
    rest = trim(rest);
    if (rest.empty() || rest[0] != '@')
        return true;
    const auto w = firstWord(rest);
    name = w.substr(1);
    rest = trim(rest.substr(w.size()));
    return lowerName(name);
}

// A symbol's line in `words`: [@name] symbol[.broad][!][!][criteria] (= | +=) words. Its symbol, and its @name.
bool symbolLine(const std::string& t, std::string& symbol, std::string& name)
{
    std::string rest = t;
    name.clear();
    if (!atName(rest, name))
        return false;
    std::size_t at = 0;
    while (at < rest.size() && (std::islower(static_cast<unsigned char>(rest[at])) || std::isdigit(static_cast<unsigned char>(rest[at])) || rest[at] == '_'))
        ++at;
    if (at == 0)
        return false;
    symbol = rest.substr(0, at);
    if (rest.compare(at, 6, ".broad") == 0)
        symbol += ".broad", at += 6;
    while (at < rest.size() && rest[at] == '!')
        ++at;
    if (at < rest.size() && rest[at] == '[')
    {
        const auto end = rest.find(']', at);
        if (end == std::string::npos)
            return false;
        at = end + 1;
    }
    while (at < rest.size() && rest[at] == ' ')
        ++at;
    return rest.compare(at, 1, "=") == 0 || rest.compare(at, 2, "+=") == 0;
}

// An item's line after its keyword: [@name] head [criteria] : words.
struct Split
{
    std::string name, head, criteria, text;
    bool colon = false;
};
bool splitItem(const std::string& after, Split& out)
{
    std::string rest = after;
    if (!atName(rest, out.name))
        return false;
    std::size_t at = 0;
    while (at < rest.size() && rest[at] != '[' && rest[at] != ':')
        ++at;
    out.head = trim(rest.substr(0, at));
    if (at < rest.size() && rest[at] == '[')
    {
        const auto end = rest.find(']', at);
        if (end == std::string::npos)
            return false;
        out.criteria = trim(rest.substr(at + 1, end - at - 1));
        at = end + 1;
        while (at < rest.size() && rest[at] == ' ')
            ++at;
    }
    if (at < rest.size() && rest[at] == ':')
    {
        out.colon = true;
        out.text = trim(rest.substr(at + 1));
    }
    else if (at < rest.size())
        return false;
    return true;
}

std::string joined(const std::vector<std::string>& v, const char* by = " ")
{
    std::string out;
    for (const auto& s : v)
        out += (out.empty() ? "" : by) + s;
    return out;
}

// A mantle's window line outranks its tree's own (+5). Where the tree's lines at that node report facts (the news, the
// gossip, the business, a bare shop, a warrant...) and the mantle's line tests none of them, it would hide them: "Nothing
// worth the telling" while a wagon lies robbed on the road (doc 84 §42, fix 8). So such a line yields: it holds only
// where none of the tree's fact lines do (each one's first fact, negated), its score as written. The hour, the meeting,
// the mood and the trade are how a line is said, not facts it reports; a line of the strict priority wins anyway.
// (tools/mantle_check.py's window rule is the same, so a writer sees where it applies.)
bool cosmeticFact(const Criterion& c)
{
    static const std::set<std::string> Cosmetic{"concept", "meeting", "time", "disposition", "trade", "stage"};
    return Cosmetic.count(c.name) > 0;
}

std::string factKey(const Criterion& c) { return std::to_string(int(c.ns)) + ":" + c.name; }

Criterion negated(Criterion c)
{
    using Op = Criterion::Op;
    switch (c.op)
    {
    case Op::In: c.op = Op::NotIn; break;
    case Op::NotIn: c.op = Op::In; break;
    case Op::Lt: c.op = Op::Ge; break;
    case Op::Ge: c.op = Op::Lt; break;
    case Op::Le: c.op = Op::Gt; break;
    case Op::Gt: c.op = Op::Le; break;
    case Op::Has: c.op = Op::Lacks; break;
    case Op::Lacks: c.op = Op::Has; break;
    }
    return c;
}

void yieldToTree(Entry& e, const std::vector<Entry>& shared, const std::vector<std::size_t>& tree, int strict)
{
    std::set<std::string> own, reported;
    for (const auto& c : e.when)
        own.insert(factKey(c));
    std::vector<const Entry*> gated;
    for (const auto i : tree)
    {
        const auto& t = shared[i];
        if (t.priority >= strict)
            continue;
        bool any = false;
        for (const auto& c : t.when)
            if (!cosmeticFact(c))
                reported.insert(factKey(c)), any = true;
        if (any)
            gated.push_back(&t);
    }
    for (const auto& k : reported)
        if (own.count(k))
            return;                                 // (It speaks to the facts itself: the writer's case, as written.)
    std::set<std::string> added;
    for (const auto* t : gated)
        for (const auto& c : t->when)
            if (!cosmeticFact(c))
            {
                // One fact of the line negated is enough to stand aside from it (never more than once a fact).
                const auto n = negated(c);
                const auto key = factKey(n) + ":" + std::to_string(int(n.op)) + ":" + joined(std::vector<std::string>(n.values.begin(), n.values.end()), "|") +
                                 ":" + std::to_string(n.number);
                if (added.insert(key).second)
                    e.when.push_back(n);
                break;
            }
}
} // namespace

std::string mantleLayer(const std::string& id) { return "m_" + id; }

const std::vector<std::string>& mantleSections()
{
    static const std::vector<std::string> S{"any", "grown", "young", "adolescent", "old"};
    return S;
}

bool readMantle(const std::string& text, const std::string& where, Mantle& m, std::vector<std::string>& errors)
{
    const auto before = errors.size();
    m = Mantle{};
    std::istringstream in(text);
    std::string raw;
    int number = 0;
    std::string section = "any";
    bool inWords = false, sawId = false;
    int block = -1;                                 // The item a block (rule, scene, story, graft) is adding lines to
    struct Pending
    {
        std::string prefix, symbol;                 // The key's start (grown.request), or the symbol's name
        bool fixed = false;                         // A key that is its name (story, scene), not numbered
    };
    std::vector<Pending> pending;
    const auto fail = [&](const std::string& message) { errors.push_back(where + ":" + std::to_string(number) + ": " + message); };
    const auto add = [&](const std::string& kind, const std::string& sec, const std::string& body, const std::string& name,
                         const std::string& prefix, const std::string& move) -> MantleItem& {
        MantleItem item;
        item.kind = kind;
        item.section = sec;
        item.body = body;
        item.name = name;
        item.move = move;
        item.line = number;
        m.items.push_back(item);
        pending.push_back({prefix, "", false});
        return m.items.back();
    };
    while (std::getline(in, raw))
    {
        ++number;
        raw = rtrim(raw);
        const auto t = trim(raw);
        const auto fw = firstWord(t);
        if (block >= 0)
        {
            const bool ends = t.empty() || headerWords().count(fw) || (sectionWords().count(fw) && t == fw) || itemWords().count(fw);
            if (!ends)
            {
                auto& item = m.items[std::size_t(block)];
                item.body += "\n" + raw;
                if (item.kind == "rule" && item.move.empty() && fw == "does")
                {
                    const auto moves = tokens(t.substr(4));
                    if (!moves.empty())
                        item.move = moves.front();
                }
                continue;
            }
            block = -1;
        }
        if (t.empty() || t[0] == '#')
            continue;
        if (!sawId)
        {
            sawId = true;
            if (fw != "mantle")
                fail("a mantle's text starts 'mantle <id>'");
        }
        std::string symbol, name;
        if (inWords && symbolLine(t, symbol, name))
        {
            add("symbol", "any", t, name, "words." + symbol, "");
            pending.back().symbol = symbol;
            continue;
        }
        if (sectionWords().count(fw) && t == fw)
        {
            inWords = fw == "words";
            if (!inWords)
                section = fw;
            continue;
        }
        const auto rest = trim(t.substr(fw.size()));
        if (headerWords().count(fw))
        {
            const auto list = tokens(rest);
            const auto one = [&](std::string& into) {
                if (list.size() != 1)
                    fail(fw + " is one word");
                else
                    into = list.front();
            };
            const auto number_ = [&](int& into) {
                char* end = nullptr;
                const long n = std::strtol(rest.c_str(), &end, 10);
                if (rest.empty() || *end || n < 0 || n > 1000000)
                    fail(fw + " is a whole number");
                else
                    into = int(n);
            };
            const auto pairs = [&](std::map<std::string, std::string>& into) {
                for (const auto& w : list)
                {
                    const auto eq = w.find('=');
                    if (eq == std::string::npos || eq == 0 || eq + 1 == w.size())
                        fail(fw + " is key=value ...: '" + w + "'");
                    else
                        into[w.substr(0, eq)] = w.substr(eq + 1);
                }
            };
            if (fw == "mantle")
            {
                one(m.id);
                if (!mantleId(m.id))
                    fail("a mantle's id is two letters and three or four digits (cw017), not '" + m.id + "'");
            }
            else if (fw == "revision")
                number_(m.revision);
            else if (fw == "format")
                number_(m.format);
            else if (fw == "state")
                one(m.state);
            else if (fw == "dialect")
                one(m.dialect);
            else if (fw == "tier")
                one(m.tier);
            else if (fw == "quirk")
                one(m.quirk);
            else if (fw == "written_for")
                one(m.writtenFor);
            else if (fw == "written_by")
                one(m.writtenBy);
            else if (fw == "accent" || fw == "accents")
                m.accents = list;
            else if (fw == "title")
                m.title = rest;
            else if (fw == "fits")
                m.fits = rest;
            else if (fw == "prefers")
                m.prefers = rest;
            else if (fw == "persona")
                m.persona = rest;
            else if (fw == "traits")
                pairs(m.traits);
            else if (fw == "styles")
                pairs(m.styles);
            else if (fw == "likes")
                m.likes = list;
            else if (fw == "dislikes")
                m.dislikes = list;
            else if (fw == "fears")
                m.fears = list;
            else if (fw == "hopes")
                m.hopes = list;
            else if (fw == "marks")
                m.marks = list;
            else if (fw == "topics")
            {
                for (std::size_t i = 0; i < list.size(); i += 2)
                {
                    char* end = nullptr;
                    const double w = i + 1 < list.size() ? std::strtod(list[i + 1].c_str(), &end) : 0;
                    if (i + 1 >= list.size() || *end || w < -1 || w > 1 || !lowerName(list[i]))
                        fail("topics is 'topic weight ...' (weather +.4 lore -.3)");
                    else
                        m.topics.push_back({list[i], w});
                }
            }
            continue;
        }
        if (itemWords().count(fw))
        {
            inWords = false;
            Split sp;
            if (fw == "rule" || fw == "scene" || fw == "graft" || fw == "story")
            {
                std::string after = rest, named;
                if (!atName(after, named))
                {
                    fail("@ names an item: lower-case letters, digits and _");
                    continue;
                }
                if (fw == "rule")
                {
                    if (!after.empty())
                        fail("a rule block is 'rule [@name]', its lines below it (when, does, say...)");
                    add("rule", section, t, named, "", "");
                }
                else if (fw == "scene")
                {
                    const auto n = named.empty() ? after : named;
                    if (!lowerName(n) || (!named.empty() && !after.empty()))
                        fail("a scene is 'scene <name>', its lines below it");
                    add("scene", section, t, n, section + ".scene." + n, "");
                    pending.back().fixed = true;
                }
                else if (fw == "story")
                {
                    Split st;
                    if (!splitItem(after, st) || st.colon || tokens(st.head).size() != 1 || !lowerName(st.head))
                        fail("a story is 'story <name> [criteria]', its lines below it");
                    add("story", section, t, st.head, "story." + st.head, "");
                    pending.back().fixed = true;
                }
                else
                {
                    const auto where_ = tokens(after);
                    if (where_.size() != 1 || where_.front().find('.') == std::string::npos || !lowerName(where_.front(), "_."))
                        fail("a graft is 'graft [@name] <tree>.<node>', its choices and nodes below it");
                    const auto at = where_.empty() ? std::string() : where_.front();
                    add("graft", section, t, named, "graft." + at, at);
                    pending.back().fixed = named.empty();
                }
                block = int(m.items.size()) - 1;
                continue;
            }
            if (!splitItem(rest, sp) || !sp.colon || sp.text.empty())
            {
                fail(fw + " is '" + fw + " [@name] " + (fw == "line" ? "<move>" : fw == "opinion" ? "<issue> <side> <strength>" : "<tree>.<node>") +
                     " [criteria] : words'");
                continue;
            }
            const auto head = tokens(sp.head);
            if (fw == "line")
            {
                if (head.empty() || !std::all_of(head.begin(), head.end(), [](const std::string& h) { return lowerName(h, "_."); }))
                {
                    fail("a line names the move it makes: line [@name] <move> [criteria] : words");
                    continue;
                }
                add("line", section, t, sp.name, section + "." + head.front(), head.front());
            }
            else if (fw == "opinion")
            {
                if (head.size() != 3 || !lowerName(head[0]))
                {
                    fail("an opinion is 'opinion <issue> <for|against|mixed> <0-3> [criteria] : words'");
                    continue;
                }
                add("opinion", section, t, sp.name, "opinion." + head[0], "");
            }
            else
            {
                if (head.size() != 1 || head[0].find('.') == std::string::npos || !lowerName(head[0], "_."))
                {
                    fail("a window line is 'dlg [@name] <tree>.<node> [criteria] : words'");
                    continue;
                }
                add("dlg", section, t, sp.name, "dlg." + head[0], head[0]);
            }
            continue;
        }
        fail("expected a header line, a section (words, grown, young, adolescent, old) or an item (line, rule, scene, story, "
             "opinion, dlg, graft), not '" + fw + "'" + (inWords ? "" : " (a symbol's line belongs under words)"));
    }
    if (!sawId || m.id.empty())
        errors.push_back(where + ": no 'mantle <id>' line");
    static const std::set<std::string> States{"draft", "ready", "retired"}, Tiers{"full", "standard", "spare"},
        Accents{"none", "light", "broad"};
    if (!States.count(m.state))
        errors.push_back(where + ": state is draft, ready or retired, not '" + m.state + "'");
    if (!Tiers.count(m.tier))
        errors.push_back(where + ": tier is full, standard or spare, not '" + m.tier + "'");
    if (m.accents.empty() || m.accents.size() > 3 ||
        !std::all_of(m.accents.begin(), m.accents.end(), [](const std::string& a) { return Accents.count(a) > 0; }))
        errors.push_back(where + ": accent is one to three of none, light and broad (the first as written)");
    if (!lowerName(m.dialect))
        errors.push_back(where + ": dialect is a grammar's name");
    if (!m.quirk.empty() && !lowerName(m.quirk))
        errors.push_back(where + ": a quirk is lower-case words joined by _");
    if (m.persona.size() > 4000 || m.title.size() > 80 || m.fits.size() > 400 || m.prefers.size() > 400)
        errors.push_back(where + ": the persona, title, fits or prefers is too long (4,000, 80, 400, 400 characters)");
    // The keys: a named item's own; the rest numbered, the next free number of their kind (a symbol's first is words.<symbol>).
    std::set<std::string> taken;
    for (std::size_t i = 0; i < m.items.size(); ++i)
    {
        auto& item = m.items[i];
        auto& p = pending[i];
        if (item.kind == "rule" && p.prefix.empty())
            p.prefix = item.section + "." + (item.move.empty() ? std::string("rule") : item.move);
        if (p.fixed)
            item.key = p.prefix;
        else if (!item.name.empty())
            item.key = p.prefix + "." + item.name;
        else
            continue;
        if (!taken.insert(item.key).second)
            errors.push_back(where + ":" + std::to_string(item.line) + ": " + item.key + " is there twice (give one an @name)");
    }
    for (std::size_t i = 0; i < m.items.size(); ++i)
    {
        auto& item = m.items[i];
        if (!item.key.empty())
            continue;
        const auto& prefix = pending[i].prefix;
        if (item.kind == "symbol" && taken.insert(prefix).second)
        {
            item.key = prefix;
            continue;
        }
        for (int n = item.kind == "symbol" ? 2 : 1;; ++n)
            if (taken.insert(prefix + "." + std::to_string(n)).second)
            {
                item.key = prefix + "." + std::to_string(n);
                break;
            }
    }
    std::map<std::string, int> at;
    for (auto& item : m.items)
    {
        item.position = at[item.section]++;
        if (item.key.size() > 120 || !lowerName(item.key, "_.:"))
            errors.push_back(where + ":" + std::to_string(item.line) + ": the key " + item.key +
                             " is lower-case letters, digits, _ . and :, at most 120");
    }
    return errors.size() == before;
}

std::string mantleText(const Mantle& c, std::vector<MantleItem> items)
{
    // The header as the export writes it (tools/mantle_text.py: the same order), then the items by section and position.
    std::string out = "mantle " + c.id + "\n";
    if (c.revision > 0)
        out += "revision " + std::to_string(c.revision) + "\n";
    out += "state " + c.state + "\n";
    out += "dialect " + c.dialect + "\n";
    out += "accent " + joined(c.accents) + "\n";
    out += "tier " + c.tier + "\n";
    const auto line = [&](const char* key, const std::string& value) {
        if (!value.empty())
            out += std::string(key) + " " + value + "\n";
    };
    line("written_for", c.writtenFor);
    line("written_by", c.writtenBy);
    line("title", c.title);
    line("fits", c.fits);
    line("prefers", c.prefers);
    std::vector<std::string> pairs;
    for (const auto& [k, v] : c.traits)
        pairs.push_back(k + "=" + v);
    line("traits", joined(pairs));
    line("quirk", c.quirk);
    line("likes", joined(c.likes));
    line("dislikes", joined(c.dislikes));
    line("fears", joined(c.fears));
    line("hopes", joined(c.hopes));
    std::string topics;
    for (const auto& [t, w] : c.topics)
    {
        std::ostringstream n;
        n << (w >= 0 ? "+" : "") << w;
        topics += (topics.empty() ? "" : " ") + t + " " + n.str();
    }
    line("topics", topics);
    pairs.clear();
    for (const auto& [k, v] : c.styles)
        pairs.push_back(k + "=" + v);
    line("styles", joined(pairs));
    line("marks", joined(c.marks));
    line("persona", c.persona);
    if (c.format != 1)
        out += "format " + std::to_string(c.format) + "\n";
    const auto& order = mantleSections();
    const auto rank = [&](const MantleItem& i) {
        return i.kind == "symbol" ? -1 : int(std::find(order.begin(), order.end(), i.section) - order.begin());
    };
    std::stable_sort(items.begin(), items.end(), [&](const MantleItem& a, const MantleItem& b) {
        return rank(a) != rank(b) ? rank(a) < rank(b) : a.position < b.position;
    });
    int was = -2;
    for (const auto& i : items)
    {
        const int r = rank(i);
        if (r != was)
        {
            out += "\n" + (r < 0 ? std::string("words") : order[std::size_t(r)]) + "\n";
            was = r;
        }
        out += i.body + "\n";
        if (i.body.find('\n') != std::string::npos || i.kind == "rule" || i.kind == "scene" || i.kind == "story" || i.kind == "graft")
            out += "\n";                            // (A block ends at a blank line.)
    }
    return out;
}

// ------------------------------------------------------------------------------------------------- The library's side

std::vector<const Library::Owned*> Library::ownersOf(const Query& q, int speaker) const
{
    std::vector<const Owned*> out;
    if (owned_.empty())
        return out;
    const Speaker& s = speaker == 0 || q.alone ? q.a : q.b;
    if (const auto m = s.facts.find("mantle"); m != s.facts.end() && !m->second.empty())
        if (const auto o = owned_.find("m:" + m->second); o != owned_.end())
            out.push_back(o->second.get());
    if (const auto id = s.facts.find("id"); id != s.facts.end() && !id->second.empty())
        if (const auto o = owned_.find("p:" + id->second); o != owned_.end())
            out.push_back(o->second.get());
    return out;
}

std::vector<const Entry*> Library::forConcept(const std::string& concept, const Query& q, int speaker) const
{
    std::vector<const Entry*> out;
    if (const auto list = byConcept_.find(concept); list != byConcept_.end())
    {
        out.reserve(list->second.size());
        for (const auto i : list->second)
            out.push_back(&entries_[i]);
    }
    for (const auto* o : ownersOf(q, speaker))
        if (const auto list = o->byConcept.find(concept); list != o->byConcept.end())
            for (const auto i : list->second)
                out.push_back(&o->entries[i]);
    return out;
}

const Library::Grammar* Library::layerOf(const std::string& name) const
{
    if (const auto g = grammars_.find(name); g != grammars_.end())
        return &g->second;
    if (name.size() > 2 && name[0] == 'm' && name[1] == '_')
        if (const auto o = owned_.find("m:" + name.substr(2)); o != owned_.end())
            return &o->second->grammar;
    return nullptr;
}

void Library::indexOwned(Owned& o, const Library& lib)
{
    o.byConcept.clear();
    o.byMove.clear();
    o.byId.clear();
    for (std::size_t i = 0; i < o.entries.size(); ++i)
    {
        o.byId[o.entries[i].id] = i;
        for (const auto& c : o.entries[i].concepts)
            o.byConcept[c].push_back(i);
        for (const auto& k : lib.movesUnder(o.entries[i]))
            o.byMove[k].push_back(i);
    }
}

bool Library::parseOwn(const std::string& text, const std::string& file, std::vector<Entry>& out, std::string& problem) const
{
    // (Library::parse with parseInto_ set changes nothing of the library's own: its entries go to `got`.)
    std::vector<Entry> got;
    parseInto_ = &got;
    const bool ok = const_cast<Library*>(this)->parse(text, file, problem);
    parseInto_ = nullptr;
    if (ok)
        for (auto& e : got)
            out.push_back(std::move(e));
    return ok;
}

bool Library::setCharacter(const std::string& npc, const std::string& text, std::string& problem)
{
    const auto file = "voice:" + npc;
    const auto key = "p:" + npc;
    if (trim(text).empty())
    {
        owned_.erase(key);
        return true;
    }
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
    auto o = std::make_shared<Owned>();
    o->owner = "@" + npc;
    if (!parseOwn(own, file, o->entries, problem))
        return false;
    for (auto& e : o->entries)
    {
        Criterion c;
        c.ns = Ns::Self;
        c.name = "id";
        c.values = {npc};
        e.when.insert(e.when.begin(), c);
        e.score += settings_.namedPerson;
    }
    indexOwned(*o, *this);
    owned_[key] = std::move(o);
    return true;
}

bool Library::compileMantle(Mantle& m, Owned& out, std::vector<std::string>& errors) const
{
    const auto before = errors.size();
    const auto fail = [&](const std::string& key, const std::string& why) {
        errors.push_back(m.id + (key.empty() ? std::string() : " " + key) + ": " + why);
    };
    // The card.
    if (m.dialect != "plain" && !grammars_.count(m.dialect))
        fail("", "dialect '" + m.dialect + "' is not a grammar of the library (plain, or a dialect)");
    for (const auto& [axis, value] : m.traits)
    {
        const auto f = person_.find(axis);
        static const std::set<std::string> Axes{"temper", "talk", "warmth", "humour", "honesty", "piety", "drive"};
        if (!Axes.count(axis) || f == person_.end())
            fail("traits", "'" + axis + "' is not a trait (temper, talk, warmth, humour, honesty, piety, drive)");
        else if (!f->second.values.empty() && !f->second.values.count(value))
            fail("traits", axis + "=" + value + " is not one of its values");
    }
    for (const auto* list : {&m.likes, &m.dislikes, &m.fears, &m.hopes, &m.marks})
        for (const auto& w : *list)
            if (!lowerName(w))
                fail("card", "'" + w + "' is lower-case words joined by _");
    for (const auto& [topic, w] : m.topics)
        if (!settings_.concepts.count("talk." + topic))
            fail("topics", "'" + topic + "' is not a topic (talk." + topic + " in settings.json)");
    for (const auto& [k, v] : m.styles)
        if (!lowerName(k) || !lowerName(v))
            fail("styles", k + "=" + v + ": lower-case words");
    const auto criteria = [&](const std::string& text, const std::string& key, std::vector<std::string>* out_) {
        for (const auto& token : tokens(text))
        {
            Criterion c;
            std::string error;
            const bool bare = token.find_first_of("=<>!") == std::string::npos;
            if (bare ? !bareWord(token, 0, Where::When, c) : !criterion(token, Where::When, 0, c, error))
                fail(key, error.empty() ? "'" + token + "' is not a fact" : error);
            else if (out_)
                out_->push_back(token);
        }
    };
    criteria(m.fits, "fits", nullptr);
    criteria(m.prefers, "prefers", nullptr);

    // The words: its own grammar layer.
    const auto layerName = mantleLayer(m.id);
    std::string words = "grammar " + layerName + "\n";
    std::vector<std::string> keyAt{"", ""};
    for (const auto& item : m.items)
        if (item.kind == "symbol")
        {
            auto body = item.body;
            std::string unused;
            atName(body, unused);
            words += body + "\n";
            keyAt.push_back(item.key);
        }
    std::map<std::string, Grammar> layers;
    std::vector<GrammarLine> lines;
    std::vector<std::string> wordErrors;
    const auto file = "mantle:" + m.id;
    // (Its errors name the item: "cw017 words.oath: ...".)
    const auto named = [&](std::vector<std::string>& found) {
        for (auto& e : found)
            if (e.rfind(file + ":", 0) == 0)
            {
                const auto rest = e.substr(file.size() + 1);
                const auto colon = rest.find(':');
                const auto n = std::atoi(rest.substr(0, colon).c_str());
                const auto key = n >= 0 && std::size_t(n) < keyAt.size() ? keyAt[std::size_t(n)] : std::string();
                errors.push_back(m.id + " " + key + rest.substr(colon == std::string::npos ? rest.size() : colon));
            }
            else
                errors.push_back(m.id + ": " + e);
        found.clear();
    };
    readGrammarText(words, file, layers, lines, wordErrors);
    out.grammar = std::move(layers[layerName]);
    checking_ = &out.grammar;
    for (const auto& l : lines)
        for (const auto& t : l.texts)
            checkText(t, wordErrors, l.where, nullptr);
    {
        std::map<std::string, std::size_t> memo;
        for (const auto& [name, symbol] : out.grammar)
        {
            std::set<std::string> open;
            std::string error;
            if (longestSymbol(name, memo, open, 1, error) > 160)
                wordErrors.push_back(m.id + " words." + name + ": can say more than 160 characters");
            if (!error.empty())
                wordErrors.push_back(m.id + " words." + name + ": " + error);
        }
    }
    named(wordErrors);
    for (const auto& mark : m.marks)
        if (!out.grammar.count(mark))
            fail("marks", "'" + mark + "' is not one of its words (a mark names a habit it defines)");

    // The items, each a rule or scene of its own (so a problem names its item).
    const auto has = [&](const char* section) {
        return std::any_of(m.items.begin(), m.items.end(), [&](const MantleItem& i) { return i.section == section && i.kind != "symbol"; });
    };
    // (Phase 11, the format frozen: `grown` is the adult and the old always, and an `old` section adds the old's own on
    // top. Before, an `old` section took the old out of `grown`, so an old wearer whose writer put most of the mantle
    // in `grown` heard almost none of it: G3 found three such pilot mantles. Pups' sections still part: a young section
    // is the young's alone when an adolescent one exists.)
    const std::map<std::string, std::string> stage{
        {"any", ""},
        {"grown", "self.stage=adult|old"},
        {"young", has("adolescent") ? "self.stage=young" : "self.stage=young|adolescent"},
        {"adolescent", "self.stage=adolescent"},
        {"old", "self.stage=old"}};
    mantleParse_ = true;
    std::string yieldAt;
    for (const auto& item : m.items)
    {
        yieldAt.clear();
        if (item.kind == "symbol")
            continue;
        const auto id = m.id + "." + item.key;
        const auto& st = stage.at(item.section);
        const auto when = [&](const std::string& more) {
            const auto all = trim(more + " " + st);
            return all.empty() ? std::string() : "when " + all + "\n";
        };
        std::string src;
        const auto first = item.body.substr(0, item.body.find('\n'));
        const auto kw = firstWord(first);
        Split sp;
        if (item.kind == "line" || item.kind == "opinion" || item.kind == "dlg")
            splitItem(trim(first.substr(kw.size())), sp);
        if (item.kind == "line")
            src = "rule " + id + "\n" + when(sp.criteria) + "does " + sp.head + "\nsay " + sp.text + "\n";
        else if (item.kind == "opinion")
        {
            const auto head = tokens(sp.head);
            static const std::set<std::string> Sides{"for", "against", "mixed"};
            const int strength = head.size() == 3 ? std::atoi(head[2].c_str()) : -1;
            if (head.size() != 3 || !issue(head[0]))
            {
                fail(item.key, "'" + (head.empty() ? std::string() : head[0]) + "' is not an issue (opinions.json)");
                continue;
            }
            if (!Sides.count(head[1]) || head[2].size() != 1 || strength < 0 || strength > 3)
            {
                fail(item.key, "an opinion is <issue> <for|against|mixed> <0-3>");
                continue;
            }
            if (sp.criteria.empty() && !m.stances.count(head[0]))
                m.stances[head[0]] = {head[1], strength};
            src = "rule " + id + "\n" + when("conv.issue=" + head[0] + " " + sp.criteria) + "does give.opinion\nsay " + sp.text + "\n";
        }
        else if (item.kind == "dlg")
        {
            const auto dot = sp.head.find('.');
            const auto* tree_ = tree(sp.head.substr(0, dot));
            if (!tree_ || !tree_->nodes.count(sp.head.substr(dot + 1)))
            {
                fail(item.key, "no window tree has the node " + sp.head);
                continue;
            }
            src = "rule " + id + "\n" + when("concept=dlg." + sp.head + " " + sp.criteria) + "say " + sp.text + "\n";
            yieldAt = "dlg." + sp.head;
        }
        else if (item.kind == "rule" || item.kind == "scene")
        {
            const auto nl = item.body.find('\n');
            src = item.kind + " " + id + "\n" + when("") + (nl == std::string::npos ? std::string() : item.body.substr(nl + 1)) + "\n";
        }
        else if (item.kind == "story")
        {
            // A whole scene the wearer tells (doc 84 §37: written scenes carry the talk): `:` a beat of theirs, `ask :` a
            // question of theirs, `b :` the listener's answer, written whole. Told once to each listener (tell.story).
            Split head;
            splitItem(trim(first.substr(kw.size())), head);
            std::string criteria = head.criteria;
            bool hasConcept = false;
            for (const auto& t : tokens(criteria))
                hasConcept = hasConcept || t.rfind("concept=", 0) == 0 || t.rfind("topic=", 0) == 0;
            if (!hasConcept)
                criteria = "concept=talk.memories " + criteria;
            std::istringstream in(item.body);
            std::string raw;
            std::getline(in, raw);
            std::vector<std::string> turns, out_;
            char was = 0;
            bool open = false, asked = false, bSpoke = false;
            while (std::getline(in, raw))
            {
                const auto t = trim(raw);
                if (t.empty() || t[0] == '#')
                    continue;
                std::string speaker, line;
                bool ask = false;
                if (t[0] == ':')
                    speaker = "a", line = "a " + t;
                else if (t.rfind("ask", 0) == 0 && trim(t.substr(3)).rfind(":", 0) == 0)
                    speaker = "a", ask = true, line = "a " + trim(t.substr(3));
                else if ((t[0] == 'a' || t[0] == 'b') && t.size() > 1 && (t[1] == ' ' || t[1] == '[' || t[1] == '?' || t[1] == ':'))
                    speaker = std::string(1, t[0]), line = t;
                else
                {
                    fail(item.key, "a story's lines are ': words' (the teller's), 'ask : words', or 'b : words' (the listener's)");
                    continue;
                }
                const auto colon = line.find(':');
                const bool tagged = line.find('[') != std::string::npos && line.find('[') < colon;
                const char who = speaker[0];
                const bool same = open && who == was;
                if (!same)
                {
                    const auto move = who == 'a' ? (ask ? "ask.detail" : "tell.story") : (asked ? "answer.detail" : "react.story");
                    turns.push_back(move);
                    asked = who == 'a' && ask;
                }
                bSpoke = bSpoke || who == 'b';
                open = tagged;
                was = who;
                out_.push_back(line);
            }
            if (!bSpoke)
            {
                fail(item.key, "a story is a whole scene: give the listener a line ('b : ...')");
                continue;
            }
            src = "scene " + id + "\n" + when(criteria) + "does " + joined(turns, " / ") + "\n" + joined(out_, "\n") + "\n";
        }
        else if (item.kind == "graft")
        {
            MantleGraft g;
            g.key = item.key;
            const auto at = item.move;
            const auto dot = at.find('.');
            g.tree = at.substr(0, dot);
            g.node = dot == std::string::npos ? std::string() : at.substr(dot + 1);
            const auto* tree_ = tree(g.tree);
            if (!tree_ || !tree_->nodes.count(g.node))
            {
                fail(item.key, "no window tree has the node " + at);
                continue;
            }
            std::istringstream in(item.body);
            std::string raw;
            std::getline(in, raw);
            std::string from;                       // The graft node being written ("": the tree's own node)
            std::map<std::string, std::vector<std::pair<std::string, std::string>>> says;   // node -> (criteria, words)
            bool bad = false;
            while (std::getline(in, raw))
            {
                const auto t = trim(raw);
                if (t.empty() || t[0] == '#')
                    continue;
                const auto w = firstWord(t);
                const auto rest = trim(t.substr(w.size()));
                if (w == "choice")
                {
                    const auto arrow = rest.rfind("->");
                    if (arrow == std::string::npos)
                    {
                        fail(item.key, "a choice is 'choice <words> [criteria] -> <node|bye|end>'");
                        bad = true;
                        continue;
                    }
                    MantleGraft::Choice c;
                    c.from = from;
                    c.to = trim(rest.substr(arrow + 2));
                    auto words_ = trim(rest.substr(0, arrow));
                    if (!words_.empty() && words_.back() == ']')
                        if (const auto open_ = words_.rfind('['); open_ != std::string::npos)
                        {
                            c.when = trim(words_.substr(open_ + 1, words_.size() - open_ - 2));
                            words_ = trim(words_.substr(0, open_));
                            criteria(c.when, item.key, nullptr);
                        }
                    c.words = words_;
                    if (c.words.empty())
                        fail(item.key, "a choice has the player's words");
                    g.choices.push_back(c);
                }
                else if (w == "node")
                {
                    if (!lowerName(rest) || tree_->nodes.count(rest) ||
                        std::any_of(g.nodes.begin(), g.nodes.end(), [&](const MantleGraft::Node& n) { return n.name == rest; }))
                    {
                        fail(item.key, "a graft's node is a new name (lower-case, not one of the tree's): '" + rest + "'");
                        bad = true;
                    }
                    from = rest;
                    g.nodes.push_back({rest, "graft." + g.tree + "." + m.id + "." + rest, "", "", false});
                }
                else if (w == "say" || w.rfind("say[", 0) == 0)
                {
                    if (from.empty())
                    {
                        fail(item.key, "a graft's say belongs to one of its nodes (node <name> first)");
                        continue;
                    }
                    std::string crit, words_ = rest;
                    if (w != "say")
                    {
                        const auto close = t.find(']');
                        crit = close == std::string::npos ? std::string() : t.substr(4, close - 4);
                        words_ = close == std::string::npos ? std::string() : trim(t.substr(close + 1));
                    }
                    says[from].push_back({crit, words_});
                }
                else if (w == "mood" || w == "free" || w == "invite")
                {
                    if (from.empty())
                    {
                        fail(item.key, w + " belongs to one of the graft's nodes");
                        continue;
                    }
                    auto& node = g.nodes.back();
                    if (w == "free")
                        node.free = true;
                    else if (w == "mood")
                    {
                        node.mood = rest;
                        if (!faceMoods().count(rest))
                            fail(item.key, "'" + rest + "' is not one of the face's moods");
                    }
                    else
                    {
                        node.invite = rest;
                        if (tokens(rest).size() != 3)
                            fail(item.key, "an invitation is 'invite <place> <hour> <days>'");
                    }
                }
                else
                {
                    fail(item.key, "a graft holds choice, node, say, mood, free and invite lines, not '" + w + "'");
                    bad = true;
                }
            }
            if (bad)
                continue;
            // Every choice leads somewhere; every node is reached, has words, and has a way back to the tree, bye or end.
            std::set<std::string> own;
            for (const auto& n : g.nodes)
                own.insert(n.name);
            for (const auto& c : g.choices)
                if (!own.count(c.to) && !tree_->nodes.count(c.to) && c.to != "bye" && c.to != "end")
                    fail(item.key, "a choice leads to '" + c.to + "', which is neither its node nor the tree's, bye or end");
            if (std::none_of(g.choices.begin(), g.choices.end(), [](const MantleGraft::Choice& c) { return c.from.empty(); }))
                fail(item.key, "a graft adds at least one choice to " + at + " (before its first node)");
            std::set<std::string> reached;
            for (bool grew = true; grew;)
            {
                grew = false;
                for (const auto& c : g.choices)
                    if ((c.from.empty() || reached.count(c.from)) && own.count(c.to) && reached.insert(c.to).second)
                        grew = true;
            }
            std::set<std::string> out__;            // Nodes with a way out
            for (bool grew = true; grew;)
            {
                grew = false;
                for (const auto& c : g.choices)
                    if (!c.from.empty() && !out__.count(c.from) && (!own.count(c.to) || out__.count(c.to)))
                        grew = out__.insert(c.from).second || grew;
            }
            for (const auto& n : g.nodes)
            {
                if (!reached.count(n.name))
                    fail(item.key, "its node " + n.name + " is never reached");
                if (!out__.count(n.name))
                    fail(item.key, "its node " + n.name + " has no way back to the tree, bye or end");
                if (says[n.name].empty())
                    fail(item.key, "its node " + n.name + " says nothing");
                int k = 0;
                for (const auto& [crit, words_] : says[n.name])
                    src += "rule " + id + "." + n.name + "." + std::to_string(++k) + "\n" + when("concept=" + n.concept + " " + crit) +
                           "say " + words_ + "\n\n";
            }
            m.grafts.push_back(std::move(g));
        }
        if (src.empty())
            continue;
        std::vector<Entry> got;
        std::string problem;
        if (!parseOwn(src, m.id + " " + item.key, got, problem))
        {
            // ("cw017 grown.greet.1:3: ...": the line is the compiled item's, no help to its writer; the key says where.)
            const auto file = m.id + " " + item.key;
            std::istringstream lines_(problem);
            for (std::string l; std::getline(lines_, l);)
            {
                if (l.rfind(file + ":", 0) == 0)
                    if (const auto colon = l.find(':', file.size() + 1); colon != std::string::npos)
                        l = file + l.substr(colon);
                errors.push_back(l);
            }
            continue;
        }
        for (auto& e : got)
        {
            if (!yieldAt.empty())
                if (const auto at = byConcept_.find(yieldAt); at != byConcept_.end())
                    yieldToTree(e, entries_, at->second, settings_.strict);
            e.owner = m.id;
            e.file = m.id;                          // (B4: a short name, kept in the string itself; the key is in its id.)
            Criterion c;
            c.ns = Ns::Self;
            c.name = "mantle";
            c.values = {m.id};
            e.when.insert(e.when.begin(), c);
            e.score += settings_.namedPerson;
            out.entries.push_back(std::move(e));
        }
    }
    mantleParse_ = false;
    checking_ = nullptr;

    // What its lines owe has an answer (§7.2's dead-end proof, for this mantle): a shared line that holds with nothing
    // known, or one of its own, doing the move or one of its fallbacks.
    if (!moves_.empty())
    {
        if (plain_.size() != entries_.size())
            plain_.assign(entries_.size(), 2);
        const auto ok = [&](std::size_t i) {
            if (plain_[i] == 2)
                plain_[i] = unconditioned(entries_[i]) ? 1 : 0;
            return plain_[i] == 1;
        };
        std::map<std::string, std::string> owed;
        for (const auto& e : out.entries)
        {
            for (const auto& o : e.owes)
                if (!o.soft)
                    owed.emplace(joined(o.moves, "|"), e.id);
            if (!e.scene)
                for (const auto& turn : e.does)
                    for (const auto& mv : turn)
                        if (const auto* move = moves_.find(mv); move && !move->optional && !move->expects.empty())
                            owed.emplace(joined(move->expects, "|"), e.id);
        }
        for (const auto& [key, by] : owed)
        {
            bool answered = false;
            std::string alt;
            std::istringstream alts(key);
            while (!answered && std::getline(alts, alt, '|'))
                for (const auto& mv : moves_.chain(alt))
                {
                    if (const auto* list = byMove(mv); list && std::any_of(list->begin(), list->end(), ok))
                        answered = true;
                    for (const auto& e : out.entries)
                        answered = answered || (!e.does.empty() && std::find(e.does.front().begin(), e.does.front().end(), mv) != e.does.front().end());
                }
            if (!answered)
                errors.push_back(by + ": owes " + key + ", and no line (shared, or its own) answers it");
        }
    }
    m.entries = out.entries.size();
    return errors.size() == before;
}

bool Library::loadMantle(const MantleText& text, Mantle& card, Owned& out, std::string& problem, bool anyState) const
{
    std::vector<std::string> errors;
    const auto where = !text.where.empty() ? text.where : !text.id.empty() ? text.id : std::string("mantle");
    if (readMantle(text.text, where, card, errors))
    {
        if (!text.id.empty() && card.id != text.id)
            errors.push_back(where + ": the text is mantle " + card.id + ", not " + text.id);
        if (!anyState && card.state != "ready")
            errors.push_back(where + ": " + card.id + " is " + card.state + ": only a ready mantle is worn");
        if (errors.empty())
            compileMantle(card, out, errors);
    }
    out.owner = card.id;
    indexOwned(out, *this);
    problem.clear();
    for (std::size_t i = 0; i < errors.size() && i < 30; ++i)
        problem += (i ? "\n" : "") + errors[i];
    if (errors.size() > 30)
        problem += "\n(" + std::to_string(errors.size() - 30) + " more)";
    return errors.empty();
}

bool Library::setMantle(const MantleText& text, std::string& problem, std::string* id, bool anyState)
{
    auto card = std::make_shared<Mantle>();
    auto o = std::make_shared<Owned>();
    const bool ok = loadMantle(text, *card, *o, problem, anyState);
    if (id)
        *id = card->id;
    if (!ok)
        return false;
    o->card = card;
    owned_["m:" + card->id] = std::move(o);
    return true;
}

std::size_t Library::setMantles(const std::vector<MantleText>& texts, std::vector<MantleRefusal>& refused)
{
    std::map<std::string, std::shared_ptr<const Owned>> loaded;
    for (const auto& t : texts)
    {
        auto card = std::make_shared<Mantle>();
        auto o = std::make_shared<Owned>();
        std::string problem;
        if (!loadMantle(t, *card, *o, problem, false))
        {
            refused.push_back({card->id.empty() ? t.id : card->id, problem});
            continue;
        }
        if (loaded.count("m:" + card->id))
        {
            refused.push_back({card->id, "loaded twice: the first is kept"});
            continue;
        }
        o->card = card;
        loaded["m:" + card->id] = std::move(o);
    }
    dropMantles();
    const auto n = loaded.size();
    owned_.merge(loaded);
    return n;
}

bool Library::checkMantle(const MantleText& text, Mantle& out, std::string& problem) const
{
    Owned o;
    return loadMantle(text, out, o, problem, true);
}

void Library::dropMantle(const std::string& id) { owned_.erase("m:" + id); }

void Library::dropMantles()
{
    for (auto i = owned_.begin(); i != owned_.end();)
        i = i->first.rfind("m:", 0) == 0 ? owned_.erase(i) : std::next(i);
}

const Mantle* Library::mantle(const std::string& id) const
{
    const auto o = owned_.find("m:" + id);
    return o == owned_.end() ? nullptr : o->second->card.get();
}

std::vector<std::string> Library::mantleTopics(const std::string& id) const
{
    // §43: the topics its whole scenes and stories are said on (their concepts, talk.<topic>), most scenes first.
    std::vector<std::string> out;
    const auto o = owned_.find("m:" + id);
    if (o == owned_.end())
        return out;
    std::map<std::string, int> n;
    for (const auto& e : o->second->entries)
        if (e.scene)
            for (const auto& c : e.concepts)
                if (c.rfind("talk.", 0) == 0)
                    ++n[c.substr(5)];
    for (const auto& [t, k] : n)
        out.push_back(t);
    std::stable_sort(out.begin(), out.end(), [&](const std::string& x, const std::string& y) { return n[x] > n[y]; });
    return out;
}

bool Library::ownEntry(const Entry& e, const Query& q, int speaker) const
{
    // (By its id, as answer's share counts them: "<mantle>.<item>", "@<npc>.<rule>".)
    const auto own = ownersOf(q, speaker);
    return std::any_of(own.begin(), own.end(), [&](const Owned* o) {
        return e.id.size() > o->owner.size() && e.id.compare(0, o->owner.size(), o->owner) == 0 && e.id[o->owner.size()] == '.';
    });
}

std::vector<std::string> Library::mantleIds() const
{
    std::vector<std::string> out;
    for (const auto& [key, o] : owned_)
        if (key.rfind("m:", 0) == 0)
            out.push_back(key.substr(2));
    return out;
}

std::size_t Library::mantleEntries() const
{
    std::size_t n = 0;
    for (const auto& [key, o] : owned_)
        if (key.rfind("m:", 0) == 0)
            n += o->entries.size();
    return n;
}

std::vector<const Entry*> Library::ownEntries(const std::string& owner) const
{
    std::vector<const Entry*> out;
    const auto key = owner.rfind('@', 0) == 0 ? "p:" + owner.substr(1) : "m:" + owner;
    if (const auto o = owned_.find(key); o != owned_.end())
        for (const auto& e : o->second->entries)
            out.push_back(&e);
    return out;
}

void Library::wear(Facts& person, const std::string& id) const
{
    const auto* m = mantle(id);
    if (!m)
        return;
    // Its opinions over the stances worked out of the wolf's facts (§7.6: "overridden by the wearer's mantle").
    for (const auto& [issue_, st] : m->stances)
    {
        const auto was = person.find("op." + issue_);
        if (was == person.end() || was->second != st.side)
            person.erase("op_why." + issue_);       // (A reason worked out for another side is not this one's.)
        person["op." + issue_] = st.side;
        person["op_strength." + issue_] = std::to_string(st.strength);
        person["op_basis." + issue_] = "mantle";
    }
    // Its likes over the drawn ones: one of each kind it names (a known item's words with it); its own words for the rest.
    if (!m->likes.empty())
    {
        std::vector<std::string> list = tokens(person.count("likes") ? person.at("likes") : std::string());
        for (const auto& like : m->likes)
        {
            const auto item = std::find_if(likes_.begin(), likes_.end(), [&](const LikeItem& l) { return !l.dislike && l.id == like; });
            if (item != likes_.end())
            {
                list.erase(std::remove_if(list.begin(), list.end(), [&](const std::string& was) {
                               return std::any_of(likes_.begin(), likes_.end(), [&](const LikeItem& l) {
                                   return !l.dislike && l.id == was && l.kind == item->kind;
                               });
                           }), list.end());
                person[item->kind + "_words"] = item->words;
            }
            list.push_back(like);
        }
        person["likes"] = joined(list);
    }
    if (!m->dislikes.empty())
    {
        person["dislikes"] = joined(m->dislikes);
        for (const auto& d : m->dislikes)
            if (const auto item = std::find_if(likes_.begin(), likes_.end(), [&](const LikeItem& l) { return l.dislike && l.id == d; });
                item != likes_.end())
            {
                person["dislike_words"] = item->words;
                person["dislike_kind"] = item->kind;
                break;
            }
    }
    if (!m->fears.empty())
        person["fears"] = joined(m->fears);
    if (!m->hopes.empty())
        person["hopes"] = joined(m->hopes);
}
} // namespace ratw::talk
