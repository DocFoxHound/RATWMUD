// Mantles passed on (Docs/Design/84-living-conversations.md §12, Phase 10): the mantle ledger, a death's release, a
// birth's choice (an echo, a rested mantle nobody in the birth town knew, any rested one with its marks drawn again, a
// spare, a neighbouring pool's, else none), a child's name from its dialect's pool, an arrival's binding, and the
// pools' health. The host (Game, World) writes what the ledger says to the world database and the wearer's voice; this
// file decides. No NPC is ever made here: a mantle goes to a child born in place of one who died, or to one of the
// Country's own coming in, and births stay one for one with deaths, realm-wide (the user's Q5).
#include "RatwSociety.h"
#include "RatwCalendar.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iterator>
#include <set>
#include <sstream>
#include <tuple>

namespace ratw
{
namespace
{
std::uint64_t mixOf(const std::string& a, std::int64_t b)
{
    return std::hash<std::string>{}(a) * 1099511628211ULL + std::uint64_t(b) * 2654435761ULL;
}
double lotOf(const std::string& words)
{
    return double(std::hash<std::string>{}(words) % 100000) / 100000;
}
std::string trim(const std::string& s)
{
    const auto a = s.find_first_not_of(" \t\r");
    const auto b = s.find_last_not_of(" \t\r");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}
std::vector<std::string> split(const std::string& s, char by)
{
    std::vector<std::string> out;
    std::string part;
    std::istringstream in(s);
    while (std::getline(in, part, by))
        out.push_back(trim(part));
    return out;
}
std::string firstName(const std::string& name)
{
    return name.substr(0, name.find(' '));
}
std::string surnameOf(const std::string& name)
{
    const auto space = name.find_last_of(' ');
    return space == std::string::npos ? std::string() : name.substr(space + 1);
}
// Placeholders for a dialect with no pool (and no plain one): the 28 the born were given before.
const char* const Placeholders[] = {"Alder", "Briar", "Cinder", "Dusk", "Ember", "Fennel", "Gale", "Hazel", "Ivy", "Juniper",
                                    "Kestrel", "Linden", "Moss", "Nettle", "Oriel", "Pike", "Quill", "Rowan", "Sorrel",
                                    "Thorn", "Umber", "Vesper", "Willow", "Yarrow", "Wren", "Tansy", "Bramble", "Sedge"};
int stageIndex(const std::string& stage)
{
    static const char* const Stages[] = {"young", "adolescent", "adult", "old"};
    for (int i = 0; i < 4; ++i)
        if (stage == Stages[i])
            return i;
    return -1;
}
} // namespace

bool readNamePool(const std::string& text, const std::string& where, std::string& dialect, NamePool& out,
                  std::vector<std::string>& problems)
{
    // names <dialect>
    // female = Wenna | Ottilie | ...
    // male = Orrin | Nab | ...
    // named_for = .25
    out = {};
    dialect.clear();
    std::istringstream in(text);
    std::string line;
    int at = 0;
    const auto bad = [&](const std::string& why) { problems.push_back(where + ":" + std::to_string(at) + ": " + why); };
    while (std::getline(in, line))
    {
        ++at;
        if (const auto hash = line.find('#'); hash != std::string::npos)
            line = line.substr(0, hash);
        line = trim(line);
        if (line.empty())
            continue;
        if (line.rfind("names ", 0) == 0)
        {
            dialect = trim(line.substr(6));
            if (dialect.empty() || dialect.find_first_not_of("abcdefghijklmnopqrstuvwxyz_") != std::string::npos)
                bad("a pool is named for a dialect, lower-case: names coldwater");
            continue;
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos)
        {
            bad("not 'female = ...', 'male = ...' or 'named_for = ...'");
            continue;
        }
        const auto key = trim(line.substr(0, eq)), value = trim(line.substr(eq + 1));
        if (key == "named_for")
        {
            char* end = nullptr;
            const double v = std::strtod(value.c_str(), &end);
            if (!end || *end || !(v >= 0 && v <= 1))
                bad("named_for is a share from 0 to 1");
            else
                out.namedFor = v;
            continue;
        }
        if (key != "female" && key != "male")
        {
            bad("'" + key + "' is not female, male or named_for (characters are male or female only)");
            continue;
        }
        auto& list = key == "female" ? out.female : out.male;
        for (const auto& name : split(value, '|'))
        {
            const bool word = !name.empty() && std::isupper(static_cast<unsigned char>(name[0])) &&
                              std::all_of(name.begin() + 1, name.end(), [](char c) { return std::islower(static_cast<unsigned char>(c)) || c == '\''; });
            if (!word)
                bad("'" + name + "' is not a first name (one word, a capital and then small letters)");
            else if (std::find(list.begin(), list.end(), name) != list.end())
                bad(name + " is in the " + key + " names twice");
            else
                list.push_back(name);
        }
    }
    if (dialect.empty())
        problems.push_back(where + ": no 'names <dialect>' line");
    if (out.female.empty() || out.male.empty())
        problems.push_back(where + ": a pool needs female and male names");
    for (const auto& f : out.female)
        if (std::find(out.male.begin(), out.male.end(), f) != out.male.end())
            problems.push_back(where + ": " + f + " is both a female and a male name");
    return problems.empty();
}

void Society::setMantleCards(std::vector<PoolMantle> cards)
{
    mantleCards_.clear();
    for (auto& c : cards)
        if (!c.id.empty())
            mantleCards_[c.id] = std::move(c);
}

const PoolMantle* Society::mantleCard(const std::string& id) const
{
    const auto found = mantleCards_.find(id);
    return found == mantleCards_.end() ? nullptr : &found->second;
}

const MantleBinding* Society::mantleOf(const std::string& wearer) const
{
    const auto found = state_.careers.mantles.find(wearer);
    return found == state_.careers.mantles.end() ? nullptr : &found->second;
}

std::string Society::wearerOf(const std::string& mantle) const
{
    for (const auto& [wearer, b] : state_.careers.mantles)
        if (b.mantle == mantle && b.releasedDay < 0 && !dead_.count(wearer))
            return wearer;
    return {};
}

bool Society::fitsMantle(const std::string& fits, const std::map<std::string, std::string>& facts, int age)
{
    // Hard criteria (§7.9's `fits`), in the talk criteria's words: fact=value|value, fact!=value, age>=N... A stage holds
    // if it is the wearer's now or one they will grow to; an age compared upward holds the same way.
    const int now = stageIndex(lifeStageName(lifeStage(age)));
    std::istringstream in(fits);
    for (std::string token; in >> token;)
    {
        if (token.rfind("self.", 0) == 0)
            token = token.substr(5);
        std::string name, op, value;
        if (const auto at = token.find_first_of("=<>!"); at == std::string::npos)
        {
            // A bare word: a stage or a sex.
            if (stageIndex(token) >= 0)
                name = "stage", op = "=", value = token;
            else if (token == "male" || token == "female")
                name = "sex", op = "=", value = token;
            else
                return false;
        }
        else
        {
            name = token.substr(0, at);
            auto rest = token.substr(at);
            for (const char* o : {"!=", ">=", "<=", "=", ">", "<"})
                if (rest.rfind(o, 0) == 0)
                {
                    op = o;
                    value = rest.substr(op.size());
                    break;
                }
            if (op.empty())
                return false;
        }
        const auto values = split(value, '|');
        if (name == "stage")
        {
            bool reachable = false, allListed = true;
            for (int s = std::max(now, 0); s < 4; ++s)
            {
                static const char* const Names[] = {"young", "adolescent", "adult", "old"};
                const bool listed = std::find(values.begin(), values.end(), Names[s]) != values.end();
                reachable = reachable || listed;
                allListed = allListed && listed;
            }
            if ((op == "=" && !reachable) || (op == "!=" && allListed) || (op != "=" && op != "!="))
                return false;
            continue;
        }
        if (name == "age")
        {
            char* end = nullptr;
            const double n = std::strtod(value.c_str(), &end);
            if (!end || *end)
                return false;
            const bool holds = op == ">=" || op == ">" ? true                        // (They will be.)
                               : op == "<"            ? age < n
                               : op == "<="           ? age <= n
                               : op == "="            ? age <= n
                                                      : !(age == n);
            if (!holds)
                return false;
            continue;
        }
        const auto f = facts.find(name);
        const bool listed = f != facts.end() && std::find(values.begin(), values.end(), f->second) != values.end();
        if ((op == "=" && !listed) || (op == "!=" && listed) || (op != "=" && op != "!="))
            return false;
    }
    return true;
}

bool Society::kinOf(const std::string& a, const std::string& b) const
{
    if (a.empty() || b.empty() || a == b)
        return false;
    const auto& c = state_.careers;
    if (const auto s = c.spouses.find(a); s != c.spouses.end() && s->second == b)
        return true;
    const auto parentsOf = [&](const std::string& id) {
        const auto found = c.parents.find(id);
        return found == c.parents.end() ? std::vector<std::string>{} : found->second;
    };
    const auto pa = parentsOf(a), pb = parentsOf(b);
    const auto in = [](const std::vector<std::string>& v, const std::string& x) { return std::find(v.begin(), v.end(), x) != v.end(); };
    if (in(pa, b) || in(pb, a))
        return true;                                // (Parent and child.)
    for (const auto& p : pa)
        if (in(pb, p) || in(parentsOf(p), b))
            return true;                            // (Siblings; b a grandparent of a.)
    for (const auto& p : pb)
        if (in(parentsOf(p), a))
            return true;                            // (a a grandparent of b.)
    return false;
}

namespace
{
// What the ledger says of each mantle: worn (by a living wearer, or asked for by a request on its way), or resting since
// the day its last wearer let it go (and who that was, and who knew them).
struct MantleStatus
{
    bool worn = false, ever = false;
    double released = -1;
    std::string last;
    const std::vector<std::string>* knownBy = nullptr;
};
} // namespace

bool Society::chooseMantle(ResidentRequest& r, const std::string& wearer, const std::map<std::string, std::string>& facts, int age,
                           double now, const CareerWorld& world)
{
    if (mantleCards_.empty() || r.dialect.empty())
    {
        if (r.kind == "birth")
            mantleChanges_.push_back({"none", wearer, {}, r.kind, r.dialect.empty() ? std::string("(no dialect)") : r.dialect, now});
        return false;
    }
    std::map<std::string, MantleStatus> status;
    const auto alive = [&](const std::string& id) { return !dead_.count(id) && (!world.alive || world.alive(id)); };
    for (const auto& [who, b] : state_.careers.mantles)
    {
        if (b.mantle.empty())
            continue;
        auto& s = status[b.mantle];
        s.ever = true;
        if (b.releasedDay < 0 && alive(who) && who != wearer)
            s.worn = true;
        // (A wearer dead with no release written: resting since they were bound.)
        const double let = b.releasedDay >= 0 ? b.releasedDay : alive(who) ? -1 : b.boundDay;
        if (let >= 0 && let >= s.released)
            s.released = let, s.last = who, s.knownBy = &b.knownBy;
    }
    for (const auto& q : requests_)
        if (!q.mantle.empty())
            status[q.mantle].worn = true;           // (Asked for already, not made yet.)
    const bool birth = r.kind == "birth";
    const auto townOf = [&](const std::string& cell) { return world.townOf ? world.townOf(cell) : std::string(); };
    const auto birthTown = townOf(r.home.cell);
    // The newborn's household (its mother's), for "never given in the household of its last wearer's kin".
    std::vector<std::string> household = r.parents;
    if (birth && !r.templateId.empty())
        if (const auto* mother = resident(r.templateId))
            for (const auto& [id, life] : state_.residents)
                if (life.homeCell == mother->homeCell && alive(id) && !Society::communalHome(life.homeCell))
                    household.push_back(id);
    const auto kinOfAny = [&](const std::string& last, const std::vector<std::string>& of) {
        return std::any_of(of.begin(), of.end(), [&](const std::string& p) { return kinOf(last, p); });
    };
    const auto rested = [&](const MantleStatus& s) { return s.ever && !s.worn && s.released >= 0 && now - s.released >= mantleRules_.restDays; };
    const auto tryPool = [&](const std::string& pool, bool fallback) {
        std::vector<std::pair<const PoolMantle*, const MantleStatus*>> open;
        static const MantleStatus None{};
        for (const auto& [id, card] : mantleCards_)
        {
            if (card.dialect != pool)
                continue;
            const auto found = status.find(id);
            const auto& s = found == status.end() ? None : found->second;
            if (s.worn || !fitsMantle(card.fits, facts, age))
                continue;
            open.push_back({&card, &s});
        }
        const auto take = [&](const PoolMantle* card, const MantleStatus* s, const std::string& how, bool redraw) {
            r.mantle = card->id;
            r.mantleHow = r.kind == "birth" ? how : "arrival";
            r.echoOf = how == "echo" ? s->last : std::string();
            r.redrawMarks = redraw || fallback;
            r.markDialect = r.dialect;              // (Drawn again from the wearer's own dialect's bank.)
            r.voice.dialect = pool;
            return true;
        };
        // Longest rested first, then a lot.
        const auto longest = [&](const auto& keep) {
            const std::pair<const PoolMantle*, const MantleStatus*>* best = nullptr;
            for (const auto& o : open)
                if (rested(*o.second) && keep(o) &&
                    (!best || std::tie(o.second->released, o.first->id) < std::tie(best->second->released, best->first->id)))
                    best = &o;
            return best;
        };
        // 1. An echo: its last wearer was kin of a parent (a birth's, never a fallback's).
        if (birth && !fallback)
            if (const auto* o = longest([&](const auto& o) { return kinOfAny(o.second->last, r.parents); }))
                return take(o->first, o->second, "echo", false);
        const auto notKinHere = [&](const auto& o) { return !kinOfAny(o.second->last, household) || household.empty(); };
        // 2. Nobody in the birth town knew its last wearer: its marks kept.
        if (const auto* o = longest([&](const auto& o) {
                if (!notKinHere(o))
                    return false;
                if (!o.second->knownBy)
                    return true;
                return std::none_of(o.second->knownBy->begin(), o.second->knownBy->end(), [&](const std::string& k) {
                    const auto* life = resident(k);
                    return alive(k) && (birthTown.empty() || (life && townOf(life->homeCell) == birthTown));
                });
            }))
            return take(o->first, o->second, "reused", false);
        // 3. Any rested one: its marks drawn again, for those who remember them.
        if (const auto* o = longest(notKinHere))
            return take(o->first, o->second, "reused", true);
        // 4. A spare, by a lot.
        const std::pair<const PoolMantle*, const MantleStatus*>* spare = nullptr;
        double best = 2;
        for (const auto& o : open)
            if (!o.second->ever)
                if (const double l = lotOf(o.first->id + "|" + r.name + "|" + std::to_string(std::int64_t(now))); l < best)
                    best = l, spare = &o;
        if (spare)
            return take(spare->first, spare->second, "spare", false);
        return false;
    };
    if (tryPool(r.dialect, false))
        return true;
    // 5. A neighbouring pool (settings.json's mantle_fallback), its marks drawn from the wearer's own dialect.
    std::set<std::string> seen{r.dialect};
    for (auto at = mantleRules_.fallback.find(r.dialect); at != mantleRules_.fallback.end() && seen.insert(at->second).second;
         at = mantleRules_.fallback.find(at->second))
        if (tryPool(at->second, true))
            return true;
    // 6. Nothing: born (or come) with the voice it would have had, and said so. Never delayed for want of one.
    mantleChanges_.push_back({"none", wearer, {}, r.kind, r.dialect, now});
    return false;
}

std::string Society::childName(const std::string& dialect, const std::string& sex, const std::string& surname,
                               const std::vector<std::string>& parents, const std::string& inPlaceOf, const std::string& key,
                               std::int64_t today, const CareerWorld& world) const
{
    // A name from the dialect's pool (§7.10; plain's for a dialect without one), unique: no two living residents share a
    // full name, and no living parent or sibling shares the child's first name. One born in place of a dead grandparent
    // takes their first name named_for of the time, where it is free.
    const auto alive = [&](const std::string& id) { return !dead_.count(id) && (!world.alive || world.alive(id)); };
    const NamePool* pool = nullptr;
    if (const auto found = mantleRules_.names.find(dialect); found != mantleRules_.names.end())
        pool = &found->second;
    else if (const auto plain = mantleRules_.names.find("plain"); plain != mantleRules_.names.end())
        pool = &plain->second;
    std::vector<std::string> names;
    if (pool)
        names = sex == "female" ? pool->female : pool->male;
    if (names.empty())
        names.assign(std::begin(Placeholders), std::end(Placeholders));
    std::set<std::string> fullNames, kinFirst;
    for (const auto& [id, life] : state_.residents)
        if (const auto* s = spec(id); s && alive(id))
            fullNames.insert(s->name);
    for (const auto& q : requests_)
        fullNames.insert(q.name);
    const auto& c = state_.careers;
    for (const auto& p : parents)
    {
        if (const auto* s = spec(p); s && alive(p))
            kinFirst.insert(firstName(s->name));
        for (const auto& [child, ps] : c.parents)
            if (std::find(ps.begin(), ps.end(), p) != ps.end() && alive(child))
                if (const auto* s = spec(child))
                    kinFirst.insert(firstName(s->name));
    }
    const auto full = [&](const std::string& first) { return first + (surname.empty() ? "" : " " + surname); };
    const auto free = [&](const std::string& first) { return !kinFirst.count(first) && !fullNames.count(full(first)); };
    // Named for a grandparent who died.
    if (!inPlaceOf.empty() && lotOf(key + "|named for") < (pool ? pool->namedFor : .25))
    {
        bool grandparent = false;
        for (const auto& p : parents)
            if (const auto ps = c.parents.find(p); ps != c.parents.end())
                grandparent = grandparent || std::find(ps->second.begin(), ps->second.end(), inPlaceOf) != ps->second.end();
        if (const auto* g = spec(inPlaceOf); grandparent && g && g->appearance.sex == sex && free(firstName(g->name)))
            return full(firstName(g->name));
    }
    const auto start = std::size_t(mixOf(key, today + 7) % names.size());
    for (std::size_t i = 0; i < names.size(); ++i)
        if (const auto& n = names[(start + i) % names.size()]; free(n))
            return full(n);
    return full(names[start]);
}

void Society::mantleWelcome(const ResidentRequest& request, const std::string& id)
{
    if (request.mantle.empty())
        return;
    const auto other = wearerOf(request.mantle);
    if (!other.empty() && other != id)
        return;                                     // (Taken since: they speak from the library.)
    auto b = request.voice;
    b.mantle = request.mantle;
    b.how = request.mantleHow;
    b.echoOf = request.echoOf;
    b.releasedDay = -1;
    b.knownBy.clear();
    b.born = request.kind == "birth";
    b.markDialect = request.redrawMarks ? request.markDialect : std::string();
    if (!b.born)
        b.from.clear(), b.accent.clear(), b.quirk.clear(), b.traits.clear();   // (An arrival keeps the voice it has.)
    state_.careers.mantles[id] = b;
    mantleChanges_.push_back({"bind", id, b.mantle, b.how, b.dialect, b.boundDay});
}

bool Society::bindMantle(const std::string& wearer, const std::string& mantle, const std::string& how, double day, std::string& why,
                         bool checkFits)
{
    if (wearer.empty() || mantle.empty())
    {
        why = "Name a resident and a mantle.";
        return false;
    }
    if (checkFits)
    {
        const auto* card = mantleCard(mantle);
        if (!card)
        {
            why = mantle + " is not a ready mantle.";
            return false;
        }
        if (const auto other = wearerOf(mantle); !other.empty() && other != wearer)
        {
            why = mantle + " is worn by " + other + ".";
            return false;
        }
        const auto* s = spec(wearer);
        if (!s)
        {
            why = wearer + " is not a resident.";
            return false;
        }
        const auto* post = jobOf(wearer);
        const std::map<std::string, std::string> facts{{"sex", s->appearance.sex}, {"role", s->role}, {"class", classOf(wearer)},
                                                       {"job", post ? post->title : s->workLabel}};
        if (!fitsMantle(card->fits, facts, s->age))
        {
            why = wearer + " doesn't fit " + mantle + " (fits " + card->fits + ").";
            return false;
        }
    }
    auto& b = state_.careers.mantles[wearer];
    if (b.mantle == mantle && b.releasedDay < 0)
    {
        if (!how.empty())
            b.how = how;
        return true;
    }
    MantleBinding fresh;
    fresh.mantle = mantle;
    fresh.how = how;
    fresh.boundDay = day;
    if (const auto* card = mantleCard(mantle))
        fresh.dialect = card->dialect;
    if (b.born)
    {
        // (A newborn keeps the voice it was born with; only the lines change.)
        fresh.born = true;
        fresh.from = b.from, fresh.accent = b.accent, fresh.quirk = b.quirk, fresh.traits = b.traits;
    }
    b = fresh;
    return true;
}

bool Society::releaseMantle(const std::string& wearer, double day, const std::vector<std::string>& knownBy)
{
    const auto found = state_.careers.mantles.find(wearer);
    if (found == state_.careers.mantles.end() || found->second.mantle.empty() || found->second.releasedDay >= 0)
        return false;
    found->second.releasedDay = day;
    found->second.knownBy = knownBy;
    return true;
}

void Society::setMantleMarks(const std::string& wearer, std::map<std::string, std::string> marks)
{
    if (const auto found = state_.careers.mantles.find(wearer); found != state_.careers.mantles.end())
        found->second.marks = std::move(marks);
}

std::vector<MantleChange> Society::takeMantleChanges()
{
    std::vector<MantleChange> out;
    out.swap(mantleChanges_);
    return out;
}

std::map<std::string, std::string> Society::wholeTraits(const std::map<std::string, std::string>& traits)
{
    std::map<std::string, std::string> out{{"temper", "even"},   {"talk", "plain"},     {"warmth", "reserved"}, {"humour", "none"},
                                           {"honesty", "plain"}, {"piety", "ordinary"}, {"drive", "steady"}};
    for (auto& [axis, value] : out)
        if (const auto t = traits.find(axis); t != traits.end() && !t->second.empty())
            value = t->second;
    return out;
}

std::string Society::cardPersonality(const std::map<std::string, std::string>& traits, const std::string& quirk)
{
    // The card in a few words (§12.5): "Calm and warm; swims in the lake even in autumn." No pronouns, nothing of sex.
    const auto t = wholeTraits(traits);
    std::vector<std::string> words;
    static const std::map<std::string, std::string> Temper{{"calm", "calm"}, {"nervy", "nervy"}, {"short", "short-tempered"}};
    static const std::map<std::string, std::string> Warmth{{"warm", "warm"}, {"cold", "cool with strangers"}};
    static const std::map<std::string, std::string> Talk{{"quiet", "quiet"}, {"chatty", "talkative"}};
    static const std::map<std::string, std::string> Humour{{"dry", "dry-humoured"}, {"merry", "merry"}};
    static const std::map<std::string, std::string> Drive{{"restless", "restless"}, {"ambitious", "ambitious"}, {"content", "content"}};
    static const std::map<std::string, std::string> Piety{{"devout", "devout"}};
    static const std::map<std::string, std::string> Honesty{{"frank", "frank"}, {"guarded", "guarded"}, {"sly", "sly"}};
    for (const auto& [axis, table] : std::vector<std::pair<std::string, const std::map<std::string, std::string>*>>{
             {"temper", &Temper}, {"warmth", &Warmth}, {"talk", &Talk}, {"humour", &Humour}, {"drive", &Drive},
             {"piety", &Piety}, {"honesty", &Honesty}})
        if (const auto w = table->find(t.at(axis)); w != table->end())
            words.push_back(w->second);
    std::string out;
    for (std::size_t i = 0; i < words.size(); ++i)
        out += (i == 0 ? "" : i + 1 == words.size() ? " and " : ", ") + words[i];
    if (out.empty())
        out = "even and plain";
    out[0] = char(std::toupper(static_cast<unsigned char>(out[0])));
    if (!quirk.empty())
    {
        auto q = quirk;
        std::replace(q.begin(), q.end(), '_', ' ');
        out += "; " + q;
    }
    return out + ".";
}

std::vector<MantlePoolHealth> Society::mantlePools(double now, const CareerWorld& world) const
{
    std::map<std::string, MantlePoolHealth> pools;
    const auto alive = [&](const std::string& id) { return !dead_.count(id) && (!world.alive || world.alive(id)); };
    std::map<std::string, std::pair<bool, double>> status;    // Mantle -> (worn, released)
    for (const auto& [who, b] : state_.careers.mantles)
    {
        if (b.mantle.empty())
            continue;
        auto& s = status.try_emplace(b.mantle, false, -1.0).first->second;
        if (b.releasedDay < 0 && alive(who))
            s.first = true;
        const double let = b.releasedDay >= 0 ? b.releasedDay : alive(who) ? -1 : b.boundDay;
        s.second = std::max(s.second, let);
    }
    for (const auto& [id, card] : mantleCards_)
    {
        auto& p = pools[card.dialect];
        p.dialect = card.dialect;
        const auto s = status.find(id);
        if (s != status.end() && s->second.first)
            continue;
        const auto honesty = card.traits.count("honesty") ? card.traits.at("honesty") : std::string("plain");
        ++p.honesty[honesty];
        if (s == status.end() || s->second.second < 0)
            ++p.spare;
        else if (now - s->second.second >= mantleRules_.restDays)
            ++p.ready;
        else
            ++p.resting;
    }
    // Births in the next 180 days: the deaths expected (births follow deaths one for one), and those owed now, shared
    // among the dialects as their mothers are (a fertile couple at home, realm-wide: Q5).
    const auto dialectOf = [&](const std::string& cell) { return world.dialectOfCell ? world.dialectOfCell(cell) : std::string(); };
    double deaths = 0;
    for (const auto& [id, life] : state_.residents)
    {
        if (!alive(id) || playerAccountId(id))
            continue;
        const int age = world.age ? world.age(id) : 30;
        const double yearly = std::min(1., DeathA * std::exp(DeathB * age) + (age < 5 ? InfantDeath : 0.));
        deaths += 1 - std::pow(1 - yearly, 180.0 / calendar::DaysPerYear);
        const auto d = dialectOf(life.homeCell);
        if (!d.empty())
        {
            pools[d].dialect = d;
            ++pools[d].livingHonesty[honestyOf(id)];
        }
    }
    deaths += double(state_.careers.birthsOwed.size());
    std::map<std::string, double> mothers;
    double all = 0;
    const auto& c = state_.careers;
    for (const auto& [a, b] : c.spouses)
    {
        if (!(a < b) || !alive(a) || !alive(b) || !household(a, b))
            continue;
        for (const auto& m : {a, b})
            if (const auto* s = spec(m); s && s->appearance.sex == "female" && world.age && world.age(m) >= 18 && world.age(m) <= 45)
                if (const auto* life = resident(m))
                {
                    const auto key = a + "|" + b;
                    if (c.births.count(key) && c.births.at(key) >= 3)
                        break;
                    const auto d = dialectOf(life->homeCell);
                    mothers[d] += 1, all += 1;
                    break;
                }
    }
    std::vector<MantlePoolHealth> out;
    for (auto& [d, p] : pools)
    {
        if (d.empty())
            continue;
        p.births180 = all > 0 ? deaths * (mothers.count(d) ? mothers[d] : 0) / all : 0;
        p.low = p.spare + p.ready < 1.5 * p.births180 + 2;
        out.push_back(p);
    }
    return out;
}
} // namespace ratw

namespace ratw
{
void Society::arrivalMantle(ResidentRequest& r, double now, const CareerWorld& world)
{
    // One of the Country's own (doc 84 §12.6) keeps their row's voice and takes a mantle from the pool of where they
    // grew up (their voice row's origin, else the town they come to), as they come.
    if (mantleCards_.empty())
        return;
    const auto* who = unplaced(r.templateId);
    if (!who)
        return;
    const auto origin = world.originOf ? world.originOf(r.templateId) : std::pair<std::string, std::string>{};
    r.dialect = !origin.first.empty() && world.dialectOfTown ? world.dialectOfTown(origin.first)
                : world.dialectOfCell                     ? world.dialectOfCell(r.home.cell)
                                                          : std::string();
    const std::map<std::string, std::string> facts{{"sex", who->appearance.sex},
                                                   {"role", r.role.empty() ? who->role : r.role},
                                                   {"job", r.workLabel.empty() ? who->workLabel : r.workLabel},
                                                   {"class", classOf(r.templateId)}};
    r.voice.boundDay = now;
    chooseMantle(r, r.templateId, facts, who->age, now, world);
}
} // namespace ratw
