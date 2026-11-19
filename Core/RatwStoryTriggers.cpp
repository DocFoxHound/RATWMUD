// Native event/condition/action interpreter used by the world's story runner. Each call consumes one
// fact in one phase. The host commits the resulting changes together with the fact's durable cursor.
#include "RatwStoryRegion.h"
#include <algorithm>
#include <stdexcept>

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
        output = ratw::json::dump(ratw::story::Dispatch(input).run());
    }
    catch (const std::exception& e)
    {
        ratw::json::Value error;
        error.set("error", e.what());
        output = ratw::json::dump(error);
    }
    return output.c_str();
}
