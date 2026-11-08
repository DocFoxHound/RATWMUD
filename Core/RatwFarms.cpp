// The Country's farms as places of work (Docs/Design/71-the-country-s-industries.md, Phase C; doc 79, 17): the list of
// farms and works from Data/Economy/country_farms.json, each with its crew of posts and its work rooms (communities.json),
// which the farms' people fill (RatwFarmPeople.cpp) and succession keeps filled. Nothing here holds or moves money: the
// farms' tills, crews, yields, wages and rents went with doc 79's Phase 3, and every area is on.
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>

namespace ratw
{
void Society::buildFarms()
{
    farms_.clear();
    const auto& defaults = items::farms();
    for (std::size_t a = 0; a < defaults.areas.size(); ++a)
        for (const auto& def : defaults.areas[a].farms)
        {
            FarmNow f;
            f.def = &def;
            f.area = a;
            f.keepers = def.keepers;
            f.hands = def.hands;
            farms_.push_back(std::move(f));
        }
    // Each farm's work rooms (communities.json, doc 71 Phase C): a post there is the farm's. A works' (doc 72) is the cell
    // out of doors its centre is in: a post there is the works' by its labels (farmOfPost).
    farmWorkplace_.clear();
    for (const auto& place : items::farmPeople().farms)
        if (const auto at = std::find_if(farms_.begin(), farms_.end(), [&](const FarmNow& f) { return f.def->id == place.farm; });
            at != farms_.end())
        {
            at->outdoors = place.works;
            for (const auto& room : place.workplaces)
                farmWorkplace_.emplace(room.area, std::size_t(at - farms_.begin()));
        }
    countFarmPosts();
}
} // namespace ratw
