#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// The roads between towns (Docs/Design/26-living-npcs.md, Phase 5): caravans that really carry goods, bandits who
// really take them, contracts that give players and residents work on the road, and rumours that travel with them.
// A first version with placeholder numbers throughout; the World owns it (World::roads()).
namespace ratw
{
// A settlement: a region (cells' territory) with homes and a market. The capital's store is the treasury; each
// other town has its own ("stores:<town>"), filled by caravans.
struct Town
{
    std::string id, market, store;
    int residents = 0, guards = 0;
};

// Goods on the road between the capital and a town. The load is its own account ("caravan:<id>"); it moves a cell
// at a time, offstage, and may be raided where bandits camp.
struct Caravan
{
    std::string id, from, to, account;
    std::vector<std::string> route;       // Cells, from the capital's market to the town's.
    std::size_t leg = 0;                  // The cell it is in (index into route).
    double nextAt = 0, departed = 0;      // World seconds; calendar days.
    int guards = 1;
    std::string status = "travelling";    // "travelling", "arrived" or "raided".
    std::vector<std::string> escorts;     // Players who took an escort contract for it.
    std::vector<std::string> letters;     // Courier contracts it carries.
};

// Robbers in wild country along a road. Loot feeds them; hunger makes them bold; starving, they scatter.
struct BanditCamp
{
    std::string id, cell;
    double strength = 4, hunger = 30;     // 0..20, 0..100.
    double lastRaid = -100;               // Calendar day.
    bool active = true;
};

// Work anyone may take: a bounty on a camp, an escort, a supply run, a letter to carry. The reward is held in trust
// ("contract:<id>") until it is paid, or returned.
struct Contract
{
    std::string id, kind, poster, town;   // kind: "bounty", "escort", "supply", "courier".
    std::string target;                   // Bounty: the camp; courier: the recipient; escort/supply: the town.
    std::string taker, status = "open";   // "open", "taken", "done", "expired".
    std::int64_t reward = 0;
    double created = 0, due = 0;          // Calendar days.
    std::string detail;
};

// Something one character has heard about another (or a place): a claim, how sure they are, and who told them.
struct Belief
{
    std::string holder, subject, claim, source;
    double confidence = 1, day = 0;
};

struct RoadsState
{
    std::vector<Caravan> caravans;
    std::vector<BanditCamp> camps;
    std::vector<Contract> contracts;
    std::vector<Belief> beliefs;
    std::int64_t day = -1, nextId = 1;
    bool stocked = false;                 // The towns' stores have had their first share from the treasury.
};
} // namespace ratw
