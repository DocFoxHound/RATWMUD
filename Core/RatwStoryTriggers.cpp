// Native event/condition/action interpreter used by the world's story runner. Each call consumes one
// fact in one phase. The host commits the resulting changes together with the fact's durable cursor.
#include "RatwStoryRegion.h"
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <optional>
#include <set>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace ratw::story
{
using json::Value;

static bool includes(const Value& values, const std::string& s)
{
    return std::any_of(values.items().begin(), values.items().end(), [&](const Value& v) { return v == Value(s); });
}

static Value& field(Value& v, const std::string& key)
{
    if (!v.has(key))
        v.set(key, nullptr);
    return *v.find(key);
}

static bool compare(const Value& left, const std::string& op, const Value& right)
{
    if (op == "=") return left == right;
    if (op == "!=") return left != right;
    if (op == ">=") return left.asNumber() >= right.asNumber();
    if (op == "<=") return left.asNumber() <= right.asNumber();
    if (op == ">") return left.asNumber() > right.asNumber();
    if (op == "<") return left.asNumber() < right.asNumber();
    throw std::invalid_argument("Unknown story comparison");
}

struct Dispatch
{
    const Value& input;
    const Value& fact;
    const std::string actor;
    const bool participant;
    Value counters, fired, campaignFired;
    Value result = Value::object();

    explicit Dispatch(const Value& request)
        : input(request), fact(request["fact"]), actor(fact.string("actor")), participant(request.boolean("participant")),
          counters(request["counters"]), fired(request["fired"]), campaignFired(request["campaignFired"]) {}

    // A fact without a witnessed tile is indeterminate even under negation.
    std::optional<bool> condition(const Value& c) const
    {
        const auto test = c.string("test");
        if (test == "not")
        {
            const auto child = condition(c["condition"]);
            return child.has_value() ? std::optional<bool>(!*child) : std::nullopt;
        }
        if (test == "all" || test == "any")
        {
            const bool all = test == "all";
            bool unknown = false;
            for (const auto& child : c.array("conditions"))
            {
                const auto value = condition(child);
                if (!value.has_value()) unknown = true;
                else if (*value != all) return !all;
            }
            return unknown ? std::nullopt : std::optional<bool>(all);
        }
        if (test == "region")
        {
            const auto box = Region::read(input["regions"][c.string("region")]);
            if (!box || fact.string("cell").empty() || !Region::tile(fact["x"]) || !Region::tile(fact["y"]))
                return std::nullopt;
            return box->contains(fact.string("cell"), int(fact.number("x")), int(fact.number("y")));
        }
        if (test == "mission") return includes(input["ready"], c.string("id"));
        Value value;
        if (test == "counter")
        {
            const bool personal = c.string("scope") == "player";
            if (personal && !participant) return false;
            const auto& store = personal ? counters["player"][actor] : counters["story"];
            value = store.number(c.string("key"));
        }
        else if (test == "field")
        {
            const auto key = c.string("field");
            value = c["value"].isNumber() ? Value(fact.number(key)) : Value(fact.string(key));
        }
        else throw std::invalid_argument("Unknown story condition");
        return compare(value, c.string("op"), c["value"]);
    }

    bool matches(const Value& rule) const
    {
        const auto on = rule.string("on");
        if (on != fact.string("kind") && !(on == "gift" && fact.string("kind") == "gift again"))
            return false;
        if (rule["signal"] != fact["signal"]) return false;
        for (const auto& [key, expected] : rule["match"].fields())
        {
            auto actual = fact.string(key);
            if (key == "item" && (on == "gift" || on == "gift again") && expected.asString().find_first_of("~@") == std::string::npos)
                actual = actual.substr(0, actual.find_first_of("~@"));
            if (actual != expected.asString()) return false;
        }
        return on != "clock" || input.number("day") >= input.number("entered") + rule.number("afterDays");
    }

    bool eligible(const Value& rule, bool shared)
    {
        // Turned off by an admin or (Phase 5) by a trigger: a campaign trigger by its id, a phase's as phase/id.
        if (includes(input["disabled"], shared ? rule.string("id") : input.string("phase") + "/" + rule.string("id")))
            return false;
        const auto repeat = rule.string("once", "each");
        const bool personal = repeat == "player" || repeat == "member";
        if (!matches(rule) || ((rule.string("audience") == "participant" || personal) && !participant))
            return false;
        const bool global = repeat == "campaign" || repeat == "member";
        auto& marks = global ? campaignFired : fired;
        const auto key = global && !shared ? input.string("phase") + "/" + rule.string("id") : rule.string("id");
        const auto owner = personal ? actor : "*";
        if (repeat != "each" && includes(marks[key], owner)) return false;
        if (rule.has("if") && !condition(rule["if"]).value_or(false)) return false;
        if (repeat != "each") field(marks, key).push(owner);
        return true;
    }

    void action(const Value& a)
    {
        const auto op = a.string("do");
        if (op == "enter") result.set("phase", a["phase"]);
        else if (op == "effect")
        {
            Value effect = a["effect"];
            if (effect.string("player") == "$actor") effect.set("player", actor);
            field(result, "effects").push(effect);
        }
        else if (op == "emit") field(result, "signals").push(a["signal"]);
        else if (op == "schedule" || op == "cancel")
        {
            const auto name = a.string("signal");
            const auto key = a.string("owner") == "player" ? name + "/" + actor : name;
            Value timer;
            if (op == "schedule")
            {
                const auto clock = a.string("clock", "game");
                timer.set("signal", name);
                timer.set("at", input.number(clock == "real" ? "elapsed" : "day") + a.number("days"));
                timer.set("clock", clock);
                timer.set("actor", actor);
                timer.set("interval", a.boolean("repeat") ? a.number("days") : 0);
                timer.set("scope", a.string("scope", "visit"));
            }
            field(result, "timers").set(key, timer);
        }
        else if (op == "set" || op == "add")
        {
            const bool personal = a.string("scope") == "player";
            if (personal && !participant) return;
            auto& byScope = field(counters, a.string("scope"));
            auto& store = personal ? field(byScope, actor) : byScope;
            const auto key = a.string("key");
            double n = a["value"].isString() ? fact.number(a.string("value")) : a.number("value");
            if (op == "add") n += store.number(key);
            store.set(key, std::clamp(n, -1e9, 1e9));
        }
        else throw std::invalid_argument("Unknown story action");
    }

    Value run()
    {
        for (const auto& rule : input.array("rules"))
        {
            if (!eligible(rule, includes(input["shared"], rule.string("id")))) continue;
            field(result, "firedRules").push(rule["id"]);
            for (const auto& a : rule.array("actions")) action(a);
            if (result.has("phase")) break;
        }
        if (counters != input["counters"]) result.set("counters", counters);
        if (fired != input["fired"]) result.set("fired", fired);
        if (campaignFired != input["campaignFired"]) result.set("campaignFired", campaignFired);
        return result;
    }
};

// ------------------------------------------------------------------------------------------------------------- version 2
// Docs/Design/85-story-triggers-and-campaign-control.md, 3.2-3.5 and Phase 5: declared variables, conditions over the
// story's own state, and flow inside a trigger's actions. The twin of tools/story_triggers.py (its `_act`, `test_v2`,
// `_resume`): the request carries the run's state pieces (`state`) and they come back changed, as the Python twin leaves
// them, key for key (tools/test_story_native.py runs every rehearsal through both).

static const double Limit = 1e9;
static const std::size_t ListMost = 500, EachMost = 500, TextMost = 200;
static const int RunDepth = 8;

static Value& obj(Value& parent, const std::string& key)
{
    if (!parent.has(key)) parent.set(key, Value::object());
    return *parent.find(key);
}

static Value& list(Value& parent, const std::string& key)
{
    if (!parent.has(key)) parent.set(key, Value::array());
    return *parent.find(key);
}

static std::uint64_t fnv(const std::string& text)
{
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (unsigned char b : text) { h ^= b; h *= 0x100000001b3ULL; }
    return h;
}

static std::string prefix(const std::string& text, std::size_t chars)
{
    std::size_t n = 0;
    for (std::size_t at = 0; at < text.size(); ++at)
        if ((static_cast<unsigned char>(text[at]) & 0xC0) != 0x80 && n++ == chars) return text.substr(0, at);
    return text;
}

static bool contains(const Value& values, const Value& v)
{
    return std::any_of(values.items().begin(), values.items().end(), [&](const Value& x) { return x == v; });
}

static bool lists(const std::string& type) { return type == "numbers" || type == "texts" || type == "wolves" || type == "residents"; }

static Value initialOf(const Value& decl)
{
    if (decl.has("initial")) return decl["initial"];
    const auto type = decl.string("type");
    if (lists(type)) return Value::array();
    if (type == "number") return Value(0);
    if (type == "switch") return Value(false);
    if (type == "timer") return Value(nullptr);
    return Value("");
}

enum class Flow { Go, Stop, Wait, Enter, Finish };

struct Dispatch2
{
    const Value& input;
    Value state;                      // counters, fired, campaignFired, counts, timers, disabled, waiting, draws, rolls, outcome, outcomes, parties
    const Value& decls;
    const Value& cohort;
    const std::string phaseId;
    const double day, elapsed;
    Value result = Value::object();
    Value firings = Value::array();
    std::string target;

    explicit Dispatch2(const Value& request)
        : input(request), state(request["state"]), decls(request["decls"]), cohort(request["cohort"]), phaseId(request.string("phase")),
          day(request.number("day")), elapsed(request.number("elapsed")) {}

    struct Ctx
    {
        const Value* rule;
        const Value* fact;
        std::string actor;
        std::optional<Value> each;
        std::size_t firing;           // Index in `firings` that notes go to.
        int depth = 0;
    };

    // (Doc 86, R1: a test for each of 5,000 wolves asks this for each one; the cohort is looked up in a set, made once.)
    mutable std::optional<std::unordered_set<std::string>> cohortSet;
    bool member(const std::string& wolf) const
    {
        if (!cohortSet)
        {
            cohortSet.emplace();
            for (const auto& w : cohort.items())
                if (w.isString()) cohortSet->insert(w.asString());
        }
        return cohortSet->count(wolf) > 0;
    }
    bool shared(const std::string& id) const { return includes(input["shared"], id); }
    std::string keyOf(const std::string& id) const { return shared(id) ? id : phaseId + "/" + id; }
    std::string partyOf(const std::string& wolf) const { return state["parties"].has(wolf) ? state["parties"].string(wolf) : wolf; }
    double now(const std::string& clock) const { return clock == "real" ? elapsed : day; }
    std::string wolfOf(const Ctx& x, const std::string& of) const { return of == "$each" ? (x.each ? x.each->asString() : "") : x.actor; }

    long long draw(long long n)
    {
        const auto k = static_cast<long long>(state.number("draws"));
        state.set("draws", Value(k + 1));
        if (state.has("rolls") && state["rolls"].size())
        {
            auto& rolls = state.find("rolls")->items();
            const auto r = static_cast<long long>(rolls.front().asNumber());
            rolls.erase(rolls.begin());
            if (rolls.empty()) state.erase("rolls");
            return r * n / 100;
        }
        return static_cast<long long>(fnv(state.string("seed") + ":" + std::to_string(k)) % static_cast<std::uint64_t>(n));
    }

    // The store a variable lives in; nullptr for a wolf not in the story. `write` creates it as Python's setdefault does.
    const Value* store(const std::string& scope, const std::string& wolf) const
    {
        static const Value Empty = Value::object();
        const bool personal = scope == "player" || scope == "party";
        if (personal && !member(wolf)) return nullptr;
        const auto& byScope = state["counters"][scope];
        if (!personal) return byScope.isObject() ? &byScope : &Empty;
        const auto* own = byScope.find(scope == "player" ? wolf : partyOf(wolf));
        return own ? own : &Empty;
    }

    Value* writable(const std::string& scope, const std::string& wolf)
    {
        const bool personal = scope == "player" || scope == "party";
        if (personal && !member(wolf)) return nullptr;
        const auto owner = scope == "player" ? wolf : partyOf(wolf);
        auto& byScope = obj(obj(state, "counters"), scope);
        return personal ? &obj(byScope, owner) : &byScope;
    }

    std::optional<Value> field(const Value& fact, const std::string& name) const
    {
        const auto& v = fact[name];
        if (v.isNull()) return name == "quantity" || name == "coins" ? Value(0) : Value("");
        return v;
    }

    std::optional<Value> value(const Value& o, const Ctx& x) const
    {
        if (!o.isObject()) return o;
        const std::string name = o.has("var") ? o.string("var") : o.has("count") ? o.string("count") : o.string("since");
        if (o.has("var") || o.has("count") || o.has("since"))
        {
            const auto& d = decls[name];
            const auto* s = store(d.string("scope"), wolfOf(x, o.string("of")));
            if (!s) return std::nullopt;
            const Value v = s->has(name) ? (*s)[name] : initialOf(d);
            if (o.has("count")) return Value(double(v.size()));
            if (o.has("since")) return v.isNull() ? std::nullopt : std::optional<Value>(Value(now(d.string("clock", "game")) - v.asNumber()));
            return v;
        }
        if (o.has("field")) return field(*x.fact, o.string("field"));
        if (o.has("each")) return x.each;
        if (o.has("audience"))
        {
            // (Doc 86, C2: the counts the host sends, as story_triggers.audience_of makes them; online may be unknown.)
            const auto& n = input["audience"][o.string("audience")];
            if (!n.isNumber()) return std::nullopt;
            return Value(n.asNumber());
        }
        for (const char* share : {"percent", "among"})
        {
            if (!o.has(share)) continue;
            std::vector<std::string> s, d;
            if (!counted(o[share], o.string("of", "active"), x, s, d)) return std::nullopt;
            if (std::string(share) == "among") return Value(double(s.size()));
            return Value(d.empty() ? 0.0 : std::floor(100.0 * double(s.size()) / double(d.size())));
        }
        for (const char* key : {"plus", "minus", "min", "max", "times", "div"})
        {
            if (!o.has(key)) continue;
            const auto a = value(o.array(key)[0], x), b = value(o.array(key)[1], x);
            if (!a || !b) return std::nullopt;
            const double l = a->asNumber(), r = b->asNumber();
            const std::string k = key;
            if (k == "div") return r == 0 ? std::nullopt : std::optional<Value>(Value(std::trunc(l / r)));
            if (k == "times") return Value(l * r);
            // (Python's min and max keep the first of two equal values; a number either way.)
            return Value(k == "plus" ? l + r : k == "minus" ? l - r : k == "min" ? (r < l ? r : l) : (r > l ? r : l));
        }
        throw std::invalid_argument("Unknown story value");
    }

    // Doc 86, R1: the wolves counted against (`d`: the active ones, all, or those online) and of those the ones who count
    // (`s`: in a story or world list, or meeting a test with the wolf as $each). False when `d` is unknown (online, not
    // sent). The twin of story_triggers.counted; the test reads no facts and rolls nothing (story_program refuses it).
    bool counted(const Value& src, const std::string& of, const Ctx& x, std::vector<std::string>& s, std::vector<std::string>& d) const
    {
        const auto& aud = input["audience"];
        const Value& ids = of == "all" ? cohort : of == "online" ? aud["onlineIds"] : aud["activeIds"];
        if (!ids.isArray()) return false;
        for (const auto& w : ids.items())
            if (w.isString()) d.push_back(w.asString());
        if (src.has("list"))
        {
            const auto name = src.string("list");
            const auto& decl = decls[name];
            const auto* st = store(decl.string("scope"), "");
            const Value members = st && st->has(name) ? (*st)[name] : initialOf(decl);
            std::unordered_set<std::string> in;
            for (const auto& m : members.items())
                if (m.isString()) in.insert(m.asString());
            for (const auto& w : d)
                if (in.count(w)) s.push_back(w);
            return true;
        }
        auto* self = const_cast<Dispatch2*>(this);
        for (const auto& w : d)
        {
            Ctx y = x;
            y.each = Value(w);
            const auto v = self->test(src["where"], y);
            if (v && *v) s.push_back(w);
        }
        return true;
    }

    static long long shareNeed(long long d, long long percent, long long floor, std::optional<long long> cap)
    {
        long long need = std::max(std::min(floor, d), (percent * d + 99) / 100);
        if (cap) need = std::min(need, *cap);
        return std::max(need, 1LL);
    }

    static bool cmp(const Value& l, const std::string& op, const Value& r)
    {
        if (op == "=") return l == r;
        if (op == "!=") return l != r;
        return compare(l, op, r);
    }

    Value var(const std::string& name, const std::string& of) const
    {
        Value o = Value::object();
        o.set("var", name);
        if (!of.empty()) o.set("of", of);
        return o;
    }

    // A version 1 condition inside a version 2 story (region, mission, field), as the first Dispatch reads it.
    std::optional<bool> classic(const Value& c, const Ctx& x) const
    {
        const auto test = c.string("test");
        if (test == "region")
        {
            const auto box = Region::read(input["regions"][c.string("region")]);
            const auto& fact = *x.fact;
            if (!box || fact.string("cell").empty() || !Region::tile(fact["x"]) || !Region::tile(fact["y"]))
                return std::nullopt;
            return box->contains(fact.string("cell"), int(fact.number("x")), int(fact.number("y")));
        }
        if (test == "mission") return includes(input["ready"], c.string("id"));
        if (test == "field")
        {
            const auto key = c.string("field");
            const Value v = c["value"].isNumber() ? Value(x.fact->number(key)) : Value(x.fact->string(key));
            return compare(v, c.string("op"), c["value"]);
        }
        throw std::invalid_argument("Unknown story condition");
    }

    std::optional<bool> test(const Value& c, const Ctx& x)
    {
        const auto kind = c.string("test");
        if (kind == "all" || kind == "any")
        {
            const bool decisive = kind == "any";
            bool unknown = false;
            for (const auto& child : c.array("conditions"))
            {
                const auto v = test(child, x);
                if (v.has_value() && *v == decisive) return decisive;
                unknown = unknown || !v.has_value();
            }
            return unknown ? std::nullopt : std::optional<bool>(!decisive);
        }
        if (kind == "not")
        {
            const auto v = test(c["condition"], x);
            return v ? std::optional<bool>(!*v) : std::nullopt;
        }
        if (kind == "compare")
        {
            const auto l = value(c["left"], x), r = value(c["right"], x);
            return l && r && cmp(*l, c.string("op"), *r);
        }
        if (kind == "switch")
        {
            const auto v = value(var(c.string("var"), c.string("of")), x);
            return v && v->isBool() && v->asBool() == c.boolean("is");
        }
        if (kind == "in list")
        {
            const auto values = value(var(c.string("list"), c.string("of")), x);
            const auto v = value(c["value"], x);
            return values && v && contains(*values, *v);
        }
        if (kind == "phase is") return phaseId == c.string("phase");
        if (kind == "visit is") return compare(Value(input.number("visits")), c.string("op"), c["value"]);
        if (kind == "timer")
        {
            const auto key = c.string("owner") == "player" ? c.string("signal") + "/" + x.actor : c.string("signal");
            const auto* timer = state["timers"].find(key);
            if (!timer || timer->isNull()) return false;
            if (!c.has("op")) return true;
            return compare(Value(timer->number("at") - now(timer->string("clock", "game"))), c.string("op"), c["days"]);
        }
        if (kind == "fired")
        {
            const auto& bucket = state["counts"][c.string("in", "visit")];
            return compare(Value(bucket.number(keyOf(c.string("trigger")))), c.string("op"), c["value"]);
        }
        if (kind == "chance") return draw(100) < c.number("percent");
        if (kind == "participation")
        {
            std::vector<std::string> s, d;
            if (!counted(c, c.string("of", "active"), x, s, d)) return std::nullopt;
            if (d.empty()) return false;
            const auto cap = c.has("cap") ? std::optional<long long>(static_cast<long long>(c.number("cap"))) : std::nullopt;
            const auto need = shareNeed(static_cast<long long>(d.size()), static_cast<long long>(c.number("percent")),
                                        static_cast<long long>(c.number("floor", 1)), cap);
            return static_cast<long long>(s.size()) >= need;
        }
        if (kind == "days since")
        {
            std::optional<Value> since;
            const auto from = c.string("from");
            if (from == "var")
            {
                Value o = Value::object();
                o.set("since", c.string("var"));
                if (c.has("of")) o.set("of", c["of"]);
                since = value(o, x);
            }
            else since = Value(day - (from == "phase" ? input.number("entered") : input.number("began", input.number("entered"))));
            return since && compare(*since, c.string("op"), c["days"]);
        }
        if (kind == "quest is" || kind == "objective is") return quest(c, x);
        if (factKinds().count(kind)) return fact(c, x);
        return classic(c, x);
    }

    // ---- Phase 8: the world's facts as of the event (`facts` on the fact), and the story's quests. The twin of
    // story_triggers.fact_test and quest_test: a missing fact is unknown.
    static const std::set<std::string>& factKinds()
    {
        static const std::set<std::string> kinds{"wolf here", "wolf in region", "wolf in place", "wolf in town", "wolf indoors",
            "count in region", "carries", "coins", "standing", "reputation", "wolf is", "party size", "in party with", "resident is",
            "resident in place", "resident in region", "resident has", "hour between", "calendar is", "weather is", "relation is"};
        return kinds;
    }

    static bool whole(const Value& v) { return v.isNumber() && std::floor(v.asNumber()) == v.asNumber(); }

    bool inside(const std::string& region, const Value& at) const
    {
        const auto& r = input["regions"][region];
        const auto& x = at["x"];
        const auto& y = at["y"];
        if (!whole(x) || !whole(y) || !at["cell"].isString()) return false;
        return at.string("cell") == r.string("cell") && r.number("x1") <= x.asNumber() && x.asNumber() <= r.number("x2") &&
               r.number("y1") <= y.asNumber() && y.asNumber() <= r.number("y2");
    }

    std::optional<bool> number(const Value* v, const Value& c) const
    {
        if (!v || !v->isNumber()) return std::nullopt;
        return compare(*v, c.string("op"), c["value"]);
    }

    std::string who(const Ctx& x, const std::string& v) const
    {
        if (v == "$each") return x.each && x.each->isString() ? x.each->asString() : "";
        return v == "$actor" ? x.actor : v;
    }

    std::optional<bool> fact(const Value& c, const Ctx& x) const
    {
        const auto kind = c.string("test");
        const auto* f = x.fact->find("facts");
        if (!f || !f->isObject()) return std::nullopt;
        static const Value Empty = Value::object();
        const auto section = [&](const char* name) -> const Value& { const auto& v = (*f)[name]; return v.isObject() ? v : Empty; };
        const auto& wolves = section("wolves");
        const auto& world = section("world");
        if (kind == "hour between")
        {
            const auto& h = world["hour"];
            if (!h.isNumber()) return std::nullopt;
            const double a = c.number("from"), b = c.number("to"), n = h.asNumber();
            return a <= b ? (a <= n && n <= b) : (n >= a || n <= b);
        }
        if (kind == "calendar is")
        {
            const auto& v = world[c.string("what")];
            if (!v.isString()) return std::nullopt;
            return v.asString() == c.string("value");
        }
        if (kind == "relation is")
        {
            static const std::vector<std::string> stances{"war", "hostile", "tense", "neutral", "friendly", "allied"};
            const auto& v = section("relations")[c.string("faction") + ">" + c.string("other")];
            const auto at = v.isString() ? std::find(stances.begin(), stances.end(), v.asString()) : stances.end();
            if (at == stances.end()) return std::nullopt;
            const auto want = std::find(stances.begin(), stances.end(), c.string("stance"));
            return compare(Value(double(at - stances.begin())), c.string("op"), Value(double(want - stances.begin())));
        }
        if (kind == "count in region")
        {
            if (!input["regions"].has(c.string("region"))) return std::nullopt;
            double n = 0;
            for (const auto& [id, w] : wolves.fields())
                if (member(id) && w.isObject() && inside(c.string("region"), w)) ++n;
            return compare(Value(n), c.string("op"), c["value"]);
        }
        if (kind.rfind("resident", 0) == 0)
        {
            const auto& r = section("residents")[c.string("npc")];
            if (!r.isObject()) return std::nullopt;
            const bool alive = r["alive"].isBool() && r["alive"].asBool();
            const bool indoors = r["indoors"].isBool() && r["indoors"].asBool();
            if (kind == "resident is")
            {
                const auto state = c.string("state");
                return state == "alive" ? alive : state == "dead" ? !alive : alive && indoors == (state == "indoors");
            }
            if (kind == "resident in place") return r["cell"].isString() && r.string("cell") == c.string("place");
            if (kind == "resident in region")
            {
                if (!input["regions"].has(c.string("region"))) return std::nullopt;
                return inside(c.string("region"), r);
            }
            return r[c.string("what")].asString("") == c.string("value");
        }
        if (kind == "weather is")
        {
            std::string where = c.string("where", "$actor");
            if (where == "$actor")
            {
                const auto& w = wolves[x.actor];
                where = w.isObject() && w["cell"].isString() ? w.string("cell") : std::string();
                if (where.empty()) return std::nullopt;
            }
            const auto& v = section("weather")[where];
            if (!v.isString()) return std::nullopt;
            return v.asString() == c.string("weather");
        }
        const auto& w = wolves[who(x, c.string("who", "$actor"))];
        if (kind == "wolf here") return w.isObject();
        if (!w.isObject()) return std::nullopt;
        const auto flag = [&](const std::string& key) { return w[key].isBool() && w[key].asBool(); };
        if (kind == "wolf in region")
        {
            if (!input["regions"].has(c.string("region"))) return std::nullopt;
            return inside(c.string("region"), w);
        }
        if (kind == "wolf in place") return w["cell"].isString() && w.string("cell") == c.string("place");
        if (kind == "wolf in town") return w["town"].isString() && w.string("town") == c.string("town");
        if (kind == "wolf indoors") return flag("indoors") == c.boolean("is");
        if (kind == "wolf is") return flag(c.string("state")) == c.boolean("is");
        if (kind == "in party with")
        {
            const auto& o = wolves[who(x, c.string("other"))];
            const auto& party = w["party"];
            return party.isString() && !party.asString().empty() && o.isObject() && o["party"] == party;
        }
        if (kind == "party size")
        {
            static const Value One(1);
            const auto* n = w.find("partySize");
            return number(n ? n : &One, c);
        }
        if (kind == "carries")
        {
            static const Value Zero(0);
            const auto& held = w["carries"];
            const auto* n = held.isObject() ? held.find(c.string("item")) : nullptr;
            return number(n ? n : &Zero, c);
        }
        if (kind == "coins") return number(w.find("coins"), c);
        if (kind == "standing")
        {
            const auto& st = w["standing"];
            return number(st.isObject() ? st.find(c.string("faction")) : nullptr, c);
        }
        if (kind == "reputation")
        {
            static const Value Zero(0);
            const auto& rep = w["reputation"];
            const auto* n = rep.isObject() ? rep.find(c.string("town")) : nullptr;
            return number(n ? n : &Zero, c);
        }
        throw std::invalid_argument("Unknown story condition");
    }

    // The copy of a quest a wolf takes part in (story_quests.copy_of): their own, the story's, or their party's.
    const Value* questCopy(const Value& def, const std::string& quest, const std::string& wolf) const
    {
        const auto& copies = input["quests"][quest];
        const auto scope = def.string("scope", "player");
        if (scope == "player") { const auto* c = copies.find(wolf); return c && c->isObject() ? c : nullptr; }
        if (scope == "story") { const auto* c = copies.find("story"); return c && c->isObject() && member(wolf) ? c : nullptr; }
        const Value* best = nullptr;
        for (const auto& [holder, c] : copies.fields())
        {
            if (!includes(c["members"], wolf)) continue;
            const auto status = c.string("status");
            if ((status == "active" || status == "ready") && !includes(c["left"], wolf)) return &c;
            if (!best || c.number("began") > best->number("began")) best = &c;
        }
        return best;
    }

    std::optional<bool> quest(const Value& c, const Ctx& x) const
    {
        const auto id = c.string("quest");
        const auto& def = input["questDefs"][id];
        if (!def.isObject()) return std::nullopt;
        const auto of = c.string("of", "$actor");
        const Value* copy = nullptr;
        std::string wolf;
        if (of == "story")
        {
            const auto* s = input["quests"][id].find("story");
            copy = s && s->isObject() ? s : nullptr;
        }
        else
        {
            wolf = who(x, of);
            copy = questCopy(def, id, wolf);
        }
        if (c.string("test") == "quest is")
        {
            const auto status = copy ? copy->string("status") : of != "story" && includes(input["questOffers"][id], wolf) ? "offered" : "none";
            return status == c.string("status");
        }
        const auto oid = c.string("objective");
        const auto& rec = copy ? (*copy)["objectives"][oid] : Value();
        bool hidden = false;
        for (const auto& o : def.array("objectives"))
            if (o.string("id") == oid) hidden = o.boolean("hidden");
        const auto status = rec.boolean("done") ? "done" : rec.boolean("failed") ? "failed" : hidden && !rec.boolean("shown") ? "hidden" : "to do";
        return status == c.string("status");
    }

    const Value* ruleById(const std::string& id) const
    {
        for (const auto& r : input.array("rules"))
            if (r.string("id") == id) return &r;
        return nullptr;
    }

    std::size_t addFiring(const Value& rule, const std::string& via)
    {
        Value f = Value::object();
        f.set("trigger", rule["id"]);
        if (!via.empty()) f.set("via", via);
        firings.push(f);
        return firings.size() - 1;
    }

    void count(const std::string& id)
    {
        const auto key = keyOf(id);
        for (const char* bucket : {"visit", "run"})
        {
            auto& b = obj(obj(state, "counts"), bucket);
            b.set(key, Value(b.number(key) + 1));
        }
    }

    static const Value& branch(const Value& a, const std::string& name)
    {
        if (name == "then" || name == "else" || name == "actions") return a[name];
        const auto colon = name.find(':');
        const auto k = std::stoul(name.substr(colon + 1));
        return name.compare(0, colon, "elif") == 0 ? a.array("elif")[k]["then"] : a.array("choices")[k]["actions"];
    }

    template <class F> void change(const Value& a, const Ctx& x, F fn)
    {
        const auto name = a.string("var");
        const auto& d = decls[name];
        auto* s = writable(d.string("scope"), wolfOf(x, a.string("of")));
        if (!s) return;
        auto next = fn(s->has(name) ? (*s)[name] : initialOf(d));
        if (next) s->set(name, *next);
    }

    static Value clampNumber(double n) { return Value(std::clamp(n, -Limit, Limit)); }

    Flow run(const Value& actions, Ctx& x, Value base, std::size_t start = 0)
    {
        const auto& items = actions.items();
        for (std::size_t i = start; i < items.size(); ++i)
        {
            Value address = base;
            address.push(Value(double(i)));
            const auto r = act(items[i], x, address);
            if (r != Flow::Go) return r;
        }
        return Flow::Go;
    }

    Flow resume(const Value& actions, Ctx& x, Value base, const Value& address, std::size_t from = 0)
    {
        auto i = std::size_t(address.items()[from].asNumber());
        if (address.size() > from + 1)
        {
            const auto name = address.items()[from + 1].asString();
            Value inner = base;
            inner.push(Value(double(i)));
            inner.push(Value(name));
            const auto r = resume(branch(actions.items()[i], name), x, inner, address, from + 2);
            if (r != Flow::Go) return r;
            ++i;
        }
        return run(actions, x, base, i);
    }

    Flow act(const Value& a, Ctx& x, const Value& address)
    {
        const auto op = a.string("do");
        if (op == "enter") { target = a.string("phase"); return Flow::Enter; }
        if (op == "effect")
        {
            Value effect = Value::object();
            for (const auto& [k, v] : a["effect"].fields())
            {
                if (k == "player" && v == Value("$actor")) effect.set(k, x.actor);
                else if (v == Value("$each") && x.each) effect.set(k, *x.each);
                else effect.set(k, v);
            }
            list(result, "effects").push(effect);
        }
        else if (op == "schedule" || op == "cancel")
        {
            const auto name = a.string("signal");
            const auto key = a.string("owner") == "player" ? name + "/" + x.actor : name;
            if (op == "cancel")
            {
                if (state.has("timers")) state.find("timers")->erase(key);
                return Flow::Go;
            }
            const auto clock = a.string("clock", "game");
            Value timer = Value::object();
            timer.set("signal", name);
            timer.set("at", now(clock) + a.number("days"));
            timer.set("clock", clock);
            timer.set("actor", x.actor);
            timer.set("interval", a.boolean("repeat") ? a.number("days") : 0);
            timer.set("scope", a.string("scope", "visit"));
            obj(state, "timers").set(key, timer);
        }
        else if (op == "emit") list(result, "signals").push(a["signal"]);
        else if (op == "if")
        {
            if (test(a["if"], x).value_or(false)) return run(a["then"], x, with(address, "then"));
            const auto& elifs = a.array("elif");
            for (std::size_t k = 0; k < elifs.size(); ++k)
                if (test(elifs[k]["if"], x).value_or(false)) return run(elifs[k]["then"], x, with(address, "elif:" + std::to_string(k)));
            if (a.has("else")) return run(a["else"], x, with(address, "else"));
        }
        else if (op == "each")
        {
            const auto before = x.each;
            for (const auto& wolf : loop(a, x))
            {
                x.each = wolf;
                const auto r = run(a["actions"], x, with(address, "actions"));
                if (r != Flow::Go) { x.each = before; return r; }
            }
            x.each = before;
        }
        else if (op == "random")
        {
            long long total = 0;
            for (const auto& c : a.array("choices")) total += static_cast<long long>(c.number("weight"));
            auto roll = draw(total);
            const auto& choices = a.array("choices");
            for (std::size_t k = 0; k < choices.size(); ++k)
            {
                const auto w = static_cast<long long>(choices[k].number("weight"));
                if (roll < w) return run(choices[k]["actions"], x, with(address, "choice:" + std::to_string(k)));
                roll -= w;
            }
        }
        else if (op == "wait")
        {
            const auto clock = a.string("clock", "game");
            Value next = Value::array();
            const auto& parts = address.items();
            for (std::size_t i = 0; i + 1 < parts.size(); ++i) next.push(parts[i]);
            next.push(Value(parts.back().asNumber() + 1));
            Value w = Value::object();
            w.set("trigger", (*x.rule)["id"]);
            w.set("scope", shared(x.rule->string("id")) ? "campaign" : "visit");
            w.set("phase", phaseId);
            w.set("at", now(clock) + a.number("days"));
            w.set("clock", clock);
            w.set("actor", x.actor);
            Value kept = *x.fact;
            kept.erase("facts");      // (Read again when it resumes: doc 85, Phase 8.)
            w.set("event", kept);
            w.set("next", next);
            list(state, "waiting").push(w);
            return Flow::Wait;
        }
        else if (op == "run")
        {
            const auto* other = ruleById(a.string("trigger"));
            if (!other || x.depth >= RunDepth) return Flow::Go;
            if (a.boolean("check"))
            {
                if (includes(state["disabled"], keyOf(other->string("id")))) return Flow::Go;
                if (other->has("if") && !test((*other)["if"], x).value_or(false)) return Flow::Go;
            }
            Ctx inner{other, x.fact, x.actor, std::nullopt, addFiring(*other, "run"), x.depth + 1};
            count(other->string("id"));
            const auto r = run((*other)["actions"], inner, Value::array());
            if (r == Flow::Enter || r == Flow::Finish) return r;
        }
        else if (op == "enable" || op == "disable")
        {
            const auto key = a.has("phase") ? a.string("phase") + "/" + a.string("trigger") : keyOf(a.string("trigger"));
            std::vector<std::string> off;
            for (const auto& k : state.array("disabled")) if (k.asString() != key) off.push_back(k.asString());
            if (op == "disable") off.push_back(key);
            std::sort(off.begin(), off.end());
            off.erase(std::unique(off.begin(), off.end()), off.end());
            if (off.empty()) state.erase("disabled");
            else
            {
                Value v = Value::array();
                for (const auto& k : off) v.push(Value(k));
                state.set("disabled", v);
            }
        }
        else if (op == "stop") return Flow::Stop;
        else if (op == "note")
        {
            auto& f = firings.items()[x.firing];
            list(f, "notes").push(a["text"]);
        }
        else if (op == "finish")
        {
            const auto who = a.string("who", "everyone");
            Value record = Value::object();
            record.set("outcome", a["outcome"]);
            record.set("text", a["text"]);
            record.set("day", day);
            if (who == "everyone") { state.set("outcome", record); return Flow::Finish; }
            const auto wolf = wolfOf(x, who);
            if (member(wolf)) obj(state, "outcomes").set(wolf, record);
        }
        else if (op == "set" || op == "add" || op == "subtract" || op == "multiply")
        {
            const auto v = value(a["value"], x);
            if (!v) return Flow::Go;
            const bool number = decls[a.string("var")].string("type") == "number";
            change(a, x, [&](const Value& cur) -> std::optional<Value> {
                if (op == "set") return number ? clampNumber(v->asNumber()) : *v;
                const double l = cur.asNumber(), r = v->asNumber();
                return clampNumber(op == "add" ? l + r : op == "subtract" ? l - r : l * r);
            });
        }
        else if (op == "clamp")
            change(a, x, [&](const Value& cur) -> std::optional<Value> {
                return Value(std::max(a.number("min"), std::min(a.number("max"), cur.asNumber())));
            });
        else if (op == "turn on" || op == "turn off" || op == "toggle")
            change(a, x, [&](const Value& cur) -> std::optional<Value> {
                return Value(op == "turn on" ? true : op == "turn off" ? false : !cur.asBool());
            });
        else if (op == "remember")
        {
            std::optional<Value> v;
            if (a.string("from") == "each") v = x.each;
            else v = field(*x.fact, a.string("from"));
            change(a, x, [&](const Value&) -> std::optional<Value> {
                if (!v) return std::nullopt;
                return Value(prefix(v->isString() ? v->asString() : json::dump(*v), TextMost));
            });
        }
        else if (op == "list add" || op == "list remove" || op == "list clear")
        {
            std::optional<Value> v;
            if (op != "list clear")
            {
                v = value(a["value"], x);
                if (!v) return Flow::Go;
            }
            change(a, x, [&](const Value& cur) -> std::optional<Value> {
                Value values = Value::array();
                if (op == "list clear") return values;
                bool removed = false;
                for (const auto& item : cur.items())
                {
                    if (op == "list remove" && !removed && item == *v) { removed = true; continue; }
                    values.push(item);
                }
                if (op == "list add" && !contains(values, *v) && values.size() < ListMost) values.push(*v);
                return values;
            });
        }
        else if (op == "pick")
        {
            const auto values = value(var(a.string("list"), a.string("of")), x);
            if (!values || !values->size()) return Flow::Go;
            const Value chosen = values->items()[std::size_t(draw((long long)values->size()))];
            change(a, x, [&](const Value&) -> std::optional<Value> { return chosen; });
        }
        else if (op == "start timer" || op == "stop timer")
        {
            const auto& d = decls[a.string("var")];
            if (op == "stop timer")
            {
                if (auto* s = writable(d.string("scope"), wolfOf(x, a.string("of")))) s->erase(a.string("var"));
                return Flow::Go;
            }
            const double at = now(d.string("clock", "game"));
            change(a, x, [&](const Value&) -> std::optional<Value> { return Value(at); });
        }
        else if (op == "shift")
        {
            const auto key = a.string("owner") == "player" ? a.string("signal") + "/" + x.actor : a.string("signal");
            if (state.has("timers"))
                if (auto* timer = state.find("timers")->find(key); timer && !timer->isNull())
                    timer->set("at", timer->number("at") + a.number("days"));
        }
        else throw std::invalid_argument("Unknown story action");
        return Flow::Go;
    }

    static Value with(const Value& address, const std::string& name)
    {
        Value v = address;
        v.push(Value(name));
        return v;
    }

    std::vector<Value> loop(const Value& a, const Ctx& x) const
    {
        std::vector<Value> out;
        const auto over = a.string("over");
        if (over == "participants") out = cohort.items();
        else if (over == "party")
        {
            if (member(x.actor))
                for (const auto& w : cohort.items())
                    if (partyOf(w.asString()) == partyOf(x.actor)) out.push_back(w);
        }
        else
        {
            const auto values = value(var(a.string("list"), ""), x);
            const bool wolves = decls[a.string("list")].string("type") == "wolves";
            for (const auto& v : values ? values->items() : json::Array{})
                if (!wolves || member(v.asString())) out.push_back(v);
        }
        if (out.size() > EachMost) out.resize(EachMost);
        return out;
    }

    bool matches(const Value& rule, const Value& fact) const
    {
        const auto on = rule.string("on");
        if (on != fact.string("kind") && !(on == "gift" && fact.string("kind") == "gift again"))
            return false;
        if (rule["signal"] != fact["signal"]) return false;
        for (const auto& [key, expected] : rule["match"].fields())
        {
            auto actual = fact.string(key);
            if (key == "item" && (on == "gift" || on == "gift again") && expected.asString().find_first_of("~@") == std::string::npos)
                actual = actual.substr(0, actual.find_first_of("~@"));
            if (actual != expected.asString()) return false;
        }
        return on != "clock" || day >= input.number("entered") + rule.number("afterDays");
    }

    void fire()
    {
        const auto& fact = input["fact"];
        const auto actor = fact.string("actor");
        const Value disabled = state["disabled"];   // (As the Python twin: who is off is read once, before the triggers.)
        for (const auto& rule : input.array("rules"))
        {
            const auto id = rule.string("id");
            if (includes(disabled, keyOf(id)) || !matches(rule, fact)) continue;
            const auto repeat = rule.string("once", "each");
            const bool personal = repeat == "player" || repeat == "member";
            if ((rule.string("audience") == "participant" || personal) && !member(actor)) continue;
            const bool global = repeat == "campaign" || repeat == "member";
            const char* bucket = global ? "campaignFired" : "fired";
            const auto token = global && !shared(id) ? phaseId + "/" + id : id;
            const auto owner = personal ? actor : "*";
            if (repeat != "each" && includes(state[bucket][token], owner)) continue;
            Ctx x{&rule, &fact, actor, std::nullopt, 0, 0};
            if (rule.has("if") && !test(rule["if"], x).value_or(false)) continue;
            if (repeat != "each") list(obj(state, bucket), token).push(Value(owner));
            x.firing = addFiring(rule, "");
            count(id);
            const auto r = run(rule["actions"], x, Value::array());
            if (r == Flow::Enter || r == Flow::Finish) break;
        }
    }

    void resumeWait()
    {
        const auto& w = input["resume"];
        const auto* rule = ruleById(w.string("trigger"));
        if (!rule) return;
        Ctx x{rule, &w["event"], w.string("actor"), std::nullopt, addFiring(*rule, "wait"), 0};
        resume((*rule)["actions"], x, Value::array(), w["next"]);
    }

    Value go()
    {
        if (input.has("resume")) resumeWait();
        else fire();
        result.set("firings", firings);
        result.set("state", state);
        if (!target.empty()) result.set("phase", target);
        return result;
    }
};
} // namespace ratw::story

// Thread-local storage keeps the returned bytes valid until the next dispatch on this thread.
extern "C" const char* ratw_story_dispatch_v1(const char* request)
{
    thread_local std::string output;
    try
    {
        ratw::json::Value input;
        std::string error;
        if (!request || !ratw::json::parse(request, input, error) || !input.isObject())
            throw std::invalid_argument("Invalid native story request");
        output = ratw::json::dump(input.number("v") == 2 ? ratw::story::Dispatch2(input).go() : ratw::story::Dispatch(input).run());
    }
    catch (const std::exception& e)
    {
        ratw::json::Value error;
        error.set("error", e.what());
        output = ratw::json::dump(error);
    }
    return output.c_str();
}
