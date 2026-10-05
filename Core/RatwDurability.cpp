#include "RatwWorld.h"

#include "RatwCalendar.h"
#include "RatwItems.h"

#include <algorithm>
#include <cctype>
#include <cmath>

// Wear and tear (Docs/Design/35-items-crafting-industry.md, Part 4 and phase 9). Gear in service wears: a sword with
// each blow it strikes, armour with each blow it takes, clothes and harness a little each day they are worn. How much
// use a piece takes is its catalog durability, longer for better made (items::qualityDurability). Worn out, it falls
// apart: one fewer in the purse, and the next of the same kind (if any) goes into service fresh. Wear is kept by item
// kind, not by slot, so taking a piece off and on again doesn't mend it. A shop that deals in a thing repairs it, for
// a fee to the shop.
namespace ratw
{
namespace
{
std::string lowerName(std::string s)
{
    if (!s.empty())
        s[0] = char(std::tolower(static_cast<unsigned char>(s[0])));
    return s;
}
} // namespace

int World::durabilityOf(const std::string& item)
{
    if (const auto* worn = items::wearable(item))
        return worn->durability;
    const auto* good = items::good(item);
    return good ? good->durability : 0;
}

double World::conditionOf(const Entity& e, const std::string& item) const
{
    const int most = durabilityOf(item);
    if (most <= 0)
        return 1;
    const auto used = e.wear.find(item);
    return used == e.wear.end() ? 1 : std::clamp(1 - used->second / most, 0.0, 1.0);
}

void World::wearGear(Entity& e, const std::string& item, double amount)
{
    const int most = durabilityOf(item);
    if (most <= 0 || amount <= 0 || item.empty())
        return;
    auto& used = e.wear[item];
    used += amount;
    if (used < most)
        return;
    // Worn out: it falls apart. Another of the kind, if there is one, takes its place fresh.
    e.wear.erase(item);
    const auto* good = items::good(item);
    const std::string name = good ? lowerName(good->name) : item;
    society_.consume(e.id, item, 1, "worn out");
    const auto* purse = society_.account(e.id);
    const bool spare = purse && Society::stock(*purse, item) > wornCount(e, item) - 1;
    if (!spare)
    {
        if (e.mouth == "sword" && (e.swordKind.empty() ? item == "sword" : e.swordKind == item))
        {
            e.mouth.clear();
            e.swordKind.clear();
        }
        for (auto it = e.worn.begin(); it != e.worn.end();)
            it = it->second == item ? e.worn.erase(it) : std::next(it);
        e.jewellery.erase(std::remove_if(e.jewellery.begin(), e.jewellery.end(), [&](const auto& j) { return j.second == item; }),
                          e.jewellery.end());
    }
    if (!e.npc)
        notice(e.id, "Your " + name + " is worn out and falls apart." + (spare ? " You take up another." : ""));
}

void World::wearArmourAt(Entity& e, const std::string& zone, double taken)
{
    if (zone.empty())
        return;
    // Each piece over where the blow fell: a point, and more for what it kept off.
    std::vector<std::string> hit;
    for (const auto& [slot, item] : e.worn)
        if (const auto* piece = items::wearable(item); piece && piece->protect > 0 && battle::armourZone(piece->slot) == zone)
            hit.push_back(item);
    for (const auto& item : hit)
        wearGear(e, item, 1 + std::max(0.0, taken) / 4);
}

std::string World::swordHeld(const Entity& e)
{
    if (e.mouth != "sword")
        return {};
    return e.swordKind.empty() ? std::string("sword") : e.swordKind;
}

void World::tendWear()
{
    // Clothes, harness and jewellery in service: a point a game day each (catalog durability is in days of wear).
    const double day = calendarDays_;
    if (lastWearDay_ < 0 || day < lastWearDay_)
    {
        lastWearDay_ = day;
        return;
    }
    const double elapsed = day - lastWearDay_;
    if (elapsed < 1.0 / 24)
        return;
    lastWearDay_ = day;
    for (auto& [id, e] : entities_)
    {
        if (e.npc || e.dead)
            continue;
        std::vector<std::string> worn;
        for (const auto& [slot, item] : e.worn)
            if (const auto* piece = items::wearable(item); piece && piece->protect <= 0)
                worn.push_back(item);           // (Armour wears with the blows it takes, not the days.)
        for (const auto& [spot, item] : e.jewellery)
            worn.push_back(item);
        for (const auto& item : worn)
            wearGear(e, item, elapsed);
    }
}

bool World::canRepair(const std::string& merchant, const std::string& item) const
{
    // A shop that sells or makes the thing, or deals in its kind (an armourer armour, a smith a sword).
    const auto* r = society_.spec(merchant);
    if (!r || r->role != "merchant" || durabilityOf(item) <= 0)
        return false;
    const auto base = items::baseOf(item);
    const auto wares = society_.wares(merchant);
    if (std::find(wares.begin(), wares.end(), base) != wares.end())
        return true;
    const auto* business = items::businessFor(r->workLabel);
    const auto* good = items::good(base);
    if (!business || !good)
        return false;
    for (const auto& s : business->sells)
        if (s == base || s == good->category)
            return true;
    for (const auto* craft : items::craftsFor(business->id))
        for (const auto& [out, n] : craft->out)
            if (out == base)
                return true;
    return false;
}

std::int64_t World::repairCost(const Entity& e, const std::string& item) const
{
    const auto* good = items::good(item);
    const double worn = 1 - conditionOf(e, item);
    if (!good || worn <= 0)
        return 0;
    return std::max<std::int64_t>(1, std::int64_t(std::ceil(good->price * worn * .5)));
}

Result World::repairGear(const std::string& player, const std::string& merchant, const std::string& item)
{
    auto* e = entity(player);
    const auto* m = entity(merchant);
    if (!e || e->dead || !m)
        return {false, "No one to mend it.", {}};
    if (m->cellId != e->cellId || std::hypot(m->position.x - e->position.x, m->position.y - e->position.y) > 2.5)
        return {false, "Get closer to them first.", {}};
    const auto* purse = society_.account(player);
    if (!purse || Society::stock(*purse, item) < 1)
        return {false, "You don't have one.", {}};
    if (!canRepair(merchant, item))
        return {false, "They don't mend that kind of thing.", {}};
    const auto cost = repairCost(*e, item);
    if (cost <= 0)
        return {false, "It needs no mending.", {}};
    if (purse->cash < cost)
        return {false, "You can't afford the mending (" + std::to_string(cost) + "p).", {}};
    if (!society_.shift(player, merchant, "", 0, cost, "repair"))
        return {false, "The mending can't be paid for.", {}};
    e->wear.erase(item);
    const auto* good = items::good(item);
    return {true, "Your " + (good ? lowerName(good->name) : item) + " is mended, for " + std::to_string(cost) + "p.", {}};
}
} // namespace ratw
