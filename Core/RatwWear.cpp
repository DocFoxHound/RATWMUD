// Wearing (Docs/Design/35-items-crafting-industry.md, Phase 4): hats, scarves, coats, harnesses, satchels, capes and paw
// wraps in their slots, and jewellery clipped to the fur at a spot. What is worn stays in the purse, marked as worn, so
// it can't be sold off one's back; it is let go of if the purse no longer has it.
#include "RatwItems.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iterator>

namespace ratw
{
namespace
{
std::string lower(std::string s)
{
    if (!s.empty())
        s[0] = char(std::tolower(static_cast<unsigned char>(s[0])));
    return s;
}
} // namespace

int World::wornCount(const Entity& e, const std::string& item)
{
    int n = (e.mouth == "sword" ? swordHeld(e) == item : e.mouth == item) ? 1 : 0;
    for (const auto& w : e.worn)
        n += w.second == item;
    for (const auto& j : e.jewellery)
        n += j.second == item;
    return n;
}

Result World::wear(const std::string& id, const std::string& item, const std::string& where)
{
    auto* e = entity(id);
    if (!e || e->dead)
        return {false, "No such character.", {}};
    const auto* piece = items::wearable(item);
    if (!piece)
        return {false, "That can't be worn.", {}};
    if (e->downedLeft > 0)
        return {false, "You are down.", {}};
    if (inBattle(id))
        return {false, "Not in the middle of a fight.", {}};
    const auto* purse = society_.account(id);
    if (!purse || Society::stock(*purse, item) <= wornCount(*e, item))
        return {false, Society::stock(purse ? *purse : EconomyAccount{}, item) > 0 ? "You are already wearing every one you have."
                                                                                  : "You don't have one.", {}};
    const auto name = lower(piece->name);
    if (piece->slot == "jewelry")
    {
        std::string spot = where;
        if (spot.empty())
            spot = piece->spots.front();
        if (!items::spotAllowed(*piece, spot))
            return {false, "That doesn't clip on there.", {}};
        if (e->jewellery.size() >= items::MaxJewellery)
            return {false, "There is no more room in your fur.", {}};
        e->jewellery.emplace_back(spot, item);
        return {true, "You clip the " + name + " to " + items::placeName(spot) + ".", {}};
    }
    const auto slots = items::slotsFor(*piece);
    std::string slot = where;
    if (slot.empty())
    {
        slot = slots.front();
        for (const auto& s : slots)                     // (The first free side, for a satchel.)
            if (!e->worn.count(s))
            {
                slot = s;
                break;
            }
    }
    if (std::find(slots.begin(), slots.end(), slot) == slots.end())
        return {false, "That isn't worn there.", {}};
    std::string message = "You put on the " + name + ".";
    if (const auto old = e->worn.find(slot); old != e->worn.end())
        if (const auto* before = items::wearable(old->second))
            message = "You take off the " + lower(before->name) + " and put on the " + name + ".";
    e->worn[slot] = item;
    return {true, message, {}};
}

Result World::takeOff(const std::string& id, const std::string& where, const std::string& item)
{
    auto* e = entity(id);
    if (!e || e->dead)
        return {false, "No such character.", {}};
    if (inBattle(id))
        return {false, "Not in the middle of a fight.", {}};
    if (items::wearSlot(where))
    {
        const auto found = e->worn.find(where);
        if (found == e->worn.end())
            return {false, "You aren't wearing anything there.", {}};
        const auto* piece = items::wearable(found->second);
        e->worn.erase(found);
        return {true, "You take off the " + (piece ? lower(piece->name) : std::string("piece")) + ".", {}};
    }
    for (auto j = e->jewellery.rbegin(); j != e->jewellery.rend(); ++j)
        if (j->first == where && (item.empty() || j->second == item))
        {
            const auto* piece = items::wearable(j->second);
            e->jewellery.erase(std::next(j).base());
            return {true, "You unclip the " + (piece ? lower(piece->name) : std::string("piece")) + " from " + items::placeName(where) + ".", {}};
        }
    return {false, "There is nothing like that there.", {}};
}

std::string World::wornWords(const Entity& e)
{
    const auto list = [](const std::vector<std::string>& parts) {
        std::string out;
        for (std::size_t i = 0; i < parts.size(); ++i)
            out += (i == 0 ? "" : i + 1 == parts.size() ? " and " : ", ") + parts[i];
        return out;
    };
    std::vector<std::string> clothes, jewels;
    for (const char* slot : items::WearSlots)           // Head to paws, in that order.
        if (const auto found = e.worn.find(slot); found != e.worn.end())
            if (const auto* piece = items::wearable(found->second))
                clothes.push_back("a " + lower(piece->name) +
                                  (found->first == "chest_left" || found->first == "chest_right" ? " on " + items::placeName(found->first) : ""));
    for (const char* spot : items::FurSpots)
    {
        std::vector<std::string> here;
        for (const auto& [at, item] : e.jewellery)
            if (at == spot)
                if (const auto* piece = items::wearable(item))
                    here.push_back("a " + lower(piece->name));
        if (!here.empty())
            jewels.push_back(list(here) + " at " + items::placeName(spot));
    }
    std::string out;
    if (!clothes.empty())
        out = "Wearing " + list(clothes) + ".";
    if (!jewels.empty())
    {
        auto j = list(jewels);
        j[0] = char(std::toupper(static_cast<unsigned char>(j[0])));
        out += (out.empty() ? "" : " ") + j + ".";
    }
    return out;
}

namespace
{
// The watch's Professional kit (doc 47), by the slot it is worn in, and its blade.
const std::pair<const char*, const char*> WatchKit[] = {
    {"body", "leather_barding"}, {"neck", "leather_gorget"}, {"head", "leather_cap"}, {"paws", "leg_guards"}};
constexpr const char* WatchBlade = "iron_sword";
} // namespace

void World::kitOut(Entity& e, bool fighting)
{
    if (!e.npc)
        return;
    const auto* job = society_.jobOf(e.id);
    const bool guard = !e.dead && job && job->role == "guard";
    const auto* purse = society_.account(e.id);
    const auto owns = [&](const std::string& item) { return purse && Society::stock(*purse, item) > 0; };
    for (const auto& [slot, item] : WatchKit)
    {
        const auto worn = e.worn.find(slot);
        if (guard && worn == e.worn.end())
            e.worn[slot] = item;                    // (Worn out, or never had: issued again.)
        else if (!guard && worn != e.worn.end() && worn->second == item && !owns(item))
            e.worn.erase(worn);
    }
    const bool issued = e.mouth == "sword" && e.swordKind == WatchBlade && !owns(WatchBlade);
    if (guard && fighting && e.mouth.empty())
    {
        e.mouth = "sword";
        e.swordKind = WatchBlade;
    }
    else if (issued && (!guard || !fighting))
    {
        e.mouth.clear();
        e.swordKind.clear();
    }
}

void World::fitWorn(const std::string& id)
{
    auto* e = entity(id);
    const auto* purse = society_.account(id);
    if (!e || !purse)
        return;
    // Too many of something worn for what the purse holds (sold, lost, an old save): the last put on goes first.
    for (auto j = e->jewellery.size(); j-- > 0;)
        if (wornCount(*e, e->jewellery[j].second) > Society::stock(*purse, e->jewellery[j].second))
            e->jewellery.erase(e->jewellery.begin() + std::ptrdiff_t(j));
    for (auto w = e->worn.begin(); w != e->worn.end();)
        w = wornCount(*e, w->second) > Society::stock(*purse, w->second) ? e->worn.erase(w) : std::next(w);
    if (!e->mouth.empty() && Society::stock(*purse, e->mouth == "sword" ? swordHeld(*e) : e->mouth) < 1)
    {
        e->mouth.clear();
        e->swordKind.clear();
    }
}

World::Load World::loadOf(const Entity& e) const
{
    Load l;
    l.comfortable = (12 + .25 * std::clamp(e.strength, 0.0, 100.0)) * (time_ < e.lightLoadUntil ? 1.5 : 1);   // (Lighten Load, doc 43.)
    if (const auto* purse = society_.account(e.id))
        for (const auto& [item, count] : purse->stock)
            if (const auto* good = items::good(item); good && count > 0)
                l.carried += good->weight * count;
    const double over = l.carried / l.comfortable;
    if (over > 2)
    {
        l.state = "overloaded";
        l.pace = 0;
    }
    else if (over > 1)
    {
        // A notch off the top pace for each quarter over (sprint 10 down to a run of 6), and running tiring up to half
        // as much again. Placeholders for the balance pass.
        l.state = "heavy";
        l.pace = 10 - int(std::ceil((over - 1) / .25 - 1e-9));
        l.drain = 1 + .5 * (over - 1);
    }
    return l;
}

void World::refreshLoad(Entity& e) const
{
    if (e.npc)
        return;
    const auto l = loadOf(e);
    e.loadPace = l.pace;
    e.loadDrain = l.drain;
}

std::string World::tooLoadedToFight(const std::string& id) const
{
    const auto* e = entity(id);
    if (!e || e->npc || loadOf(*e).state != "overloaded")
        return {};
    return "You are carrying too much to fight. Put something down first.";
}
} // namespace ratw
