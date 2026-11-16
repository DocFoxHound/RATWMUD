// The topic director in data (RatwTopics.h; Docs/Design/84 §7.3, Phase 5).
#include "RatwTopics.h"
#include "RatwJsonDoc.h"

#include <set>

namespace ratw::topics
{
namespace
{
Score s(double base, int hourly = -1, int jitter = -1, double jitterScale = 1, double add = 0)
{
    Score x;
    x.base = base;
    x.hourly = hourly;
    x.jitter = jitter;
    x.jitterScale = jitterScale;
    x.add = add;
    return x;
}

Book makeToday()
{
    // The C++ scores of World::ambientTopic and sceneTopics as they were at Phase 4 (r3-talk-p4, 0eec3398), source by
    // source; the formula each is used in is the director's (RatwAmbient.cpp).
    Book b;
    const auto add = [&](const char* topic, bool news, bool everyday, std::map<std::string, Score> kinds) {
        Topic t;
        t.name = topic;
        t.news = news;
        t.everyday = everyday;
        t.kinds = std::move(kinds);
        b.topics[topic] = std::move(t);
    };
    Score claim = s(3, -1, -1, 1, .5);              // 3 + confidence, + .5 for one about a traveller.
    Score deed = s(3.2);
    deed.scale = .3;                                // 3.2 + confidence x .3.
    Score quarrel = s(2.5);
    quarrel.per = 100;                              // 2.5 + |the worse affinity| / 100.
    Score prices = s(1.65, 16);
    prices.step = .3;                               // Each further move .3 lower.
    add("gossip", true, false, {{"claim", claim}});
    add("bandits", true, false, {{"claim", claim}, {"raided", s(2.4)}, {"camp", s(2.0)}});
    add("deed", true, false, {{"", deed}});
    add("news", true, false, {{"", s(2.8, -1, -1, 1, .2)}});   // + .2 for a loss or a crime.
    add("quarrel", false, false, {{"", quarrel}});
    add("friends", false, true, {{"", s(1.5)}});
    add("day", false, true, {{"", s(1, -1, -1, 1, .3)}});     // + .3 on a festival.
    add("caravan", true, false,
        {{"raided", s(2.1)}, {"late", s(1.75, 13)}, {"due", s(1.5, 13)}, {"", s(1.7, 13)}, {"posted", s(1.45, 14)}, {"news", s(1.65, 15)}});
    add("prices", true, false, {{"", prices}});
    add("price_cause", true, false, {{"", s(0, -1, -1, 1, .05)}});   // The price's own draw + .05.
    add("ships", true, false,
        {{"lost", s(1.85, 18)}, {"late", s(1.75, 18)}, {"in", s(1.7, 18)}, {"due", s(1.5, 18)}, {"sailed", s(1.3, 18)}});
    add("event", true, false, {{"now", s(1.7, 19)}, {"soon", s(1.55, 19)}, {"", s(1.35, 19)}});
    add("times", true, false, {{"hard", s(1.7, 11)}, {"boom", s(1.7, 11)}, {"", s(1.6, 11)}});
    add("harvest", true, false, {{"fair", s(1.1, 12)}, {"", s(1.5, 12)}});
    add("crime", true, false, {{"", s(2.4)}, {"arrest", s(2.0)}});
    add("newcomer", true, false, {{"", s(1.8)}});
    add("life", true, false, {{"death", s(2.3)}, {"", s(1.7, 21)}});
    add("festival", true, false, {{"today", s(2.0)}, {"soon", s(1.9)}});
    add("player", false, false, {{"", s(1.6, -1, 7, .5)}});
    add("accent", false, true, {{"", s(1.3, -1, 9, .6, .3)}});   // + .3 for a broad accent.
    add("family", false, true, {{"", s(1.4, -1, 1)}});
    add("home", false, true, {{"housemates", s(1.4, -1, 1)}, {"", s(.7, -1, 21)}});
    add("work", false, true, {{"", s(1.1, -1, 2)}});
    add("weather", false, true, {{"", s(1.0, -1, 3)}, {"fair", s(.6, -1, 3)}});
    add("lore", false, true, {{"", s(.7, -1, 4)}});
    add("smalltalk", false, true, {{"", s(.8, -1, 5)}});
    int salt = 20;
    for (const char* kind : {"food", "home", "pastimes", "animals", "health", "memories", "dreams", "faith", "travel", "land", "season"})
    {
        if (std::string(kind) != "home")
            add(kind, false, true, {{"", s(.7, -1, salt)}});
        ++salt;
    }
    return b;
}

Book& current()
{
    static Book b = makeToday();
    return b;
}

const Score& zero()
{
    static const Score z;
    return z;
}

bool number(const json::Value& v, double& out)
{
    if (!v.isNumber())
        return false;
    out = v.asNumber();
    return true;
}
} // namespace

const Score& Book::score(const std::string& topic, const std::string& kind) const
{
    const auto t = topics.find(topic);
    if (t == topics.end())
        return zero();
    if (const auto k = t->second.kinds.find(kind); k != t->second.kinds.end())
        return k->second;
    const auto own = t->second.kinds.find("");
    return own != t->second.kinds.end() ? own->second : zero();
}

bool Book::news(const std::string& topic) const
{
    const auto t = topics.find(topic);
    return t != topics.end() && t->second.news;
}

bool Book::everyday(const std::string& topic) const
{
    const auto t = topics.find(topic);
    return t != topics.end() && t->second.everyday;
}

double Book::weight(const std::string& topic) const
{
    const auto t = topics.find(topic);
    return t != topics.end() ? t->second.weight : 1.0;
}

const std::vector<std::pair<std::string, std::string>>& sources()
{
    static const std::vector<std::pair<std::string, std::string>> all = [] {
        std::vector<std::pair<std::string, std::string>> out;
        for (const auto& [name, t] : today().topics)
            for (const auto& [kind, sc] : t.kinds)
                out.push_back({name, kind});
        return out;
    }();
    return all;
}

const Book& today()
{
    static const Book b = makeToday();
    return b;
}

const Book& book() { return current(); }

void reset() { current() = makeToday(); }

bool load(const std::string& text, std::string& problem)
{
    json::Value doc;
    std::string error;
    if (!json::parse(text, doc, error) || !doc.isObject() || !doc["topics"].isObject())
    {
        problem = "topics.json: " + (error.empty() ? "no \"topics\" object" : error);
        return false;
    }
    std::vector<std::string> errors;
    Book b;
    static const std::set<std::string> Top{"about", "topics", "jitter", "hourly", "pair_novelty", "place_saturation",
                                           "everyday_floor", "favourite", "favourites", "pair_bonus", "told_once", "topics_per_talk"};
    for (const auto& [k, v] : doc.fields())
        if (!Top.count(k))
            errors.push_back("topics.json: unknown field '" + k + "'");
    const auto num = [&](const json::Value& in, const char* key, double& out, double lo, double hi, const std::string& where) {
        const auto* v = in.find(key);
        if (!v)
            return;
        if (!number(*v, out) || out < lo || out > hi)
            errors.push_back(where + ": " + key + " is a number from " + std::to_string(lo) + " to " + std::to_string(hi));
    };
    num(doc, "jitter", b.jitter, 0, 5, "topics.json");
    num(doc, "hourly", b.hourly, 0, 5, "topics.json");
    if (const auto& n = doc["pair_novelty"]; n.isObject())
    {
        num(n, "same_topic_within_days", b.noveltyDays, 0, 60, "topics.json pair_novelty");
        num(n, "factor", b.noveltyFactor, 0, 1, "topics.json pair_novelty");
    }
    if (const auto& n = doc["place_saturation"]; n.isObject())
    {
        double per = 0;
        num(n, "same_incident_per_day", per, 0, 100, "topics.json place_saturation");
        b.saturationPerDay = int(per);
        num(n, "factor", b.saturationFactor, 0, 1, "topics.json place_saturation");
    }
    if (const auto& n = doc["everyday_floor"]; n.isObject())
    {
        double window = 0;
        num(n, "window", window, 0, 64, "topics.json everyday_floor");
        b.floorWindow = int(window);
        num(n, "news_share_over", b.floorOver, 0, 1, "topics.json everyday_floor");
        num(n, "boost", b.floorBoost, 0, 10, "topics.json everyday_floor");
    }
    num(doc, "favourite", b.favourite, 0, 5, "topics.json");
    double per = 1;
    num(doc, "topics_per_talk", per, 1, 3, "topics.json");
    b.topicsPerTalk = int(per);
    if (const auto* told = doc.find("told_once"))
    {
        if (!told->isBool())
            errors.push_back("topics.json: told_once is true or false");
        b.toldOnce = told->asBool();
    }
    static const std::set<std::string> Bands{"strangers", "acquaintances", "friends", "rivals", "family", "spouses", "apprentice", "housemates"};
    for (const auto& [band, v] : doc["pair_bonus"].fields())
    {
        if (!Bands.count(band) || !v.isNumber() || v.asNumber() < -3 || v.asNumber() > 3)
            errors.push_back("topics.json: pair_bonus." + band + ": a pair.band and a number from -3 to 3");
        b.pairBonus[band] = v.asNumber();
    }
    for (const auto& [trait, list] : doc["favourites"].fields())
        for (const auto& t : list.items())
            b.favourites[trait].push_back(t.asString(""));
    static const std::set<std::string> TopicFields{"news", "everyday", "weight", "kinds", "next", "aspects", "about"};
    static const std::set<std::string> ScoreFields{"base", "add", "traveller", "grave", "festival", "broad", "confidence",
                                                   "per", "step", "hourly", "jitter", "jitter_scale", "about"};
    for (const auto& [name, j] : doc["topics"].fields())
    {
        const auto where = "topics.json: " + name;
        if (!j.isObject())
        {
            errors.push_back(where + " is not an object");
            continue;
        }
        Topic t;
        t.name = name;
        for (const auto& [k, v] : j.fields())
            if (!TopicFields.count(k))
                errors.push_back(where + ": unknown field '" + k + "'");
        t.news = j.boolean("news");
        t.everyday = j.boolean("everyday");
        if (t.news && t.everyday)
            errors.push_back(where + " is news or everyday, not both");
        num(j, "weight", t.weight, 0, 5, where);
        for (const auto& [kind, k] : j["kinds"].fields())
        {
            const auto at = where + " kind '" + kind + "'";
            Score sc;
            for (const auto& [f, v] : k.fields())
                if (!ScoreFields.count(f))
                    errors.push_back(at + ": unknown field '" + f + "'");
            num(k, "base", sc.base, 0, 10, at);
            for (const char* extra : {"add", "traveller", "grave", "festival", "broad"})
                num(k, extra, sc.add, -5, 5, at);
            num(k, "confidence", sc.scale, 0, 5, at);
            num(k, "per", sc.per, 1, 10000, at);
            num(k, "step", sc.step, 0, 5, at);
            num(k, "jitter_scale", sc.jitterScale, 0, 5, at);
            double salt = -1;
            num(k, "hourly", salt, 0, 1000, at);
            sc.hourly = int(salt);
            salt = -1;
            num(k, "jitter", salt, 0, 1000, at);
            sc.jitter = int(salt);
            t.kinds[kind == "_" ? std::string() : kind] = sc;
        }
        for (const auto& [to, when] : j["next"].fields())
        {
            if (!when.isString())
                errors.push_back(where + ": next." + to + " is the criteria for the step (\"\" for none)");
            t.next.push_back({to, when.asString("")});
        }
        for (const auto& a : j.array("aspects"))
            t.aspects.push_back(a.asString(""));
        b.topics[name] = std::move(t);
    }
    for (const auto& [name, t] : b.topics)
        for (const auto& [to, when] : t.next)
            if (!b.topics.count(to))
                errors.push_back("topics.json: " + name + " leads to '" + to + "', which is no topic");
    for (const auto& [trait, list] : b.favourites)
        for (const auto& t : list)
            if (!b.topics.count(t))
                errors.push_back("topics.json: favourites." + trait + " names '" + t + "', which is no topic");
    std::string missing;
    for (const auto& [topic, kind] : sources())
    {
        const auto t = b.topics.find(topic);
        if (t == b.topics.end() || !t->second.kinds.count(kind))
            missing += (missing.empty() ? "" : ", ") + topic + "/" + (kind.empty() ? std::string("_") : kind);
    }
    if (!missing.empty())
        errors.push_back("topics.json: the director scores these, and they are missing: " + missing);
    if (!errors.empty())
    {
        problem.clear();
        for (std::size_t i = 0; i < errors.size() && i < 20; ++i)
            problem += (i ? "\n" : "") + errors[i];
        return false;
    }
    current() = std::move(b);
    return true;
}
} // namespace ratw::topics
