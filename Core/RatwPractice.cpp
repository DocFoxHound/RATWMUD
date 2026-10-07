#include "RatwPractice.h"

#include "RatwCreation.h"
#include "RatwJsonDoc.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>

namespace ratw::practice
{
namespace
{
struct Catalog
{
    bool loaded = false;
    std::string error;
    std::vector<Skill> skills;
    std::vector<Source> sources;
    Rules rules;
};

// Data/Progression: RATW_DATA_DIR (the Data directory), else the working directory or one above it, else the source
// tree (as the Gift catalog is found: RatwGifts.cpp).
std::filesystem::path skillsFile()
{
    namespace fs = std::filesystem;
    std::error_code ec;
    if (const char* dir = std::getenv("RATW_DATA_DIR"); dir && *dir)
        return fs::path(dir) / "Progression" / "skills.json";
    for (auto at = fs::current_path(ec); !ec && !at.empty(); at = at.parent_path())
    {
        if (fs::exists(at / "Data" / "Progression" / "skills.json", ec))
            return at / "Data" / "Progression" / "skills.json";
        if (at == at.parent_path())
            break;
    }
#ifdef RATW_SOURCE_DIR
    return fs::path(RATW_SOURCE_DIR) / "Data" / "Progression" / "skills.json";
#else
    return fs::path("Data") / "Progression" / "skills.json";
#endif
}

Skill readSkill(const json::Value& f, bool attribute)
{
    Skill s;
    s.id = f.string("id");
    s.name = f.string("name");
    s.shortName = f.string("short", s.name);
    s.field = f.string("field");
    s.attribute = attribute;
    s.start = f.number("start");
    s.cap = f.number("cap", 100);
    s.quickenedCap = f.number("quickenedCap", -1);
    s.specialtyStart = f.number("specialtyStart", -1);
    s.softPerDay = f.number("softPerDay", 1);
    s.lineStep = std::max(1e-6, f.number("lineStep", 1));
    s.percent = f.string("format") == "percent";
    s.line = f.string("line");
    return s;
}

Catalog build()
{
    Catalog c;
    const auto path = skillsFile();
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        c.error = "cannot read " + path.string();
        return c;
    }
    std::stringstream text;
    text << in.rdbuf();
    json::Value doc;
    if (!json::parse(text.str(), doc, c.error))
    {
        c.error = path.string() + ": " + c.error;
        return c;
    }
    for (const auto& f : doc.array("attributes"))
        c.skills.push_back(readSkill(f, true));
    for (const auto& f : doc.array("skills"))
        c.skills.push_back(readSkill(f, false));
    for (const auto& s : c.skills)
        if (s.id.empty() || s.cap <= 0)
        {
            c.error = path.string() + ": every attribute and skill needs an id and a cap";
            c.skills.clear();
            return c;
        }
    for (const auto& f : doc.array("sources"))
    {
        Source s;
        s.id = f.string("id");
        s.byPartner = f.boolean("byPartner");
        s.partnerDecay = f.boolean("partnerDecay");
        for (const auto& [skill, base] : f.object("grows").fields())
            s.grows.emplace_back(skill, base.asNumber(0));
        if (!s.id.empty())
            c.sources.push_back(std::move(s));
    }
    auto& r = c.rules;
    r.roomFloor = doc.object("room").number("floor", r.roomFloor);
    r.softDay = doc.object("soft").number("day", r.softDay);
    r.softAfter = doc.object("soft").number("after", r.softAfter);
    const auto& partners = doc.object("partners");
    for (const char* kind : {"player", "resident", "animal", "fierce", "post"})
        r.partnerKinds[kind] = partners.number(kind, 1);
    r.weakerBy = partners.number("weakerBy", r.weakerBy);
    r.weaker = partners.number("weaker", r.weaker);
    r.betterBy = partners.number("betterBy", r.betterBy);
    r.better = partners.number("better", r.better);
    const auto& teachers = doc.object("teachers");
    r.teachReach = teachers.number("reach", r.teachReach);
    r.teachBetterBy = teachers.number("betterBy", r.teachBetterBy);
    r.teachNearby = teachers.number("nearby", r.teachNearby);
    r.teachCache = teachers.number("cacheSeconds", r.teachCache);
    for (const char* kind : {"mentor", "trainer", "master"})
        r.teacherKinds[kind] = teachers.number(kind, 1);
    const auto& rested = doc.object("rested");
    r.restedAwayFor = rested.number("awayFor", r.restedAwayFor);
    r.restedPerDay = rested.number("perDayAway", r.restedPerDay);
    r.restedMost = rested.number("most", r.restedMost);
    r.restedRate = rested.number("rate", r.restedRate);
    const auto& variety = doc.object("variety");
    r.occasion = variety.number("occasion", r.occasion);
    r.varietyWindow = variety.number("window", r.varietyWindow);
    r.varietyRepeats = int(variety.number("repeats", r.varietyRepeats));
    r.varietyFactor = variety.number("factor", r.varietyFactor);
    r.ring = std::max(1, int(variety.number("ring", r.ring)));
    r.block = std::max(1.0, variety.number("block", r.block));
    if (!doc.array("partnerDecay").empty())
    {
        r.partnerDecay.clear();
        for (const auto& v : doc.array("partnerDecay"))
            r.partnerDecay.push_back(v.asNumber(0));
    }
    r.lineThrottle = doc.object("lines").number("throttle", r.lineThrottle);
    r.capLine = doc.object("lines").string("cap", "Your {name} is as good as it will get.");
    r.seasoned = doc.object("bands").number("seasoned", r.seasoned);
    r.veteran = doc.object("bands").number("veteran", r.veteran);
    c.loaded = true;
    return c;
}

const Catalog& catalog()
{
    static std::once_flag once;
    static Catalog c;
    std::call_once(once, [] { c = build(); });
    return c;
}

std::string replaced(std::string text, const std::string& key, const std::string& with)
{
    for (auto at = text.find(key); at != std::string::npos; at = text.find(key, at + with.size()))
        text.replace(at, key.size(), with);
    return text;
}
} // namespace

bool load(std::string* error)
{
    const auto& c = catalog();
    if (error)
        *error = c.error;
    return c.loaded;
}

const std::vector<Skill>& skills()
{
    return catalog().skills;
}

const Skill* skill(const std::string& id)
{
    for (const auto& s : catalog().skills)
        if (s.id == id)
            return &s;
    return nullptr;
}

const Source* source(const std::string& id)
{
    for (const auto& s : catalog().sources)
        if (s.id == id)
            return &s;
    return nullptr;
}

const Rules& rules()
{
    return catalog().rules;
}

double capFor(const Skill& s, bool quickened, const std::string& grade)
{
    if (s.attribute)
        if (const auto a = creation().grades.find(s.id); a != creation().grades.end())
            if (const auto g = a->second.find(grade.empty() ? "plain" : grade); g != a->second.end())
                return g->second.cap;
    return quickened && s.quickenedCap > 0 ? s.quickenedCap : s.cap;
}

double room(double value, double cap, const Rules& r)
{
    if (cap <= 0 || value >= cap)
        return 0;
    return std::max(r.roomFloor, (cap - value) / cap);
}

double soft(PracticeState::Day& day, const Skill& s, double now, const Rules& r)
{
    if (now - day.start >= r.softDay || now < day.start)
        day = {now, 0};
    return day.gained < s.softPerDay ? 1 : r.softAfter;
}

double partnerFactor(const std::string& kind, double partnerValue, double value, const Rules& r)
{
    double f = 1;
    if (const auto k = r.partnerKinds.find(kind); k != r.partnerKinds.end())
        f = k->second;
    if (partnerValue >= 0)
    {
        if (partnerValue <= value - r.weakerBy)
            f *= r.weaker;
        else if (partnerValue >= value + r.betterBy)
            f *= r.better;
    }
    return f;
}

double variety(PracticeState& p, const std::string& key, const std::string& occasion, double now, const Rules& r)
{
    // The same occasion as the latest of this key: nothing new to record.
    const PracticeState::Seen* latest = nullptr;
    for (const auto& s : p.ring)
        if (s.key == key && (!latest || s.at >= latest->at))
            latest = &s;
    const bool same = latest && (occasion.empty() ? now - latest->at < r.occasion : latest->occasion == occasion);
    if (!same)
    {
        p.ring.push_back({key, occasion, now});
        if (int(p.ring.size()) > r.ring)
            p.ring.erase(p.ring.begin());
    }
    else if (occasion.empty())
        for (auto& s : p.ring)
            if (&s == latest)
                s.at = now;                         // (A continuing occasion stays one occasion.)
    int seen = 0;
    for (const auto& s : p.ring)
        if (s.key == key && now - s.at < r.varietyWindow)
            ++seen;
    return seen > r.varietyRepeats ? r.varietyFactor : 1;
}

double partnerDecay(PracticeState& p, const std::string& partner, const std::string& occasion, double now, const Rules& r)
{
    if (partner.empty() || r.partnerDecay.empty())
        return 1;
    if (p.partnersDay < 0 || now - p.partnersDay >= r.softDay || now < p.partnersDay)
    {
        p.partners.clear();
        p.partnersDay = now;
    }
    auto& occasions = p.partners[partner];
    const auto name = occasion.empty() ? "t" + std::to_string(std::int64_t(std::floor(now / std::max(1.0, r.occasion)))) : occasion;
    auto at = std::find(occasions.begin(), occasions.end(), name);
    if (at == occasions.end())
    {
        occasions.push_back(name);
        at = occasions.end() - 1;
    }
    const auto i = std::size_t(at - occasions.begin());
    return r.partnerDecay[std::min(i, r.partnerDecay.size() - 1)];
}

double rested(PracticeState& p, double gain, double now, const Rules& r)
{
    if (p.lastGainAt >= 0 && now - p.lastGainAt >= r.restedAwayFor)
        p.rested = std::min(r.restedMost, p.rested + r.restedPerDay * (now - p.lastGainAt) / 86400);
    if (gain <= 0)
        return 0;
    p.lastGainAt = now;
    const double extra = std::min(gain * r.restedRate, std::max(0.0, p.rested));
    p.rested -= extra;
    return extra;
}

int lineDue(PracticeState& p, const Skill& s, double before, double after, double now, const Rules& r)
{
    const int was = int(std::floor(before / s.lineStep + 1e-9)), is = int(std::floor(after / s.lineStep + 1e-9));
    auto told = p.told.find(s.id);
    if (told == p.told.end())
        told = p.told.emplace(s.id, std::make_pair(-1e18, was)).first;
    if (is <= told->second.second || now - told->second.first < r.lineThrottle)
        return -1;
    told->second = {now, is};
    return is;
}

std::string lineText(const Skill& s, double value)
{
    const auto shown = s.percent ? std::to_string(int(std::lround(value * 100))) + "%" : std::to_string(int(std::floor(value + 1e-9)));
    return replaced(replaced(s.line, "{value}", shown), "{name}", s.name);
}

namespace
{
Standing buildStanding()
{
    Standing st;
    auto path = skillsFile().parent_path() / "standing.json";
    std::ifstream in(path, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    json::Value doc;
    std::string error;
    if (!in || !json::parse(text.str(), doc, error))
        return st;
    st.stepBase = std::max(1.0, doc.object("curve").number("stepBase", st.stepBase));
    st.stepGrowth = std::max(0.0, doc.object("curve").number("stepGrowth", st.stepGrowth));
    if (!doc.array("titles").empty())
    {
        st.titles.clear();
        for (const auto& t : doc.array("titles"))
            st.titles.emplace_back(int(t.number("level", 1)), t.string("title"));
        std::sort(st.titles.begin(), st.titles.end());
    }
    if (!doc.array("socialReasons").empty())
    {
        st.socialReasons.clear();
        for (const auto& r : doc.array("socialReasons"))
            st.socialReasons.push_back(r.asString({}));
    }
    st.dailyCap = int(doc.number("dailyCap", st.dailyCap));
    return st;
}
} // namespace

const Standing& standing()
{
    static std::once_flag once;
    static Standing st;
    std::call_once(once, [] { st = buildStanding(); });
    return st;
}

long long xpFor(int level)
{
    const auto& st = standing();
    long long xp = 0;
    for (int l = 1; l < level; ++l)
        xp += (long long)std::llround(st.stepBase + st.stepGrowth * (l - 1));
    return xp;
}

int levelFor(long long xp)
{
    const auto& st = standing();
    long long reached = 0;
    int level = 1;
    for (; level < 10000; ++level)
    {
        const auto step = (long long)std::llround(st.stepBase + st.stepGrowth * (level - 1));
        if (reached + step > xp)
            break;
        reached += step;
    }
    return level;
}

std::string titleFor(int level)
{
    std::string title = "Stranger";
    for (const auto& [at, name] : standing().titles)
        if (level >= at)
            title = name;
    return title;
}

bool socialReason(const std::string& reason)
{
    const auto& reasons = standing().socialReasons;
    return std::find(reasons.begin(), reasons.end(), reason) != reasons.end();
}

namespace
{
json::Value& catalogStore()
{
    static json::Value catalog = json::Value::object();
    return catalog;
}

Creation buildCreation()
{
    Creation c;
    auto& catalog = catalogStore();
    auto path = skillsFile().parent_path() / "creation.json";
    std::ifstream in(path, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    json::Value doc;
    std::string error;
    if (!in || !json::parse(text.str(), doc, error))
        return c;
    for (const auto& [attribute, grades] : doc.object("grades").fields())
        for (const auto& [grade, pair] : grades.fields())
            if (pair.isArray() && pair.items().size() == 2)
                c.grades[attribute][grade] = {pair.items()[0].asNumber(0), pair.items()[1].asNumber(0)};
    c.budget = int(doc.number("budget", c.budget));
    c.strongCost = int(doc.number("strongCost", c.strongCost));
    c.weakRefund = int(doc.number("weakRefund", c.weakRefund));
    c.mostStrong = int(doc.number("mostStrong", c.mostStrong));
    c.mostWeak = int(doc.number("mostWeak", c.mostWeak));
    c.capBonus = doc.number("capBonus", 0);
    for (const auto& s : doc.array("specialties"))
        c.specialties.push_back({s.string("id"), s.string("name"), s.string("skill")});
    for (const auto& [tier, cost] : doc.object("tierCost").fields())
        c.tierCost[tier] = int(cost.asNumber(0));
    const auto& st = doc.object("stamina");
    c.recoveryBase = st.number("recoveryBase", c.recoveryBase);
    c.recoveryPer = st.number("recoveryPer", c.recoveryPer);
    c.drainBase = st.number("drainBase", c.drainBase);
    c.drainPer = st.number("drainPer", c.drainPer);
    c.turnPer = st.number("turnPer", c.turnPer);
    // The creator's copy: each attribute with its name and grades, and the rest as the file has it.
    auto attributes = json::Value::array();
    for (const auto& s : skills())
        if (s.attribute)
            if (const auto g = c.grades.find(s.id); g != c.grades.end())
            {
                auto row = json::Value::object();
                row.add("id", s.id);
                row.add("name", s.name);
                row.add("short", s.shortName);
                if (s.percent)
                    row.add("percent", true);
                for (const auto& [grade, v] : g->second)
                {
                    auto pair = json::Value::array();
                    pair.push(v.start);
                    pair.push(v.cap);
                    row.add(grade, pair);
                }
                attributes.push(row);
            }
    catalog.add("attributes", attributes);
    for (const char* key : {"budget", "strongCost", "weakRefund", "mostStrong", "mostWeak"})
        catalog.add(key, doc.number(key));
    catalog.add("specialties", doc["specialties"]);
    catalog.add("presets", doc["presets"]);
    catalog.add("tierCost", doc["tierCost"]);
    return c;
}
} // namespace

const Creation& creation()
{
    static std::once_flag once;
    static Creation c;
    std::call_once(once, [] { c = buildCreation(); });
    return c;
}

const json::Value& creationCatalog()
{
    creation();                                     // (Built with the catalog.)
    return catalogStore();
}

const Specialty* specialty(const std::string& id)
{
    for (const auto& s : creation().specialties)
        if (s.id == id)
            return &s;
    return nullptr;
}

bool checkBuild(const json::Value& build, const std::string& tier, Build& out, std::string& error)
{
    const auto& c = creation();
    out = {};
    if (!build.isObject())
    {
        error = "A build is a set of grades and a specialty.";
        return false;
    }
    for (const auto& [key, v] : build.fields())
        if (key != "grades" && key != "specialty")
        {
            error = "Unknown part of a build: " + key + ".";
            return false;
        }
    int strong = 0, weak = 0;
    for (const auto& [attribute, grade] : build.object("grades").fields())
    {
        const auto g = grade.asString({});
        if (!c.grades.count(attribute) || (g != "weak" && g != "plain" && g != "strong"))
        {
            error = "Each attribute is weak, plain or strong.";
            return false;
        }
        if (g != "plain")
            out.grades[attribute] = g;
        strong += g == "strong";
        weak += g == "weak";
    }
    if (build.has("grades") && !build["grades"].isObject())
    {
        error = "Each attribute is weak, plain or strong.";
        return false;
    }
    if (strong > c.mostStrong || weak > c.mostWeak)
    {
        error = "At most " + std::to_string(c.mostStrong) + " strengths and " + std::to_string(c.mostWeak) + " weaknesses.";
        return false;
    }
    const auto cost = c.tierCost.find(tier);
    const int spent = strong * c.strongCost - weak * c.weakRefund + (cost == c.tierCost.end() ? 0 : cost->second);
    if (spent > c.budget)
    {
        error = "That build costs " + std::to_string(spent) + " points; there are " + std::to_string(c.budget) + ".";
        return false;
    }
    if (const auto* s = build.find("specialty"))
    {
        out.specialty = s->asString({});
        if (!s->isString() || (!out.specialty.empty() && !specialty(out.specialty)))
        {
            error = "Not a specialty.";
            return false;
        }
    }
    return true;
}

double staminaRecovery(double endurance)
{
    return std::max(0.0, creation().recoveryBase + creation().recoveryPer * endurance);
}

double staminaDrain(double endurance)
{
    return std::max(.1, creation().drainBase - creation().drainPer * endurance);
}

double staminaPerTurnExtra(double endurance)
{
    return creation().turnPer * (endurance - 50);
}

std::string capText(const Skill& s)
{
    auto name = s.name;
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) { return char(std::tolower(ch)); });
    return replaced(rules().capLine, "{name}", name);
}
} // namespace ratw::practice
