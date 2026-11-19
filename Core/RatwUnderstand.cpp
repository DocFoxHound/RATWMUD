// Authored understanding of a player's own words in the dialogue window (RatwUnderstand.h; Docs/Design/84 §13.5).
#include "RatwUnderstand.h"

#include "RatwJsonDoc.h"

#include <algorithm>
#include <cctype>
#include <set>

namespace ratw::understand
{
namespace
{
std::string lower(std::string s)
{
    for (auto& c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool same(const std::vector<std::string>& w, std::size_t at, const std::vector<std::string>& phrase)
{
    if (phrase.empty() || at + phrase.size() > w.size())
        return false;
    for (std::size_t i = 0; i < phrase.size(); ++i)
        if (w[at + i] != phrase[i])
            return false;
    return true;
}

// Where a phrase is said in the words (its first word's index), or npos.
std::size_t findIn(const std::vector<std::string>& w, const std::vector<std::string>& phrase)
{
    for (std::size_t at = 0; at + phrase.size() <= w.size(); ++at)
        if (same(w, at, phrase))
            return at;
    return std::string::npos;
}

// One letter off (a slip of the paw: "bakry"), for words of five letters or more.
bool nearly(const std::string& a, const std::string& b)
{
    if (a == b)
        return true;
    if (a.size() < 5 || b.size() < 5)
        return false;
    const auto n = a.size(), m = b.size();
    if (n > m + 1 || m > n + 1)
        return false;
    std::size_t i = 0, j = 0;
    int edits = 0;
    while (i < n && j < m)
    {
        if (a[i] == b[j])
        {
            ++i, ++j;
            continue;
        }
        if (++edits > 1)
            return false;
        if (n > m)
            ++i;
        else if (m > n)
            ++j;
        else
            ++i, ++j;
    }
    return edits + int(n - i) + int(m - j) <= 1;
}

// A phrase said nearly (each word the same, or one letter off).
std::size_t findNearly(const std::vector<std::string>& w, const std::vector<std::string>& phrase)
{
    for (std::size_t at = 0; at + phrase.size() <= w.size(); ++at)
    {
        bool ok = true;
        for (std::size_t i = 0; i < phrase.size() && ok; ++i)
            ok = nearly(w[at + i], phrase[i]);
        if (ok)
            return at;
    }
    return std::string::npos;
}

const std::set<std::string>& stop()
{
    static const std::set<std::string> S{"a", "an", "the", "in", "on", "at", "of", "to", "and", "or", "even", "every", "for",
                                         "with", "by", "is", "are", "be", "i", "you", "me", "my", "your", "it", "this", "that",
                                         "do", "does", "what", "some", "any", "can", "could", "would", "will", "let", "us",
                                         "please", "have", "has", "got", "just", "again", "there", "here"};
    return S;
}

std::vector<std::string> phrase(const std::string& text) { return tokens(text); }
} // namespace

std::vector<std::string> tokens(const std::string& text)
{
    std::vector<std::string> out;
    std::string w;
    const auto flush = [&] {
        while (!w.empty() && w.front() == '\'')
            w.erase(w.begin());
        while (!w.empty() && w.back() == '\'' && !(w.size() == 2 && w[0] == 'o'))
            w.pop_back();
        if (!w.empty())
            out.push_back(w);
        w.clear();
    };
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        const auto c = static_cast<unsigned char>(text[i]);
        if (std::isalnum(c))
            w += char(std::tolower(c));
        else if (c == '\'' || (c == 0xE2 && i + 2 < text.size() && static_cast<unsigned char>(text[i + 1]) == 0x80 &&
                               static_cast<unsigned char>(text[i + 2]) == 0x99))
        {
            w += '\'';
            if (c == 0xE2)
                i += 2;                             // (A curly apostrophe.)
        }
        else
            flush();
    }
    flush();
    return out;
}

bool Understanding::load(const std::string& text, std::string& problem)
{
    json::Value doc;
    if (!json::parse(text, doc, problem))
        return false;
    std::vector<Intent> intents;
    for (const auto& i : doc.array("intents"))
    {
        Intent in;
        in.id = i.string("id");
        in.needs = i.string("needs");
        for (const auto& c : i.array("cues"))
        {
            Cue cue;
            cue.words = phrase(c.string("p"));
            cue.weight = c.number("w");
            cue.whole = c.boolean("whole");
            cue.start = c.boolean("start");
            cue.slot = c.string("slot");
            if (cue.words.empty() || cue.weight <= 0 || cue.weight > 1)
            {
                problem = "understand.json: intent " + in.id + " has a cue with no words or a weight outside 0-1";
                return false;
            }
            in.cues.push_back(std::move(cue));
        }
        in.minor = i.boolean("minor");
        for (const auto& w : i.array("not"))
            in.unless.push_back(phrase(w.asString("")));
        if (in.id.empty() || in.cues.empty())
        {
            problem = "understand.json: an intent needs an id and cues";
            return false;
        }
        intents.push_back(std::move(in));
    }
    if (intents.empty())
    {
        problem = "understand.json: no intents";
        return false;
    }
    intents_ = std::move(intents);
    threshold_ = doc.number("threshold", .5);
    runnerUp_ = doc.number("runner_up", .35);
    filler_.clear();
    for (const auto& f : doc.array("filler"))
        filler_.push_back(phrase(f.asString("")));
    address_.clear();
    for (const auto& f : doc.array("address"))
        address_.push_back(phrase(f.asString("")));
    quirkWords_.clear();
    for (const auto& [k, list] : doc.object("quirk_words").fields())
        for (const auto& w : list.items())
            quirkWords_[k].push_back(lower(w.asString("")));
    contractions_.clear();
    for (const auto& [k, v] : doc.object("contractions").fields())
        contractions_[lower(k)] = phrase(v.asString(""));
    kin_.clear();
    for (const auto& k : doc.array("kin"))
        kin_.push_back(phrase(k.asString("")));
    issues_.clear();
    for (const auto& [id, list] : doc.object("issues").fields())
        for (const auto& p : list.items())
            issues_[id].push_back(phrase(p.asString("")));
    notNames_.clear();
    for (const auto& n : doc.array("not_names"))
        notNames_.push_back(lower(n.asString("")));
    // Longest first: "good wolf" before "good".
    std::sort(filler_.begin(), filler_.end(), [](const auto& a, const auto& b) { return a.size() > b.size(); });
    std::sort(address_.begin(), address_.end(), [](const auto& a, const auto& b) { return a.size() > b.size(); });
    return true;
}

std::vector<std::string> Understanding::words(const std::string& text, const Lexicon& lexicon) const
{
    std::vector<std::string> out;
    for (const auto& t : tokens(text))
    {
        if (const auto c = contractions_.find(t); c != contractions_.end())
        {
            out.insert(out.end(), c->second.begin(), c->second.end());
            continue;
        }
        // "Myrtle's" is Myrtle; "wick's" Wick.
        if (t.size() > 2 && t.compare(t.size() - 2, 2, "'s") == 0)
        {
            out.push_back(t.substr(0, t.size() - 2));
            continue;
        }
        out.push_back(t);
    }
    // The resident's own names, wherever they're said ("Myrtle, what do you sell?").
    const auto self = tokens(lexicon.self);
    if (!self.empty())
        out.erase(std::remove_if(out.begin(), out.end(), [&](const std::string& w) { return std::find(self.begin(), self.end(), w) != self.end(); }),
                  out.end());
    // Filler at the start ("Um, ...", "Oh, ..."); a way of addressing someone at either end ("..., friend?").
    for (bool cut = true; cut && !out.empty();)
    {
        cut = false;
        for (const auto* list : {&filler_, &address_})
            for (const auto& f : *list)
            {
                if (f.size() < out.size() && same(out, 0, f))
                {
                    out.erase(out.begin(), out.begin() + std::ptrdiff_t(f.size()));
                    cut = true;
                    break;
                }
                if (list == &address_ && f.size() < out.size() && same(out, out.size() - f.size(), f))
                {
                    out.resize(out.size() - f.size());
                    cut = true;
                    break;
                }
            }
    }
    return out;
}

Reading Understanding::read(const std::string& text, const Lexicon& lexicon) const
{
    Reading r;
    const auto w = words(text, lexicon);
    if (w.empty() || intents_.empty())
        return r;
    // The slots.
    std::map<std::string, std::string> slots;
    const auto self = tokens(lexicon.self);
    for (const auto& p : lexicon.people)
    {
        const auto name = tokens(p);
        if (name.empty() || name == self)
            continue;
        if (findNearly(w, name) != std::string::npos ||
            std::any_of(w.begin(), w.end(), [&](const std::string& x) { return nearly(x, name.front()); }) ||
            (name.size() > 1 && std::any_of(w.begin(), w.end(), [&](const std::string& x) { return x == name.back(); })))
        {
            slots["person"] = p;
            break;
        }
    }
    {
        std::size_t best = 0;
        for (const auto& p : lexicon.places)
        {
            auto name = tokens(p);
            if (!name.empty() && name.front() == "the")
                name.erase(name.begin());
            // ("the Grey Gull" for the Grey Gull Inn: a name's last word may be left off where it only says the kind.)
            static const std::set<std::string> Kind{"inn", "tavern", "alehouse", "forge", "smithy", "house", "hall", "stores", "store"};
            auto shorter = name;
            if (shorter.size() >= 3 && Kind.count(shorter.back()))
                shorter.pop_back();
            if (!name.empty() && name.size() >= best &&
                (findNearly(w, name) != std::string::npos || (shorter.size() < name.size() && findNearly(w, shorter) != std::string::npos)))
                slots["place"] = p, best = name.size() + 1;
        }
        if (!slots.count("place"))
            for (const auto& k : lexicon.placeKinds)
            {
                const auto name = tokens(k);
                if (!name.empty() && name.size() >= best && findNearly(w, name) != std::string::npos)
                    slots["place"] = k, best = name.size();
            }
    }
    // The town spoken in, by name, but not as a part of a place's ("Coldwater Stores").
    if (const auto town = tokens(lexicon.town); !town.empty() && findIn(w, town) != std::string::npos)
    {
        const auto place = slots.count("place") ? tokens(slots["place"]) : std::vector<std::string>{};
        if (findIn(place, town) == std::string::npos)
            slots["town"] = lexicon.town;
    }
    {
        std::size_t best = 0;
        for (const auto& [id, list] : issues_)
            for (const auto& p : list)
                if (p.size() > best && findIn(w, p) != std::string::npos)
                    slots["issue"] = id, best = p.size();
    }
    for (const auto& k : kin_)
        if (findIn(w, k) != std::string::npos)
        {
            slots["kin"] = [&] {
                std::string s;
                for (const auto& x : k)
                    s += (s.empty() ? "" : " ") + x;
                return s;
            }();
            break;
        }
    for (const auto& g : lexicon.goods)
        if (const auto name = tokens(g); !name.empty() && findNearly(w, name) != std::string::npos)
        {
            slots["good"] = g;
            break;
        }
    if (!lexicon.quirk.empty())
    {
        std::vector<std::string> q;
        std::string part;
        for (const char c : lexicon.quirk + "_")
            if (c == '_')
            {
                if (!part.empty() && !stop().count(part) && part.size() >= 3)
                    q.push_back(part);
                part.clear();
            }
            else
                part += char(std::tolower(static_cast<unsigned char>(c)));
        for (std::size_t i = 0, n = q.size(); i < n; ++i)
            if (const auto more = quirkWords_.find(q[i]); more != quirkWords_.end())
                for (const auto& m : more->second)
                    q.push_back(m);
        for (const auto& x : w)
            for (const auto& k : q)
            {
                // The word or its stem ("swims": swim, swimming).
                const auto stem = k.substr(0, std::max<std::size_t>(4, k.size() > 4 ? k.size() - 1 : k.size()));
                if (x == k || (stem.size() >= 4 && x.size() >= stem.size() && x.compare(0, stem.size(), stem) == 0))
                    slots["quirk"] = k;
            }
    }
    // The player's own name ("I'm Kestrel.", "Name's Kestrel", "Call me Kestrel"): the word after, as typed with a capital
    // where it follows "I am"; never a word that says something else ("I'm lost").
    {
        static const std::vector<std::vector<std::string>> Before{{"my", "name", "is"}, {"name", "is"}, {"call", "me"},
                                                                  {"they", "call", "me"}, {"i", "am", "called"}, {"known", "as"},
                                                                  {"go", "by"}, {"i", "am"}, {"this", "is"}, {"introduce", "myself"}};
        const auto cap = [](std::string x) {
            if (!x.empty())
                x[0] = char(std::toupper(static_cast<unsigned char>(x[0])));
            return x;
        };
        const auto capitalised = [&](const std::string& word) {
            // In the text as typed, the word with a capital.
            std::string s;
            for (std::size_t i = 0; i <= text.size(); ++i)
            {
                const char c = i < text.size() ? text[i] : ' ';
                if (std::isalnum(static_cast<unsigned char>(c)) || c == '\'')
                    s += c;
                else
                {
                    if (lower(s) == word && !s.empty() && std::isupper(static_cast<unsigned char>(s[0])))
                        return true;
                    s.clear();
                }
            }
            return false;
        };
        const auto known = [&](const std::string& x) {
            if (std::find(notNames_.begin(), notNames_.end(), x) != notNames_.end() || stop().count(x))
                return true;
            for (const auto& [id, list] : issues_)
                for (const auto& p : list)
                    if (p.size() == 1 && p.front() == x)
                        return true;
            return std::any_of(kin_.begin(), kin_.end(), [&](const auto& k) { return k.size() == 1 && k.front() == x; });
        };
        for (const auto& b : Before)
        {
            const auto at = findIn(w, b);
            if (at == std::string::npos)
                continue;
            if (b == std::vector<std::string>{"introduce", "myself"})
            {
                // "Let me introduce myself: Kestrel." / "...myself as Kestrel".
                std::size_t k = at + 2;
                if (k < w.size() && w[k] == "as")
                    ++k;
                if (k < w.size() && !known(w[k]) && capitalised(w[k]))
                {
                    slots["name"] = cap(w[k]);
                    if (k + 1 < w.size() && !known(w[k + 1]) && capitalised(w[k + 1]))
                        slots["name"] += " " + cap(w[k + 1]);
                }
                if (slots.count("name"))
                    break;
                continue;
            }
            const auto k = at + b.size();
            if (k >= w.size() || known(w[k]))
                continue;
            const bool loose = b.size() >= 2 && b != std::vector<std::string>{"i", "am"} && b != std::vector<std::string>{"this", "is"};
            // (Said with a capital, or the last word said: "i am sorrel".)
            if (!loose && !capitalised(w[k]) && k + 1 != w.size())
                continue;
            if (slots.count("person") && tokens(slots["person"]).front() == w[k])
                continue;                           // ("I'm Wick's friend": not a name given.)
            slots["name"] = cap(w[k]);
            if (k + 1 < w.size() && !known(w[k + 1]) && capitalised(w[k + 1]))
                slots["name"] += " " + cap(w[k + 1]);   // ("Garret Fenwick")
            break;
        }
        if (!slots.count("name") && w.size() >= 4)
            for (std::size_t at = 1; at + 2 < w.size(); ++at)
                if (w[at] == "is" && w[at + 1] == "my" && w[at + 2] == "name" && !known(w[at - 1]))
                {
                    slots["name"] = cap(w[at - 1]);
                    break;
                }
    }
    // Each intent's score: its best cue, and a little for each other cue it has.
    struct Scored
    {
        double score;
        const Intent* intent;
    };
    std::vector<Scored> scored;
    for (const auto& in : intents_)
    {
        if (!in.needs.empty() && !slots.count(in.needs))
            continue;
        double best = 0;
        int more = 0;
        for (const auto& c : in.cues)
        {
            if (!c.slot.empty() && !slots.count(c.slot))
                continue;
            const bool hit = c.whole ? c.words == w : c.start ? same(w, 0, c.words) : findIn(w, c.words) != std::string::npos;
            if (!hit)
                continue;
            if (c.weight > best)
            {
                if (best > 0)
                    ++more;
                best = c.weight;
            }
            else
                ++more;
        }
        if (best <= 0)
            continue;
        double score = std::min(1.0, best + .08 * std::min(more, 2));
        if (std::any_of(in.unless.begin(), in.unless.end(), [&](const auto& u_) { return findIn(w, u_) != std::string::npos; }))
            score -= .4;
        if (score > 0)
            scored.push_back({score, &in});
    }
    // A greeting, thanks, a yes said with a question: the question is what was said ("Hello, I'm Bramble").
    if (std::any_of(scored.begin(), scored.end(), [](const Scored& x) { return !x.intent->minor && x.score >= .55; }))
        for (auto& x : scored)
            if (x.intent->minor)
                x.score -= .3;
    if (scored.empty())
        return r;
    std::stable_sort(scored.begin(), scored.end(), [](const Scored& a, const Scored& b) { return a.score > b.score; });
    const auto& top = scored.front();
    const double second = scored.size() > 1 ? scored[1].score : 0;
    r.intent = top.intent->id;
    r.second = scored.size() > 1 ? scored[1].intent->id : std::string();
    r.confidence = std::clamp(top.score - runnerUp_ * second, 0.0, 1.0);
    r.sure = r.confidence >= threshold_;
    // The slots that intent speaks to.
    static const std::map<std::string, std::vector<std::string>> Wants{
        {"ask_opinion", {"issue"}}, {"ask_person", {"person"}}, {"ask_way", {"place"}},  {"introduce", {"name"}},
        {"ask_family", {"kin"}},    {"ask_prices", {"good"}},   {"ask_wares", {"good"}}, {"ask_quirk", {"quirk"}}};
    if (const auto want = Wants.find(r.intent); want != Wants.end())
        for (const auto& k : want->second)
            if (const auto s = slots.find(k); s != slots.end())
                r.slots[k] = s->second;
    return r;
}

int Understanding::matchChoice(const std::string& text, const std::vector<std::string>& choices, const Lexicon& lexicon) const
{
    const auto content = [&](const std::vector<std::string>& w) {
        std::set<std::string> out;
        for (const auto& x : w)
            if (!stop().count(x))
                out.insert(x);
        return out;
    };
    const auto said = words(text, lexicon);
    const auto mine = content(said);
    int best = -1;
    double bestScore = 0;
    for (std::size_t i = 0; i < choices.size(); ++i)
    {
        const auto theirs = content(words(choices[i], lexicon));
        if (theirs.empty() || mine.empty())
            continue;
        std::size_t both = 0;
        for (const auto& x : theirs)
            both += mine.count(x);
        const double score = double(both) / double(mine.size() + theirs.size() - both);
        const bool all = both == theirs.size() && mine.size() <= theirs.size() + 2;
        if ((score >= .6 || all) && score > bestScore)
            best = int(i), bestScore = score;
    }
    return best;
}

std::vector<std::string> Understanding::intents() const
{
    std::vector<std::string> out;
    for (const auto& i : intents_)
        out.push_back(i.id);
    return out;
}

std::vector<std::string> Understanding::issues() const
{
    std::vector<std::string> out;
    for (const auto& [id, list] : issues_)
        out.push_back(id);
    return out;
}
} // namespace ratw::understand
