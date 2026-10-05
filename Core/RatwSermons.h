#pragma once
// Sermons (Docs/Design/42-money-in-circulation.md, Phase 6): what preachers preach at the Restday service, from
// Data/Voice/sermons.json. Data only; the service is the society's and the game's.
#include <string>
#include <vector>

namespace ratw::sermons
{
struct Sermon
{
    std::string id, title, theme;
    std::vector<std::string> places;                // Communities it belongs to (its own faith's); empty: any.
    std::vector<std::string> lines;                 // A line a minute from the pulpit; "{town}" is the town's name.
};
// Loaded once (RATW_DATA_DIR, the working directory or above it, or the source tree); empty if they can't be read.
const std::vector<Sermon>& all();
// This week's for a community's church: one that belongs there or anywhere, never the same within four weeks. Null
// when there are none.
const Sermon* forWeek(const std::string& community, long long week);
} // namespace ratw::sermons
