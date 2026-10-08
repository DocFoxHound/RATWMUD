#include "RatwStars.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace ratw::stars
{
namespace
{
std::filesystem::path socialFile()
{
    // Data/Social: RATW_DATA_DIR, else the working directory or one above it, else the source tree.
    namespace fs = std::filesystem;
    std::error_code ec;
    if (const char* dir = std::getenv("RATW_DATA_DIR"); dir && *dir)
        return fs::path(dir) / "Social" / "social.json";
    for (auto at = fs::current_path(ec); !ec && !at.empty(); at = at.parent_path())
    {
        if (fs::exists(at / "Data" / "Social" / "social.json", ec))
            return at / "Data" / "Social" / "social.json";
        if (at == at.parent_path())
            break;
    }
#ifdef RATW_SOURCE_DIR
    return fs::path(RATW_SOURCE_DIR) / "Data" / "Social" / "social.json";
#else
    return fs::path("Data") / "Social" / "social.json";
#endif
}

Rules build()
{
    Rules r;
    r.tags = {"storyteller", "packmate", "goodfun", "welcoming"};
    r.tagNames = {{"storyteller", "Storyteller"}, {"packmate", "Packmate"}, {"goodfun", "Good fun"}, {"welcoming", "Welcoming"}};
    r.newcomersOnly = {"welcoming"};
    std::ifstream in(socialFile(), std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    json::Value doc;
    std::string error;
    if (!in || !json::parse(text.str(), doc, error))
        return r;
    const auto& s = doc.object("stars");
    const auto ints = [](const json::Value& list, std::vector<int>& out) {
        if (list.items().empty())
            return;
        out.clear();
        for (const auto& n : list.items())
            out.push_back(int(n.asNumber(0)));
    };
    ints(s.array("bands"), r.bands);
    ints(s.array("giverBands"), r.giverBands);
    const auto& counting = s.object("counting");
    r.giverADay = int(counting.number("giverADay", r.giverADay));
    r.pairADay = int(counting.number("pairADay", r.pairADay));
    r.pairIn30Days = int(counting.number("pairIn30Days", r.pairIn30Days));
    r.keptDays = int(s.number("keptDays", r.keptDays));
    r.showRate = s.boolean("showRate", r.showRate);
    r.rateAfter = int(s.number("rateAfter", r.rateAfter));
    r.tagWindow = s.number("tagWindow", r.tagWindow);
    r.knownForAt = int(s.number("knownForAt", r.knownForAt));
    for (const auto& w : s.array("rateWords"))
        if (w.isArray() && w.items().size() == 2)
            r.rateWords.push_back({w.items()[0].asNumber(0), w.items()[1].asString({})});
    if (!s.array("tags").empty())
    {
        r.tags.clear();
        for (const auto& t : s.array("tags"))
        {
            r.tags.push_back(t.string("id"));
            r.tagNames[t.string("id")] = t.string("name", t.string("id"));
            if (t.boolean("newcomersOnly"))
                r.newcomersOnly.push_back(t.string("id"));
        }
    }
    r.catalog = s;
    const auto& scenes = doc.object("scenes");
    if (!scenes.array("colours").empty())
    {
        r.sceneColours.clear();
        for (const auto& c : scenes.array("colours"))
            r.sceneColours.push_back(c.asString("#8796a3"));
    }
    r.mapRange = scenes.number("mapRange", r.mapRange);
    const auto& h = doc.object("howl");
    auto& w = r.howl;
    for (auto [key, field] : std::initializer_list<std::pair<const char*, double*>>{
             {"range", &w.range}, {"heightLimit", &w.heightLimit}, {"indoorsHowler", &w.indoorsHowler},
             {"indoorsListener", &w.indoorsListener}, {"cooldown", &w.cooldown}, {"markSeconds", &w.markSeconds},
             {"jitter", &w.jitter}, {"chorusRange", &w.chorusRange}, {"chorusWindow", &w.chorusWindow},
             {"chorusExtend", &w.chorusExtend}, {"chorusMost", &w.chorusMost}, {"chorusCarry", &w.chorusCarry},
             {"chorusCarryMost", &w.chorusCarryMost}, {"sneakRange", &w.sneakRange}, {"residentRange", &w.residentRange},
             {"rememberedSeconds", &w.rememberedSeconds}})
        *field = h.number(key, *field);
    w.residentsTurn = int(h.number("residentsTurn", w.residentsTurn));
    w.residentsSpeak = int(h.number("residentsSpeak", w.residentsSpeak));
    for (const auto& [when, list] : h.object("lines").fields())
        for (const auto& line : list.items())
            if (line.isString())
                w.lines[when].push_back(line.asString());
    return r;
}
} // namespace

const Rules& rules()
{
    static const Rules r = build();
    return r;
}

std::string sceneColour(const std::string& session)
{
    std::uint32_t h = 2166136261u;
    for (const unsigned char ch : session)
        h = (h ^ ch) * 16777619u;
    const auto& palette = rules().sceneColours;
    return palette.empty() ? std::string("#8796a3") : palette[h % palette.size()];
}

std::string band(int n, bool givers)
{
    const auto& list = givers ? rules().giverBands : rules().bands;
    std::string out = "a few";
    for (const int b : list)
        if (n >= b)
            out = std::to_string(b) + "+";
    return out;
}

Star Book::record(Star star)
{
    // Counted: among the giver account's first 10 of the rolling day, at most the 3rd from it to this account in a day,
    // and the 10th between the two in 30 days. One that doesn't count is still given (and thanked, and pays its XP).
    const auto& r = rules();
    int giverDay = 0, pairDay = 0, pair30 = 0;
    for (const auto& s : recent_)
    {
        if (!s.counted || s.giverAccount != star.giverAccount)
            continue;
        const double ago = std::abs(star.at - s.at);   // (Either way round: a star loaded or recorded out of order.)
        giverDay += ago < 86400;
        if (s.recipientAccount == star.recipientAccount)
        {
            pairDay += ago < 86400;
            pair30 += ago < r.keptDays * 86400.0;
        }
    }
    star.counted = giverDay < r.giverADay && pairDay < r.pairADay && pair30 < r.pairIn30Days &&
                   !star.giverAccount.empty() && star.giverAccount != star.recipientAccount;
    auto& t = tallies_[star.recipientAccount];
    if (star.kind == "gold")
        ++t.goldReceived;
    if (star.counted)
    {
        ++t.total;
        ++t.kinds[star.kind];
        if (!star.tag.empty())
            ++t.tags[star.tag];
        t.givers.insert(star.giverAccount);
    }
    recent_.push_back(star);
    return star;
}

const Tally* Book::tally(const std::string& account) const
{
    const auto it = tallies_.find(account);
    return it == tallies_.end() ? nullptr : &it->second;
}

bool Book::tag(const std::string& starId, const std::string& giverAccount, const std::string& tag, bool newcomer, double now,
               std::string& error)
{
    const auto& r = rules();
    const auto it = std::find_if(recent_.rbegin(), recent_.rend(), [&](const Star& s) { return s.id == starId; });
    const bool known = std::find(r.tags.begin(), r.tags.end(), tag) != r.tags.end();
    const bool newcomers = std::find(r.newcomersOnly.begin(), r.newcomersOnly.end(), tag) != r.newcomersOnly.end();
    if (it == recent_.rend() || it->giverAccount != giverAccount)
        error = "That isn't a star you gave.";
    else if (!it->tag.empty())
        error = "That star is tagged already.";
    else if (now - it->at > r.tagWindow)
        error = "It's too late to tag that star.";
    else if (!known)
        error = "Not a tag.";
    else if (newcomers && !newcomer)
        error = "Only a newcomer gives that tag.";
    else
    {
        it->tag = tag;
        if (it->counted)
            ++tallies_[it->recipientAccount].tags[tag];
        return true;
    }
    return false;
}

std::vector<const Star*> Book::openToTag(const std::string& giverCharacter, double now) const
{
    // (Newest first, from the end: the list is in time order, so the window is a short walk back.)
    std::vector<const Star*> out;
    for (auto it = recent_.rbegin(); it != recent_.rend() && now - it->at <= rules().tagWindow; ++it)
        if (it->giverCharacter == giverCharacter && it->tag.empty())
            out.push_back(&*it);
    return out;
}

void Book::chances(const std::string& account, int n)
{
    if (n > 0 && !account.empty())
        tallies_[account].chances += n;
}

void Book::prune(double now)
{
    const double from = now - rules().keptDays * 86400.0;
    recent_.erase(std::remove_if(recent_.begin(), recent_.end(), [&](const Star& s) { return s.at < from; }), recent_.end());
}

json::Value Book::view(const std::string& account, bool exact) const
{
    const auto* t = tally(account);
    const Tally none;
    const auto& tl = t ? *t : none;
    auto o = json::Value::object();
    o.add("exact", exact);
    if (exact)
    {
        o.add("total", tl.total);
        o.add("from", int(tl.givers.size()));
        auto kinds = json::Value::object();
        for (const auto& [k, n] : tl.kinds)
            kinds.add(k, n);
        o.add("kinds", kinds);
    }
    o.add("band", band(tl.total));
    o.add("fromBand", band(int(tl.givers.size()), true));
    const auto& r = rules();
    // Tags (§2): counts for the exact view; for everyone else each tag's share in words, never a number.
    int tagged = 0, most = 0;
    for (const auto& [t, n] : tl.tags)
        tagged += n, most = std::max(most, n);
    auto tags = json::Value::array();
    for (const auto& id : r.tags)
    {
        const auto it = tl.tags.find(id);
        if (it == tl.tags.end() || it->second <= 0)
            continue;
        auto row = json::Value::object();
        row.add("tag", id);
        row.add("name", r.tagNames.count(id) ? r.tagNames.at(id) : id);
        if (exact)
            row.add("count", it->second);
        else
        {
            const double share = double(it->second) / tagged;
            row.add("words", share >= .5 ? "mostly" : share >= .25 ? "often" : share >= .1 ? "sometimes" : "now and then");
        }
        tags.push(row);
    }
    o.add("tags", tags);
    // Known for (§2): from 50 counted stars, the most-given tag (both, when tied).
    if (tl.total >= r.knownForAt && most > 0)
    {
        auto known = json::Value::array();
        for (const auto& id : r.tags)
            if (const auto it = tl.tags.find(id); it != tl.tags.end() && it->second == most)
                known.push(r.tagNames.count(id) ? r.tagNames.at(id) : id);
        o.add("knownFor", known);
    }
    // The star rate (§3; shown, the user's answer): Gold Stars received for the chances to be starred, in words, once
    // there have been enough chances.
    if (r.showRate && tl.chances >= r.rateAfter)
    {
        const double rate = double(tl.goldReceived) / tl.chances;
        for (const auto& [floor, words] : r.rateWords)
            if (rate >= floor)
            {
                o.add("rate", words);
                break;
            }
        if (exact)
            o.add("rateShare", std::round(std::min(1.0, rate) * 100) / 100);
    }
    return o;
}

json::Value Book::save() const
{
    auto tallies = json::Value::array();
    for (const auto& [account, t] : tallies_)
    {
        auto e = json::Value::object();
        e.add("account", account);
        e.add("total", t.total);
        e.add("chances", t.chances);
        e.add("goldReceived", t.goldReceived);
        auto kinds = json::Value::object(), tags = json::Value::object();
        for (const auto& [k, n] : t.kinds)
            kinds.add(k, n);
        for (const auto& [k, n] : t.tags)
            tags.add(k, n);
        e.add("kinds", kinds);
        e.add("tags", tags);
        auto givers = json::Value::array();
        for (const auto& g : t.givers)
            givers.push(g);
        e.add("givers", givers);
        tallies.push(e);
    }
    auto recent = json::Value::array();
    for (const auto& s : recent_)
    {
        auto e = json::Value::object();
        e.add("id", s.id);
        e.add("kind", s.kind);
        e.add("source", s.source);
        e.add("giverAccount", s.giverAccount);
        e.add("giverCharacter", s.giverCharacter);
        e.add("recipientAccount", s.recipientAccount);
        e.add("recipientCharacter", s.recipientCharacter);
        if (!s.tag.empty())
            e.add("tag", s.tag);
        e.add("at", s.at);
        e.add("xp", s.xp);
        e.add("counted", s.counted);
        recent.push(e);
    }
    auto root = json::Value::object();
    root.add("tallies", tallies);
    root.add("recent", recent);
    return root;
}

void Book::load(const json::Value& saved)
{
    tallies_.clear();
    recent_.clear();
    for (const auto& e : saved.array("tallies"))
    {
        if (e.string("account").empty())
            continue;
        auto& t = tallies_[e.string("account").substr(0, 80)];
        t.total = std::max(0, int(e.number("total")));
        t.chances = std::max(0, int(e.number("chances")));
        t.goldReceived = std::max(0, int(e.number("goldReceived")));
        for (const auto& [k, n] : e.object("kinds").fields())
            t.kinds[k.substr(0, 20)] = std::max(0, int(n.asNumber(0)));
        for (const auto& [k, n] : e.object("tags").fields())
            if (std::find(rules().tags.begin(), rules().tags.end(), k) != rules().tags.end())
                t.tags[k] = std::max(0, int(n.asNumber(0)));
        for (const auto& g : e.array("givers"))
            if (g.isString() && t.givers.size() < 100000)
                t.givers.insert(g.asString().substr(0, 80));
    }
    for (const auto& e : saved.array("recent"))
        recent_.push_back({e.string("id").substr(0, 80), e.string("kind").substr(0, 20), e.string("source").substr(0, 120),
                           e.string("giverAccount").substr(0, 80), e.string("giverCharacter").substr(0, 80),
                           e.string("recipientAccount").substr(0, 80), e.string("recipientCharacter").substr(0, 80),
                           e.string("tag").substr(0, 20), e.number("at"), int(e.number("xp")), e.boolean("counted")});
    std::sort(recent_.begin(), recent_.end(), [](const Star& a, const Star& b) { return a.at < b.at; });
}
} // namespace ratw::stars
