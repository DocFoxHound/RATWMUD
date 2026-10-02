#include "RatwScenes.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace ratw::scenes
{
const std::set<std::string> Topics{
    "smalltalk", "work", "family", "weather", "lore", "prices", "caravan", "bandits", "crime", "life", "festival",
    "newcomer", "player", "gossip", "news", "quarrel", "friends", "day", "bark"};
const std::set<std::string> Places{"street", "market", "tavern", "home", "work", "chapel", "barracks", "gate", "field",
                                   "shore", "wild", "hall"};
const std::set<std::string> Bands{"strangers", "acquaintances", "friends", "rivals", "family", "spouses", "apprentice"};
const std::set<std::string> Times{"dawn", "day", "dusk", "night"};
const std::set<std::string> Seasons{"spring", "summer", "autumn", "winter"};
const std::set<std::string> Days{"work", "market", "rest", "festival"};
const std::set<std::string> Weathers{"clear", "rain", "fog", "snow", "overcast", "storm", "sandstorm", "indoors"};
const std::set<std::string> Sexes{"female", "male"};
const std::set<std::string> Stages{"young", "adolescent", "adult", "old"};
const std::set<std::string> Jobs{"merchant", "smith", "baker", "brewer", "innkeeper", "guard", "soldier", "farmer",
                                 "fisher", "miner", "healer", "priest", "scholar", "laborer", "carter", "crafter",
                                 "herder", "noble", "servant", "child", "beggar", "sailor", "apprentice", "cook",
                                 "woodcutter", "official", "none"};
const std::set<std::string> Roles{"merchant", "guard", "civilian"};
const std::set<std::string> Blanks{"a", "b", "subject", "claim", "news", "item", "price", "other", "place", "town",
                                   "weekday", "season", "festival", "victim", "a_job", "b_job"};
// The tags a topic may set, and the values each may take ("*": any word).
const std::set<std::string> TopicTags{"item", "dir", "kind", "when", "known"};
namespace
{
const std::map<std::string, std::set<std::string>> TagValues{
    {"item", {"herbs", "meal"}},
    {"dir", {"up", "down"}},
    {"kind", {"arrived", "left", "late", "raided", "camp", "theft", "assault", "arrest", "marriage", "death", "birth",
              "apprentice", "succession", "crime", "loss", "work"}},
    {"when", {"today", "soon"}},
    {"known", {"yes", "no"}}};
const std::set<std::string> Regions{"upper_accord", "ridgemere", "ser_ferro", "accord_crossing", "saltreach",
                                    "cinderbrook", "westmarch", "lakeside", "fenhollow", "amberford",
                                    "hollowmere_village", "ghost_town", "northern_fortress", "isle_fortress",
                                    "dark_fortress", "wild"};
const std::set<std::string> Always{"weather", "bark"};

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

// The values a condition's key may take; false if the key isn't known. Regions are checked after groups are read.
bool known(const std::string& key, const std::set<std::string>*& values)
{
    static const std::map<std::string, const std::set<std::string>*> keys{
        {"topic", &Topics}, {"place", &Places}, {"band", &Bands}, {"time", &Times}, {"season", &Seasons},
        {"day", &Days}, {"weather", &Weathers}, {"sex", &Sexes}, {"stage", &Stages}, {"job", &Jobs}, {"role", &Roles},
        {"region", nullptr}};
    std::string bare = key.size() > 2 && (key[0] == 'a' || key[0] == 'b') && key[1] == '.' ? key.substr(2) : key;
    if (const auto t = TagValues.find(bare); t != TagValues.end())
    {
        values = &t->second;
        return true;
    }
    const auto found = keys.find(bare);
    if (found == keys.end())
        return false;
    values = found->second;
    return true;
}

// A bare word in a line's tags, as the condition it means for that line's speaker (or the moment).
bool bareWord(const std::string& w, int speaker, Condition& c)
{
    const std::string who = speaker == 0 ? "a." : "b.";
    const auto in = [&](const std::set<std::string>& set) { return set.count(w) > 0; };
    if (w == "child")
        c = {who + "stage", {"young", "adolescent"}, false};
    else if (in(Sexes))
        c = {who + "sex", {w}, false};
    else if (in(Stages))
        c = {who + "stage", {w}, false};
    else if (in(Roles) && w != "merchant")
        c = {who + "role", {w}, false};
    else if (w == "merchant")
        c = {who + "role", {w}, false};
    else if (in(Jobs))
        c = {who + "job", {w}, false};
    else if (in(Bands))
        c = {"band", {w}, false};
    else if (in(Times))
        c = {"time", {w}, false};
    else if (in(Weathers))
        c = {"weather", {w}, false};
    else
        return false;
    return true;
}
} // namespace

void Library::clear()
{
    scenes_.clear();
    byTopic_.clear();
    groups_.clear();
    ids_.clear();
}

bool Library::parse(const std::string& text, const std::string& file, std::string& problem)
{
    std::vector<std::string> errors;
    std::vector<Scene> parsed;
    std::map<std::string, std::set<std::string>> groups = groups_;
    std::istringstream in(text);
    std::string raw;
    int number = 0;
    Scene* scene = nullptr;
    const auto fail = [&](const std::string& message) { errors.push_back(file + ":" + std::to_string(number) + ": " + message); };
    const auto condition = [&](const std::string& token, Condition& c) {
        auto eq = token.find('=');
        if (eq == std::string::npos || eq == 0)
            return false;
        c.negate = token[eq - 1] == '!';
        c.key = token.substr(0, c.negate ? eq - 1 : eq);
        for (const auto& v : split(token.substr(eq + 1), '|'))
            c.values.insert(v);
        const std::set<std::string>* values = nullptr;
        if (!known(c.key, values))
        {
            fail("unknown condition '" + c.key + "'");
            return true;
        }
        if (values)
            for (const auto& v : c.values)
                if (!values->count(v))
                    fail("'" + v + "' is not a " + c.key + " (" + token + ")");
        return true;
    };
    const auto checkBlanks = [&](const std::string& t) {
        for (auto at = t.find('{'); at != std::string::npos; at = t.find('{', at + 1))
        {
            const auto end = t.find('}', at);
            if (end == std::string::npos)
            {
                fail("a { without its }");
                return;
            }
            if (!Blanks.count(t.substr(at + 1, end - at - 1)))
                fail("unknown blank {" + t.substr(at + 1, end - at - 1) + "}");
        }
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
                if (!Regions.count(r) && !groups.count(r))
                    fail("'" + r + "' is not a region");
                if (groups.count(r))
                    groups[name].insert(groups[r].begin(), groups[r].end());
                else
                    groups[name].insert(r);
            }
            continue;
        }
        if (line.rfind("scene ", 0) == 0)
        {
            parsed.push_back({});
            scene = &parsed.back();
            scene->id = trim(line.substr(6));
            scene->file = file;
            if (scene->id.empty() || scene->id.find_first_of(" \t") != std::string::npos)
                fail("a scene needs one ID");
            if (ids_.count(scene->id) || std::count_if(parsed.begin(), parsed.end(), [&](const Scene& s) { return s.id == scene->id; }) > 1)
                fail("scene " + scene->id + " is defined twice");
            continue;
        }
        if (!scene)
        {
            fail("expected 'scene <id>' first");
            continue;
        }
        if (line.rfind("when ", 0) == 0)
        {
            for (const auto& token : words(line.substr(5)))
            {
                Condition c;
                if (!condition(token, c))
                    fail("'" + token + "' is not key=value");
                else if (c.key == "topic")
                    scene->topics.insert(c.values.begin(), c.values.end());
                else
                    scene->when.push_back(c);
            }
            continue;
        }
        if (line.rfind("weight ", 0) == 0)
        {
            try
            {
                scene->weight = std::stod(line.substr(7));
            }
            catch (...)
            {
                scene->weight = -1;
            }
            if (!(scene->weight > 0 && scene->weight <= 20))
                fail("weight is a number from 0 to 20");
            continue;
        }
        // A line: who[?][tags] : alternative | alternative
        const auto colon = line.find(':');
        if (colon == std::string::npos)
        {
            fail("expected 'a : words' or 'b : words'");
            continue;
        }
        std::string head = trim(line.substr(0, colon));
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
        if (at < head.size())
        {
            if (head[at] != '[' || head.back() != ']')
            {
                fail("tags go in [ ]");
                continue;
            }
            for (const auto& token : split(head.substr(at + 1, head.size() - at - 2), ','))
                for (const auto& w : words(token))
                {
                    Condition c;
                    if (w.find('=') != std::string::npos)
                    {
                        condition(w, c);
                        if (c.key == "topic")
                            fail("a line can't test the topic");
                    }
                    else if (!bareWord(w, who, c))
                    {
                        fail("'" + w + "' is not a sex, stage, job, role, band, time or weather");
                        continue;
                    }
                    v.conditions.push_back(c);
                }
        }
        for (const auto& t : split(line.substr(colon + 1), '|'))
        {
            if (t.size() > 160)
                fail("a line of more than 160 characters");
            checkBlanks(t);
            v.texts.push_back(t);
        }
        if (v.texts.empty())
        {
            fail("a line with no words");
            continue;
        }
        // The same speaker's previous line, tagged, chooses with this one (a turn); otherwise a new turn starts.
        auto& turns = scene->turns;
        const bool joins = !turns.empty() && turns.back().who == who && !turns.back().variants.back().conditions.empty() &&
                           turns.back().optional == optional;
        if (!joins)
            turns.push_back({who, optional, {}});
        turns.back().variants.push_back(v);
    }
    for (auto& s : parsed)
    {
        number = 0;
        if (s.topics.empty())
            errors.push_back(s.file + ": scene " + s.id + " has no topic (when topic=...)");
        if (s.turns.empty())
            errors.push_back(s.file + ": scene " + s.id + " has no lines");
        bool plainA = false, plainB = false, bark = s.topics.count("bark") > 0;
        for (const auto& t : s.turns)
            if (!t.optional && t.variants.back().conditions.empty())
                (t.who == 0 ? plainA : plainB) = true;
        if (!plainA)
            errors.push_back(s.file + ": scene " + s.id + " needs a line a always says (one without tags)");
        if (!bark && !plainB)
            errors.push_back(s.file + ": scene " + s.id + " needs a line b always says (one without tags)");
        if (bark && std::any_of(s.turns.begin(), s.turns.end(), [](const Turn& t) { return t.who == 1; }))
            errors.push_back(s.file + ": scene " + s.id + " is a bark: one speaker (a) only");
        std::vector<Condition*> regionTests;
        for (auto& c : s.when)
            regionTests.push_back(&c);
        for (auto& t : s.turns)
            for (auto& v : t.variants)
                for (auto& c : v.conditions)
                    regionTests.push_back(&c);
        for (auto* cp : regionTests)
            if (auto& c = *cp; c.key == "region")
            {
                std::set<std::string> expanded;
                for (const auto& v : c.values)
                {
                    if (groups.count(v))
                        expanded.insert(groups[v].begin(), groups[v].end());
                    else if (Regions.count(v))
                        expanded.insert(v);
                    else
                        errors.push_back(s.file + ": scene " + s.id + ": '" + v + "' is not a region or group");
                }
                c.values = expanded;
            }
        s.always = std::any_of(s.topics.begin(), s.topics.end(), [](const std::string& t) { return Always.count(t) > 0; });
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
    for (auto& s : parsed)
    {
        ids_.insert(s.id);
        for (const auto& t : s.topics)
            byTopic_[t].push_back(scenes_.size());
        scenes_.push_back(std::move(s));
    }
    return true;
}

bool Library::load(const std::string& dir, std::string& problem)
{
    namespace fs = std::filesystem;
    clear();
    std::error_code ec;
    if (!fs::is_directory(dir, ec))
    {
        problem = "No scene library at " + dir + ".";
        return false;
    }
    std::vector<fs::path> files;
    for (fs::recursive_directory_iterator it(dir, ec), end; it != end && !ec; it.increment(ec))
        if (it->is_regular_file() && it->path().extension() == ".scene")
            files.push_back(it->path());
    // Groups first (every file may use them), then the scenes, in a fixed order.
    std::sort(files.begin(), files.end(), [](const fs::path& x, const fs::path& y) {
        const bool gx = x.filename() == "groups.scene", gy = y.filename() == "groups.scene";
        return gx != gy ? gx : x.generic_string() < y.generic_string();
    });
    std::string all;
    for (const auto& f : files)
    {
        std::ifstream in(f);
        std::stringstream text;
        text << in.rdbuf();
        std::string one;
        if (!parse(text.str(), fs::relative(f, dir, ec).generic_string(), one))
            all += (all.empty() ? "" : "\n") + one;
    }
    if (!all.empty())
    {
        clear();
        problem = all;
        return false;
    }
    return true;
}

bool Library::holds(const Condition& c, const Situation& s, int speaker) const
{
    std::string key = c.key;
    const Person* p = nullptr;
    if (key.size() > 2 && key[1] == '.' && (key[0] == 'a' || key[0] == 'b'))
    {
        p = key[0] == 'a' ? &s.a : &s.b;
        key = key.substr(2);
    }
    (void)speaker;
    std::string value;
    if (p)
        value = key == "sex" ? p->sex : key == "stage" ? p->stage : key == "job" ? p->job : key == "role" ? p->role : std::string();
    else if (const auto t = s.tags.find(key); t != s.tags.end())
        value = t->second;
    const bool in = c.values.count(value) > 0;
    return c.negate ? !in : in;
}

bool Library::fits(const Scene& scene, const Situation& s) const
{
    if (!scene.topics.count(s.topic))
        return false;
    return std::all_of(scene.when.begin(), scene.when.end(), [&](const Condition& c) { return holds(c, s, -1); });
}

Rendered Library::render(const Scene& scene, const Situation& s, std::uint64_t seed) const
{
    Rendered out;
    if (!fits(scene, s))
        return out;
    auto blanks = s.blanks;
    blanks["a"] = s.a.name;
    if (!s.b.name.empty())
        blanks["b"] = s.b.name;
    const auto fill = [&](const std::string& text, std::string& result) {
        result.clear();
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            if (text[i] != '{')
            {
                result += text[i];
                continue;
            }
            const auto end = text.find('}', i);
            const auto name = text.substr(i + 1, end - i - 1);
            const auto found = blanks.find(name);
            if (found == blanks.end() || found->second.empty())
                return false;
            result += found->second;
            i = end;
        }
        return true;
    };
    std::uint64_t r = mix(seed ^ hash(scene.id));
    // Alternatives are written in parallel: a line with as many alternatives as the line spoken before it answers the
    // same one (the second reply to the second remark), even past a single-line answer in between. Otherwise one at
    // random.
    std::size_t threadIndex = 0, threadCount = 0;
    for (std::size_t t = 0; t < scene.turns.size(); ++t)
    {
        const auto& turn = scene.turns[t];
        const Variant* chosen = nullptr;
        for (const auto& v : turn.variants)
            if (std::all_of(v.conditions.begin(), v.conditions.end(), [&](const Condition& c) { return holds(c, s, turn.who); }))
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
        // One alternative at random; another if its blanks can't be filled.
        r = mix(r + t);
        const std::size_t n = chosen->texts.size(), start = n == threadCount ? threadIndex : std::size_t(r % n);
        std::string said;
        bool done = false;
        std::size_t k = 0;
        for (; k < n && !done; ++k)
            done = fill(chosen->texts[(start + k) % n], said);
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
        out.lines.push_back({turn.who, said});
    }
    out.id = scene.id;
    out.always = scene.always;
    return out;
}

Rendered Library::pick(const Situation& s, std::uint64_t seed, const std::function<bool(const std::string&)>& heard,
                       const std::deque<std::string>& recent) const
{
    const auto list = byTopic_.find(s.topic);
    if (list == byTopic_.end())
        return {};
    struct Option
    {
        Rendered r;
        double weight;
    };
    std::vector<Option> fresh, old;
    for (const auto i : list->second)
    {
        const auto& scene = scenes_[i];
        if (std::find(recent.begin(), recent.end(), scene.id) != recent.end())
            continue;
        auto r = render(scene, s, seed);
        if (r.id.empty())
            continue;
        // Scenes that test more about the moment are the better fit: written for it, not for anywhere.
        const double fit = scene.weight * (1 + 0.35 * double(scene.when.size()));
        (scene.always || !heard || !heard(scene.id) ? fresh : old).push_back({std::move(r), fit});
    }
    auto& from = !fresh.empty() ? fresh : old;
    if (from.empty())
        return {};
    double total = 0;
    for (const auto& o : from)
        total += o.weight;
    double roll = double(mix(seed * 31 + 7) % 1000000) / 1000000.0 * total;
    for (auto& o : from)
    {
        if ((roll -= o.weight) <= 0)
            return std::move(o.r);
    }
    return std::move(from.back().r);
}

std::string jobCategory(const std::string& title, const std::string& label, const std::string& role, int age)
{
    if (age < 15)
        return "child";
    std::string t = title + " " + label;
    std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    const auto has = [&](std::initializer_list<const char*> words) {
        return std::any_of(words.begin(), words.end(), [&](const char* w) { return t.find(w) != std::string::npos; });
    };
    if (has({"apprentice"}))
        return "apprentice";
    if (role == "guard" || has({"watch", "guard", "sergeant", "warden", "garrison", "constable", "gaoler"}))
        return has({"garrison", "soldier", "commands the"}) ? "soldier" : "guard";
    if (has({"smith", "forge", "anvil", "armourer", "armorer", "farrier"}))
        return "smith";
    if (has({"bake", "bread", "oven"}))
        return "baker";
    if (has({"brew", "ale"}))
        return "brewer";
    if (has({"inn", "tavern", "serves at", "barkeep"}))
        return "innkeeper";
    if (has({"cook", "kitchen"}))
        return "cook";
    if (has({"farm", "field", "plough", "harvest", "orchard", "grain", "miller", "salt pan", "salter"}))
        return "farmer";
    if (has({"fish", "net", "eel", "boat", "trapper"}))
        return "fisher";
    if (has({"sail", "ship", "harbour", "dock", "wharf", "quay", "rope"}))
        return "sailor";
    if (has({"mine", "miner", "ore", "quarry", "pit", "charcoal", "peat"}))
        return "miner";
    if (has({"heal", "bonesetter", "physician", "apothecary", "herbalist", "infirmary", "mending"}))
        return "healer";
    if (has({"priest", "chapel", "shrine", "cathedral", "acolyte", "clergy", "saint", "keeper of the"}))
        return "priest";
    if (has({"scribe", "scholar", "record", "librar", "clerk", "teacher", "cartograph", "reading"}))
        return "scholar";
    if (has({"shepherd", "drover", "herd", "stable", "sheep", "cattle"}))
        return "herder";
    if (has({"wood", "logger", "timber", "sawmill", "reed cutter"}))
        return "woodcutter";
    if (has({"carter", "carries", "porter", "caravan", "wagon", "loads"}))
        return "carter";
    if (has({"lord", "lady", "house grayrock", "noble", "prince", "heir", "palazzo"}))
        return "noble";
    if (has({"servant", "maid", "valet", "footman", "steward"}))
        return "servant";
    if (has({"beg"}))
        return "beggar";
    if (has({"council", "magistrate", "toll", "official", "tax", "court", "chancellor"}))
        return "official";
    if (has({"carpenter", "cooper", "tailor", "weaver", "potter", "mason", "tanner", "chandler", "tinker", "jewel",
             "fletcher", "netmak", "boatwright", "glass", "rope", "lamp"}))
        return "crafter";
    if (has({"labour", "labor", "sweep", "washes", "messenger", "lamplighter", "errand", "dig", "haul"}))
        return "laborer";
    if (role == "merchant" || has({"shop", "keeps", "seller", "trader", "merchant", "stall", "store", "goods"}))
        return "merchant";
    if (age >= 62)
        return "none";
    return "none";
}
} // namespace ratw::scenes
