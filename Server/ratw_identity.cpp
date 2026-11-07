// ratw_identity: central's check of a wolf's identity (Docs/Design/77-central-accounts-and-hosted-worlds.md, 4.1).
// Central (Central/ratw_central) must accept exactly the wolves a world's creation accepts, so it asks this command,
// built from Core with the game's own rules (RatwIdentity.h, wire::readAppearance, practice::checkBuild,
// people::applyFields), instead of keeping a copy of them in Python.
//
// One JSON request per line on stdin, one JSON reply per line on stdout, until stdin closes (central keeps one running):
//   {"check": "wolf", "wolf": {"name", "age", "appearance", "family"?}}
//       -> {"ok": true, "wolf": {"name", "age", "sex", "appearance", "family"}}: the name as it is kept (trimmed, a
//          capital first), the appearance in the game's own form, the sex out of it, "" for no family.
//   {"check": "look", "appearance": {...}}             -> {"ok": true, "appearance": {...}, "sex": "female" | "male"}
//   {"check": "profile", "profile": {...}?, "fields": {...}}
//       -> {"ok": true, "profile": {...}}: the saved profile (or a new one) with the fields applied, as the game saves it
//   {"check": "build", "tier": "normal"?, "family": ""?, "build": {...}}
//       -> {"ok": true, "build": {"grades": {...}, "specialty": "..."}} (for a world's first arrival, doc 77, Q12 (a))
//   {"check": "version"}                               -> {"ok": true, "version": 1}
// A refusal is {"ok": false, "error": "..."}, with the words creation uses. A line that isn't JSON is refused the same
// way. Data/ comes from RATW_DATA_DIR, else the working directory or above it, else the source tree.
#include "RatwCreation.h"
#include "RatwIdentity.h"
#include "RatwJsonDoc.h"
#include "RatwPeople.h"
#include "RatwWire.h"

#include <iostream>
#include <string>

using namespace ratw;
using json::Value;

namespace
{
constexpr int Version = 1;
constexpr std::size_t MaxLine = 64 * 1024;

Value refuse(const std::string& why)
{
    auto o = Value::object();
    o.add("ok", false);
    o.add("error", why);
    return o;
}

// Only these fields, so a request can't carry something central would store unchecked.
bool only(const Value& o, std::initializer_list<const char*> allowed)
{
    if (!o.isObject())
        return false;
    for (const auto& [key, v] : o.fields())
    {
        bool fine = false;
        for (const char* a : allowed)
            fine = fine || key == a;
        if (!fine)
            return false;
    }
    return true;
}

Value look(const Value& given, Appearance& a)
{
    if (!wire::readAppearance(given, a))
        return refuse("Choose a complete valid wolf appearance.");
    return Value();
}

Value check(const Value& request)
{
    const std::string what = request.string("check");
    if (what == "version")
    {
        auto o = Value::object();
        o.add("ok", true);
        o.add("version", Version);
        return o;
    }
    if (what == "wolf")
    {
        const auto& w = request["wolf"];
        if (!only(w, {"name", "age", "appearance", "family"}))
            return refuse("A wolf is a name, an age, an appearance and a Gift family, and nothing else.");
        const auto* rawName = w.find("name");
        const std::string name = rawName && rawName->isString() ? identity::name(rawName->asString()) : std::string();
        const double age = identity::age(w);
        Appearance a;
        if (name.empty() || age < 0 || !wire::readAppearance(w["appearance"], a))
            return refuse("Choose a 2\xe2\x80\x93" "32 character name, a whole age from 6\xe2\x80\x93" "99, and a complete valid wolf appearance.");
        std::string family;
        if (!identity::family(w.find("family"), family))
            return refuse("Choose one of the eight Gift families, or none.");
        auto out = Value::object();
        out.add("name", name);
        out.add("age", age);
        out.add("sex", a.sex);
        out.add("appearance", wire::appearance(a));
        out.add("family", family);
        auto o = Value::object();
        o.add("ok", true);
        o.add("wolf", out);
        return o;
    }
    if (what == "look")
    {
        Appearance a;
        if (auto no = look(request["appearance"], a); !no.isNull())
            return no;
        auto o = Value::object();
        o.add("ok", true);
        o.add("appearance", wire::appearance(a));
        o.add("sex", a.sex);
        return o;
    }
    if (what == "profile")
    {
        const auto* saved = request.find("profile");
        if (saved && !saved->isObject() && !saved->isNull())
            return refuse("A saved profile is an object.");
        people::Profile p = saved && saved->isObject() ? people::load(*saved) : people::Profile{};
        std::string why;
        if (!people::applyFields(p, request["fields"], why))
            return refuse(why);
        auto o = Value::object();
        o.add("ok", true);
        o.add("profile", people::save(p));
        return o;
    }
    if (what == "build")
    {
        std::string tier = request.string("tier", "normal"), family = request.string("family");
        auto gift = Value::object();
        gift.add("tier", tier);
        gift.add("family", family);
        std::string t, f;
        if (!identity::gift(&gift, t, f))
            return refuse("Choose Normal, or Gifted or Quickened with one of the eight Gift families.");
        practice::Build build;
        std::string why;
        if (!practice::checkBuild(request["build"], tier, build, why))
            return refuse(why);
        auto grades = Value::object();
        for (const auto& [attribute, grade] : build.grades)
            grades.add(attribute, grade);
        auto b = Value::object();
        b.add("grades", grades);
        b.add("specialty", build.specialty);
        auto o = Value::object();
        o.add("ok", true);
        o.add("build", b);
        return o;
    }
    return refuse("Unknown check: use wolf, look, profile, build or version.");
}
} // namespace

int main(int argc, char** argv)
{
    if (argc > 1)
    {
        std::cerr << "usage: ratw_identity < requests (one JSON request per line; see Server/ratw_identity.cpp)\n";
        return 2;
    }
    std::ios::sync_with_stdio(false);
    std::string line;
    while (std::getline(std::cin, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;
        Value request, reply;
        std::string error;
        if (line.size() > MaxLine)
            reply = refuse("The request is too long.");
        else if (!json::parse(line, request, error) || !request.isObject())
            reply = refuse("The request is not a JSON object.");
        else
            reply = check(request);
        std::cout << json::dump(reply) << '\n' << std::flush;
    }
    return 0;
}
