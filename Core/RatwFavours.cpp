// Favours (Docs/Design/55-letters-gifts-favours.md, 7; Phase 3): grooming's effects in the world. Asking, consent and
// the fifteen seconds are Game's (RatwGameFavours.cpp); here, what a grooming does once done:
// - Well-groomed for 24 game hours or until the wolf's next full rest (the user, 2026-10-08); self-groomed, half of
//   every effect for 2 game hours;
// - first impressions: residents' liking and trust grow a quarter faster toward it while they hardly know it
//   (familiarity under 25);
// - healing: injuries heal a tenth faster at rest;
// - fewer lasting injuries: every lasting roll 10 points lower (5 self-groomed), floored at 0;
// - licked clean: a severe acute injury groomed by another carries `Injury::cleaned` (its setting roll 10 lower);
// - less scent for 2 game hours: smelt from 0.6 as far (0.8 self-groomed); masking oil still wins (0);
// - the bond, both ways, once a pair a game day.
#include "RatwWorld.h"

#include <algorithm>
#include <cmath>

namespace ratw
{
namespace
{
constexpr double GroomDays = 1, SelfDays = 2. / 24, ScentDays = 2. / 24;
}

double World::groomedFactor(const Entity& e) const
{
    return e.groomedUntil > calendarDays_ ? (e.groomHalf ? .5 : 1.) : 0.;
}

double World::scentScale(const Entity& e) const
{
    if (scentMasked(e))
        return 0;
    return e.groomScentUntil > calendarDays_ ? (e.groomHalf ? .8 : .6) : 1.;
}

void World::applyGrooming(const std::string& groomer, const std::string& groomed)
{
    auto* g = entity(groomed);
    if (!g)
        return;
    const bool self = groomer == groomed;
    if (self)
    {
        // Half, for 2 game hours; never in place of a grooming by another still on it.
        if (!(g->groomedUntil > calendarDays_ && !g->groomHalf))
        {
            g->groomedUntil = calendarDays_ + SelfDays;
            g->groomScentUntil = calendarDays_ + ScentDays;
            g->groomHalf = true;
            g->groomedBy = groomer;
        }
    }
    else
    {
        g->groomedUntil = calendarDays_ + GroomDays;
        g->groomScentUntil = calendarDays_ + ScentDays;
        g->groomHalf = false;
        g->groomedBy = groomer;
        for (auto& i : g->injuries)                 // Licked clean.
            if (i.kind == "acute" && injury::severityNow(i) >= 3)
                i.cleaned = true;
        const auto key = groomer < groomed ? groomer + "|" + groomed : groomed + "|" + groomer;
        if (const double today = std::floor(calendarDays_); !groomBondDay_.count(key) || groomBondDay_[key] < today)
        {
            groomBondDay_[key] = today;
            bonds_.mutual(groomer, groomed, {3, 2, 2, 0, 0}, calendarDays_);
        }
    }
    recordEvent({"groomed", groomer, self ? std::string() : groomed, g->cellId, 0, 0, {}, 0, 0, self ? "self" : "other"});
}

void World::addScent(const std::string& who, const std::string& item, int quantity, const std::string& maker, const std::string& giver)
{
    auto* e = entity(who);
    if (!e || e->npc || quantity <= 0 || (maker.empty() && giver.empty()))
        return;
    e->scents.push_back({item, maker, giver, quantity, maker.empty() ? -1 : calendarDays_, giver.empty() ? -1 : calendarDays_});
    while (e->scents.size() > 60)
        e->scents.erase(e->scents.begin());
}
} // namespace ratw
