// Names and introductions (RatwNames.h; Docs/Design/32-parties-chapters-factions.md, 1.5).
#include "RatwNames.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace ratw::names
{
using json::Value;

namespace
{
std::string lower(std::string s)
{
    for (auto& ch : s)
        ch = char(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

bool wordChar(char ch) { return std::isalnum(static_cast<unsigned char>(ch)) || ch == '\'' || ch == '-'; }

// The text as lower-case words, with punctuation as its own token where it matters (",").
std::vector<std::string> words(const std::string& raw)
{
    // Curly apostrophes as straight ones ("I’m").
    std::string text;
    for (std::size_t i = 0; i < raw.size(); ++i)
        if (raw.compare(i, 3, "\xe2\x80\x99") == 0)
        {
            text += '\'';
            i += 2;
        }
        else
            text += raw[i];
    std::vector<std::string> out;
    std::string word;
    for (char ch : text)
    {
        if (std::isalnum(static_cast<unsigned char>(ch)) || ((ch == '\'' || ch == '-') && !word.empty()))
            word += char(std::tolower(static_cast<unsigned char>(ch)));
        else
        {
            while (!word.empty() && (word.back() == '\'' || word.back() == '-'))
                word.pop_back();
            if (!word.empty())
                out.push_back(word);
            word.clear();
            if (ch == ',' || ch == '.' || ch == '!' || ch == '?' || ch == ';' || ch == ':')
                out.push_back(std::string(1, ch));
        }
    }
    while (!word.empty() && (word.back() == '\'' || word.back() == '-'))
        word.pop_back();
    if (!word.empty())
        out.push_back(word);
    return out;
}

bool matchAt(const std::vector<std::string>& w, std::size_t at, const std::vector<std::string>& phrase)
{
    if (at + phrase.size() > w.size())
        return false;
    for (std::size_t i = 0; i < phrase.size(); ++i)
        if (w[at + i] != phrase[i])
            return false;
    return true;
}

int hexValue(const std::string& hex, int at)
{
    return int(std::strtol(hex.substr(std::size_t(at), 2).c_str(), nullptr, 16));
}

// The nearest of a few named colours to a "#rrggbb".
std::string colourWord(const std::string& given, bool coat)
{
    const std::string hex = lower(given);     // (The palette is written in upper case.)
    struct Named
    {
        const char* word;
        int r, g, b;
    };
    static const std::array<Named, 8> coats{{{"cream", 0xe1, 0xd9, 0xc6}, {"silver", 0xad, 0xb3, 0xb2}, {"grey", 0x77, 0x7d, 0x7b},
                                             {"dun", 0x8e, 0x82, 0x71}, {"brown", 0x65, 0x51, 0x3f}, {"black", 0x30, 0x35, 0x34},
                                             {"russet", 0xa2, 0x68, 0x43}, {"sandy", 0xbe, 0xaa, 0x84}}};
    static const std::array<Named, 7> marks{{{"white", 0xf0, 0xf0, 0xea}, {"cream", 0xe1, 0xd9, 0xc6}, {"tan", 0xc8, 0xa8, 0x78},
                                             {"brown", 0x6b, 0x4e, 0x37}, {"black", 0x25, 0x28, 0x2a}, {"grey", 0x8c, 0x8f, 0x8e},
                                             {"red", 0xa6, 0x5a, 0x3a}}};
    if (!hexColour(hex))
        return coat ? "grey" : "pale";
    const int r = hexValue(hex, 1), g = hexValue(hex, 3), b = hexValue(hex, 5);
    const auto pick = [&](const auto& set) {
        const char* best = set[0].word;
        long nearest = -1;
        for (const auto& n : set)
        {
            const long d = long(r - n.r) * (r - n.r) + long(g - n.g) * (g - n.g) + long(b - n.b) * (b - n.b);
            if (nearest < 0 || d < nearest)
            {
                nearest = d;
                best = n.word;
            }
        }
        return std::string(best);
    };
    return coat ? pick(coats) : pick(marks);
}

std::string markingWords(const Marking& m, const std::string& colour)
{
    const auto& k = m.mask;
    if (k == "socks" || k == "stockings")
        return colour + " " + k;
    if (k == "tail_tip")
        return article(colour) + " " + colour + "-tipped tail";
    if (k == "ear_tips")
        return colour + "-tipped ears";
    if (k == "freckles")
        return "freckles";
    if (k == "brindle")
        return "a brindled coat";
    if (k == "merle")
        return "a merle coat";
    if (k == "scar")
        return "a scar";
    if (k == "eye_patches")
        return colour + " eye patches";
    return article(colour) + " " + colour + " " + k;     // blaze, mask, cape, bib, belly, saddle
}
} // namespace

std::string introducedName(const std::string& text, const std::vector<std::string>& names)
{
    const auto w = words(text);
    static const std::vector<std::vector<std::string>> phrases{
        {"i'm"}, {"im"}, {"i", "am"}, {"my", "name", "is"}, {"my", "name's"}, {"name's"}, {"names"}, {"call", "me"},
        {"they", "call", "me"}, {"you", "can", "call", "me"}, {"you", "may", "call", "me"}, {"the", "name", "is"}, {"the", "name's"}};
    for (const auto& name : names)
    {
        const auto n = words(name);
        if (n.empty())
            continue;
        for (std::size_t at = 0; at < w.size(); ++at)
        {
            // "I'm Kestrel", "call me Kestrel", "my name is Kestrel".
            for (const auto& p : phrases)
                if (matchAt(w, at, p))
                {
                    std::size_t after = at + p.size();
                    if (after < w.size() && w[after] == ",")
                        ++after;
                    if (matchAt(w, after, n))
                        return name;
                }
            // "Kestrel, of the Ashen Lodge" or "Kestrel, at your service", opening a sentence.
            const bool opens = at == 0 || w[at - 1] == "." || w[at - 1] == "!" || w[at - 1] == "?";
            if (opens && matchAt(w, at, n))
            {
                std::size_t after = at + n.size();
                if (after < w.size() && w[after] == ",")
                    ++after;
                if (matchAt(w, after, {"of"}) || matchAt(w, after, {"at", "your", "service"}))
                    return name;
            }
        }
    }
    return {};
}

std::string aliasProblem(const std::string& alias, const std::string& trueName, const std::vector<std::string>& aliases)
{
    if (alias.size() < 2 || alias.size() > 24)
        return "A name is 2 to 24 letters.";
    for (std::size_t i = 0; i < alias.size(); ++i)
    {
        const char ch = alias[i];
        const bool inside = i > 0 && i + 1 < alias.size();
        if (!std::isalpha(static_cast<unsigned char>(ch)) && !(inside && (ch == ' ' || ch == '-' || ch == '\'')))
            return "A name is letters, with spaces, hyphens or apostrophes only between them.";
    }
    if (lower(alias) == lower(trueName))
        return "That is your own name.";
    for (const auto& a : aliases)
        if (lower(a) == lower(alias))
            return "You already go by that name.";
    if (aliases.size() >= MaxAliases)
        return "You can go by at most three other names. Retire one first.";
    return {};
}

std::string article(const std::string& word)
{
    if (word.empty())
        return "a";
    const char first = char(std::tolower(static_cast<unsigned char>(word[0])));
    return std::string("aeiou").find(first) != std::string::npos ? "an" : "a";
}

std::string capitalised(const std::string& text)
{
    if (text.empty())
        return text;
    std::string out = text;
    out[0] = char(std::toupper(static_cast<unsigned char>(out[0])));
    return out;
}

std::string describe(const Appearance& a, int age)
{
    std::vector<std::string> words;
    if (a.stature == "tall" || a.stature == "short")
        words.push_back(a.stature == "tall" ? "tall" : "small");
    else if (a.build == "lean" || a.build == "heavy")
        words.push_back(a.build);
    const auto stage = lifeStage(age);
    if (stage == LifeStage::Young || stage == LifeStage::Adolescent)
        words.push_back("young");
    else if (stage == LifeStage::Old)
        words.push_back("old");
    words.push_back(colourWord(!a.coat.empty() ? a.coat : coatColorHex(a.baseColor), true));
    words.push_back("wolf");
    std::string out;
    for (const auto& w : words)
        out += (out.empty() ? "" : " ") + w;
    out = article(out) + " " + out;
    // The plainest marking to tell them by: the most visible one.
    const Marking* shown = nullptr;
    for (const auto& m : a.markings)
        if (knownMarking(m.mask) && m.opacity >= .4 && (!shown || m.opacity > shown->opacity))
            shown = &m;
    if (shown)
    {
        const std::string hex = !shown->color.empty() ? shown->color : !a.markingTint.empty() ? a.markingTint : coatColorHex(a.markingColor);
        out += " with " + markingWords(*shown, colourWord(hex, false));
    }
    return out;
}

std::string veil(const std::string& text, const std::map<std::string, std::string>& labels)
{
    if (labels.empty())
        return text;
    std::string out;
    out.reserve(text.size() + 32);
    std::size_t i = 0;
    while (i < text.size())
    {
        bool replaced = false;
        const bool boundaryBefore = i == 0 || !wordChar(text[i - 1]);
        if (boundaryBefore && std::isupper(static_cast<unsigned char>(text[i])))
            for (const auto& [name, label] : labels)
            {
                if (name.empty() || text.compare(i, name.size(), name) != 0)
                    continue;
                const std::size_t end = i + name.size();
                // Possessive "Ash's" is still Ash.
                if (end < text.size() && wordChar(text[end]) && !(text[end] == '\'' && end + 1 < text.size() && text[end + 1] == 's'))
                    continue;
                // Part of a longer proper name ("Ash Hollow"): leave it.
                if (end + 1 < text.size() && text[end] == ' ' && std::isupper(static_cast<unsigned char>(text[end + 1])))
                    continue;
                // At the start of the text or a sentence: capitalised.
                std::size_t back = out.size();
                while (back > 0 && out[back - 1] == ' ')
                    --back;
                const bool opens = back == 0 || out[back - 1] == '.' || out[back - 1] == '!' || out[back - 1] == '?' ||
                                   out[back - 1] == '"' || out[back - 1] == '\n';
                out += opens ? capitalised(label) : label;
                i = end;
                replaced = true;
                break;
            }
        if (!replaced)
            out += text[i++];
    }
    return out;
}

bool Acquaintances::learn(const std::string& knower, const std::string& known, const std::string& name, const std::string& how,
                          double day)
{
    if (knower.empty() || known.empty() || knower == known || name.empty())
        return false;
    auto& mine = known_[knower];
    auto found = mine.find(known);
    if (found == mine.end())
    {
        if (mine.size() >= MaxKnown)
        {
            // The one met longest ago goes.
            auto oldest = mine.begin();
            for (auto it = mine.begin(); it != mine.end(); ++it)
                if (it->second.day < oldest->second.day)
                    oldest = it;
            mine.erase(oldest);
        }
        mine[known] = {{name}, how, day};
        return true;
    }
    auto& names = found->second.names;
    const auto at = std::find(names.begin(), names.end(), name);
    if (at != names.end())
    {
        if (at + 1 == names.end())
            return false;
        names.erase(at);                          // Heard again: the newest once more.
        names.push_back(name);
        return false;
    }
    names.push_back(name);
    if (names.size() > MaxAliases + 1)
        names.erase(names.begin());
    return true;
}

const Known* Acquaintances::find(const std::string& knower, const std::string& known) const
{
    const auto mine = known_.find(knower);
    if (mine == known_.end())
        return nullptr;
    const auto found = mine->second.find(known);
    return found == mine->second.end() ? nullptr : &found->second;
}

std::string Acquaintances::nameFor(const std::string& knower, const std::string& known) const
{
    const auto* k = find(knower, known);
    return k && !k->names.empty() ? k->names.back() : std::string();
}

void Acquaintances::forget(const std::string& id)
{
    known_.erase(id);
    for (auto& [knower, mine] : known_)
        mine.erase(id);
}

std::size_t Acquaintances::size() const
{
    std::size_t n = 0;
    for (const auto& [knower, mine] : known_)
        n += mine.size();
    return n;
}

Value Acquaintances::save() const
{
    auto root = Value::object();
    for (const auto& [knower, mine] : known_)
    {
        auto list = Value::array();
        for (const auto& [known, k] : mine)
        {
            auto j = Value::object();
            j.add("id", known);
            auto names = Value::array();
            for (const auto& n : k.names)
                names.push(n);
            j.add("names", names);
            j.add("how", k.how);
            j.add("day", k.day);
            list.push(j);
        }
        root.add(knower, list);
    }
    return root;
}

void Acquaintances::load(const Value& saved)
{
    known_.clear();
    for (const auto& [knower, list] : saved.fields())
        for (const auto& j : list.items())
        {
            const auto known = j.string("id");
            for (const auto& n : j.array("names"))
                if (n.isString())
                    learn(knower, known, n.asString(), j.string("how", "introduced"), j.number("day"));
        }
}
} // namespace ratw::names
