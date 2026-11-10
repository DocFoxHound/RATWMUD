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
// other town has its own ("stores:<town>"), which is also its own treasury. The law's towns (World::towns) and, since doc
// 79's Phase 4, the road layer's markets (World::roadMarkets, from the Market Board's: every community with a merchant
// post, its store its treasury) are both Towns.
struct Town
{
    std::string id, market, store;
    double marketX = 0, marketY = 0;      // Where the market's merchant stands: where caravans load and unload.
    int residents = 0, guards = 0;
};

// A wagon on the road between two markets, or a farm's cart to its market (Docs/Design/79-a-staged-economy.md, 8.1-8.2):
// staged goods, a manifest for show (no account since doc 79's Phase 4: the old `account`, read from an older save, is
// ignored and the wagon comes home empty), driven by a real carter away from his post for the trip. The wagon is a
// character in the world ("road:<id>", never saved itself): it walks the road where someone is near to see it, and goes
// in timed hops elsewhere, like anyone offstage. It may be raided where bandits camp.
struct Caravan
{
    std::string id, from, to, account;    // from, to: markets (a cart's `from`, its farm's id); account: an older save's.
    std::string kind = "wagon";           // "wagon" (between markets: comes home again) or "cart" (a farm's: one way).
    std::vector<std::pair<std::string, int>> manifest;   // Base good -> pieces, the first first.
    std::map<std::string, double> sourceP;   // Each good's pressure at its source as it set out (7.4's arrival nudge).
    std::string driver;                   // The carter (a resident) driving it; "" none (a test's, an older save's).
    std::string departure;                // The committed departure it was (its notices'), "" none.
    std::string startCell, endCell;       // Where it loads (the yard, the farm) and unloads (the market's place).
    double startX = 0, startY = 0, endX = 0, endY = 0;
    std::vector<std::string> route;       // Cells, from the one market to the other's (as planned).
    std::size_t leg = 0;                  // Cells entered since it set out.
    double nextAt = 0, departed = 0;      // (nextAt unused since the wagon walks; kept for older saves); calendar days.
    double due = 0;                       // Calendar day it is due at the far end (0: never late).
    std::string lateCause;                // Why it is late, in words ("a broken axle"), once it is.
    std::size_t delayAt = 0;              // The leg a seeded mishap holds it at (0 none), and until when.
    double delayUntil = 0;
    bool overdue = false;                 // Posted overdue (a day past due), and the pressure that has added at its end.
    double latePushed = 0;
    int guards = 1;                       // Carters and town guards; escorts with it add to these.
    std::string status = "travelling";    // "travelling", "returning", "raided" or "arrived".
    std::vector<std::string> escorts;     // Those who took an escort contract for it: players or town guards.
    std::vector<std::string> letters;     // Courier contracts it carries.
    std::string cell;                     // Where the wagon is (empty: at its first market, not yet out).
    double x = 0, y = 0;
    double waitUntil = 0;                 // Calendar day: it waits at the market this long for its escorts and carter.
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

// Work anyone may take: a bounty on a camp, an escort, a letter to carry, a town project's work (doc 57). The reward is
// held in trust ("contract:<id>") until it is paid, or returned.
struct Contract
{
    std::string id, kind, poster, town;   // kind: "bounty", "escort", "courier"...
    std::string target;                   // Bounty: the camp; courier: the recipient; escort/supply: the town.
    std::string taker, status = "open";   // "open", "taken", "done", "expired".
    std::int64_t reward = 0;
    double created = 0, due = 0;          // Calendar days.
    std::string detail;
    // Offered by letter to one player first (doc 55, 5): only they may take it, from anywhere, until then.
    std::string offeredTo;
    double offeredUntil = 0;
};

// Something one character has heard about another (or a place): a claim, how sure they are, and who told them.
struct Belief
{
    std::string holder, subject, claim, source;
    double confidence = 1, day = 0;
    std::string incident;                 // What it is about, when it is a crime someone saw or heard of (RatwCrime.h).
    std::string as;                       // The name the holder knows the subject by ("" none: by look). Doc 56.
};

struct RoadsState
{
    std::vector<Caravan> caravans;
    std::vector<BanditCamp> camps;
    std::vector<Contract> contracts;
    std::vector<Belief> beliefs;
    std::int64_t day = -1, nextId = 1;
};
} // namespace ratw
