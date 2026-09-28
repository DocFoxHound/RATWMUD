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
    double marketX = 0, marketY = 0;      // Where the market's merchant stands: where caravans load and unload.
    int residents = 0, guards = 0;
};

// Goods on the road between two towns. The load is its own account ("caravan:<id>"). The wagon is a character
// in the world ("road:<id>", never saved itself): it walks the road where someone is near to see it, and goes in
// timed hops elsewhere, like anyone offstage. It may be raided where bandits camp.
struct Caravan
{
    std::string id, from, to, account;
    std::vector<std::string> route;       // Cells, from the one town's market to the other's (as planned).
    std::size_t leg = 0;                  // Cells entered since it set out.
    double nextAt = 0, departed = 0;      // (Unused since the wagon walks; kept for older saves); calendar days.
    int guards = 1;                       // Carters and town guards; escorts with it add to these.
    std::string status = "travelling";    // "travelling", "returning", "raided" or "arrived".
    std::vector<std::string> escorts;     // Those who took an escort contract for it: players or town guards.
    std::vector<std::string> letters;     // Courier contracts it carries.
    std::string cell;                     // Where the wagon is (empty: at its first market, not yet out).
    double x = 0, y = 0;
    double waitUntil = 0;                 // Calendar day: it waits at the market this long for its escorts.
    std::map<std::string, int> with;      // Cells each escort entered alongside it: paid only for being there.
};

// Robbers in wild country along a road. Loot feeds them; hunger makes them bold; starving, they scatter.
// Where someone is near, its bandits are there in person ("road:<camp>:<n>", never saved), and may stop a
// traveller for their purse or fight them.
struct BanditCamp
{
    std::string id, cell;
    double strength = 4, hunger = 30;     // 0..20, 0..100.
    double lastRaid = -100;               // Calendar day.
    bool active = true;
    double x = -1, y = -1;                // Where they camp in the cell (found the first time anyone comes near).
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
