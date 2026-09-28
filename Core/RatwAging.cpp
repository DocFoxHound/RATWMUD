#include "RatwWorld.h"
#include <algorithm>
#include <cmath>

namespace ratw
{
double ageVisionFactor(const Entity& actor) { return std::max(.45, 1. - std::max(0, actor.age - 64) * .015); }
double ageHearingFactor(const Entity& actor) { return std::max(.50, 1. - std::max(0, actor.age - 64) * .012); }
double effectiveDexterity(const Entity& actor)
{
    return actor.dexterity * std::max(.55, 1. - std::max(0, actor.age - 64) * .01);
}
int advanceAge(Entity& actor, double day)
{
    if (!std::isfinite(day) || day < 0 || day > calendar::MaxGameDays) return 0;
    if (actor.lastBirthdayDay < 0) { actor.lastBirthdayDay = day; return 0; }
    const double elapsed = day - actor.lastBirthdayDay;
    if (elapsed + 1e-8 < calendar::DaysPerYear) return 0;
    const int years = int(std::min(10000. - actor.age, std::floor((elapsed + 1e-8) / calendar::DaysPerYear)));
    if (years < 1) return 0;
    const int physicalYears = std::max(0, std::min(actor.age + years, 34) - std::min(actor.age, 34));
    const int wisdomYears = years - physicalYears;
    actor.strength = std::min(100., actor.strength + physicalYears);
    actor.dexterity = std::min(100., actor.dexterity + physicalYears);
    actor.wisdom = std::min(100., actor.wisdom + wisdomYears);
    actor.age += years;
    actor.lastBirthdayDay += years * calendar::DaysPerYear;
    if (!actor.npc) actor.ageNoticePending = std::min(10000, actor.ageNoticePending + years);
    return years;
}
} // namespace ratw
