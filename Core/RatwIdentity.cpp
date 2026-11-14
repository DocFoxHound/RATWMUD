#include "RatwIdentity.h"

#include "RatwAccountsCore.h"
#include "RatwGifts.h"
#include "RatwMind.h"
#include "RatwNames.h"
#include "RatwWire.h"

#include <cmath>

namespace ratw::identity
{
std::string name(const std::string& given)
{
    // A name always starts with a capital (the user, 2026-10-07): it reads as a name, and it is veiled from those who
    // don't know it wherever it is written (names::veil looks for capitalised names).
    const std::string n = names::capitalised(mind::trim(given));
    return accounts::validDisplayName(n) ? n : std::string();
}

std::string newName(const std::string& given)
{
    std::string n = name(given);
    return accounts::textUnits(n) <= std::size_t(NewNameMost) ? n : std::string();
}

double age(const json::Value& o, const std::string& key)
{
    const double a = wire::strictNumber(o, key, -1);
    return a < 6 || a > 99 || a != std::floor(a) ? -1 : a;
}

bool gift(const json::Value* given, std::string& tier, std::string& family)
{
    tier = "normal";
    family.clear();
    if (!given)
        return true;
    bool fine = given->isObject() && given->size() <= 2;
    for (const auto& [key, v] : given->fields())
        fine = fine && (key == "tier" || key == "family") && v.isString();
    tier = fine ? given->string("tier") : std::string();
    family = fine ? given->string("family") : std::string();
    return fine && (tier == "normal" || tier == "gifted" || tier == "quickened") && (tier == "normal") == family.empty() &&
           (family.empty() || gifts::playable(family));
}

bool family(const json::Value* given, std::string& out)
{
    out.clear();
    if (!given || given->isNull() || (given->isString() && given->asString().empty()))
        return true;
    if (!given->isString() || !gifts::playable(given->asString()))
        return false;
    out = given->asString();
    return true;
}
} // namespace ratw::identity
