#pragma once
// Schedules (Docs/Design/26-living-npcs.md, Phase 9): the week, market days, festivals and the weather changing
// plans. The calendar gives every day its kind (Marketday, Restday, a festival on the season's day, or an ordinary
// working day); the world works out, for each community, where its market square is and who stands where; the society
// has its residents live by it (Society::setDay).
#include <cstdint>
#include <string>

namespace ratw
{
// A festival the Dungeon Master called for a community, on one calendar day.
struct CalledFestival
{
    std::string community, name;
    std::int64_t day = 0;
};
} // namespace ratw
