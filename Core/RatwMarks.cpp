#include "RatwWorld.h"

#include "RatwItems.h"

#include <algorithm>
#include <cmath>

// The nose and the maker's mark (Docs/Design/35-items-crafting-industry.md, Part 4).
//
// A wolf's nose is a physical stat (`Entity::smell`), sharpened by using it: every time it noses about, it grows a
// little, slower as it nears what a wolf's nose can be (by practice: doc 49). A masterwork carries its maker's scent with their mark, so a
// good nose knows whose work a thing is, and stolen goods give themselves away: the one robbed, the maker, or a guard
// on duty who catches the maker's scent on the thief still holding the thing knows it for stolen, and is a witness to
// the theft. Masking oil hides a wolf's own scent, and the scent of what it carries, for a few hours.
namespace ratw
{
namespace
{
constexpr double MaskHours = 4;             // Masking oil: game hours (a game hour is 600 world seconds).
constexpr double MarkCheckSeconds = 10;     // How often noses near a thief are tried.
constexpr double MarkCatch = .15;           // A try's chance, × the scent's clarity × the nose's keenness².
std::uint64_t mix(const std::string& a, std::int64_t b)
{
    std::uint64_t h = 1469598103934665603ULL ^ std::uint64_t(b) * 0x9E3779B97F4A7C15ULL;
    for (unsigned char c : a)
        h = (h ^ c) * 1099511628211ULL;
    h ^= h >> 31;
    return h * 0xBF58476D1CE4E5B9ULL;
}
} // namespace

double World::noseAcuity(const Entity& e)
{
    return std::max(0.0, e.smell) * std::clamp(e.noseHealth, 0.0, 1.0) * (1 + .75 * std::clamp(e.scentSkill / 100, 0.0, 1.0));
}

bool World::scentMasked(const Entity& e) const
{
    return e.scentMaskedUntil > time_;
}

void World::trainNose(const std::string& id)
{
    // How much, and how far it can go, is practice's (doc 49: "nose.use" in Data/Progression/skills.json).
    if (const auto* e = entity(id); e && e->noseHealth > 0)
        practise(id, "nose.use");
}

Result World::maskScent(const std::string& player)
{
    auto* e = entity(player);
    const auto* purse = society_.account(player);
    if (!e || e->dead || !purse)
        return {false, "No such character.", {}};
    // Any masking oil (doc 35, Part 4): the plainest first; a finer one lasts longer.
    const auto kinds = Society::kindsHeld(*purse, "masking_oil");
    if (kinds.empty())
        return {false, "You have no masking oil.", {}};
    const auto& oil = kinds.front();
    const double hours = MaskHours * items::qualityDurability(items::qualityOf(oil));
    if (society_.consume(player, oil, 1, "used") < 1)
        return {false, "You have no masking oil.", {}};
    e->scentMaskedUntil = std::max(e->scentMaskedUntil, time_) + hours * 600;
    return {true, "You work the masking oil into your coat. For a few hours your scent, and that of what you carry, is hidden.", {}};
}

std::vector<World::MarkSmelt> World::marksSmelt(const std::string& player) const
{
    // What a nose makes of the masterworks those near by carry: whose work they are.
    std::vector<MarkSmelt> out;
    const auto* me = entity(player);
    if (!me || me->noseHealth <= 0)
        return out;
    const double keen = noseAcuity(*me);
    for (const Entity* other : entitiesIn(me->cellId))
    {
        if (!other || other->id == player || other->dead || scentMasked(*other))
            continue;
        const auto* purse = society_.account(other->id);
        if (!purse)
            continue;
        const double clarity = scentClarity(player, other->id);
        if (clarity * keen < .5)
            continue;                           // A sharper nose, or nearer, downwind.
        for (const auto& [item, n] : purse->stock)
            if (n > 0 && !items::makerOf(item).empty() && items::makerOf(item) != other->id)
                out.push_back({other->id, item});
    }
    return out;
}

void World::tendMarks()
{
    if (time_ < nextMarkCheck_)
        return;
    nextMarkCheck_ = time_ + MarkCheckSeconds;
    // (By index: weighing an incident may add to the list.)
    for (std::size_t at = 0; at < crime_.incidents.size(); ++at)
    {
        auto& inc = crime_.incidents[at];
        if (inc.kind != "theft" || inc.status == "charged" || items::makerOf(inc.item).empty())
            continue;
        const auto* thief = entity(inc.offender);
        const auto* purse = society_.account(inc.offender);
        if (!thief || thief->dead || thief->offstage || scentMasked(*thief) || !purse || Society::stock(*purse, inc.item) < 1)
            continue;
        const auto maker = items::makerOf(inc.item);
        for (const Entity* o : entitiesIn(thief->cellId))
        {
            if (!o || !o->npc || o->dead || o->offstage || o->id == inc.offender)
                continue;
            if (o->id != inc.victim && o->id != maker && !guardOnDuty(o->id))
                continue;                       // Those who would know it: the one robbed, its maker, the watch.
            if (std::any_of(inc.witnesses.begin(), inc.witnesses.end(), [&](const Witness& w) { return w.id == o->id && w.identified; }))
                continue;
            const double clarity = scentClarity(o->id, inc.offender);
            const double keen = noseAcuity(*o);
            const double odds = MarkCatch * clarity * keen * keen;
            if (clarity <= 0 || double(mix(o->id + "|" + inc.id, std::int64_t(time_)) % 10000) / 10000 >= odds)
                continue;
            // Caught by the nose: the maker's scent on goods the thief has no business carrying.
            inc.witnesses.push_back({o->id, true, clarity, guardOnDuty(o->id)});
            const auto* victim = entity(inc.victim);
            const auto* good = items::good(inc.item);
            believe(o->id, inc.offender, "stole " + std::string(good ? good->name : "goods") + " from " + (victim ? victim->name : "someone"),
                    "smelt it", .7, inc.id);
            if (inc.status == "cold")
                inc.status = "open";
            if (!thief->npc)
                notice(inc.offender, o->name + " sniffs the air near you, and looks hard at what you carry.");
            recordEvent({"stolen goods smelt", o->id, inc.offender, thief->cellId, 0, 0, inc.item, 1, 0, inc.id});
            weigh(crime_.incidents[at]);
            break;                                  // (One nose at a time; the rest may catch it on a later try.)
        }
    }
}
} // namespace ratw
