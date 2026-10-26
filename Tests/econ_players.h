#pragma once
// Players who push (Docs/Design/64-economy-migration.md, Phase 13): econ_watch's scripted players, each scenario a thing
// players typically do that could upset the economy. They act only as the game lets a player act, through its own
// entry points (Society::addPlayer, Society::trade for a shop, World::steal, World::setDead), and every coin they have
// comes from the capital's treasury by a logged transfer, never made. Deterministic: no clock, no unseeded chance.
//
//   hoard   20 players hunt and gather near the capital and sell what they bring in, keeping every coin (the mint's case)
//   corner  a rich player buys up the capital's bread, flour and meals every morning for a week, then sells them back
//   dump    5 players sell swords, iron swords and hides to the shops every day, as many as the shops take
//   whale   a player arrives with 5% of the realm's money and spends it on rapiers and finery
//   poach   a Hold takes 20 of the capital's hands from their posts at twice the going rate
//   kill    the key makers die: a town's only miller, the biggest farm's farmer, the realm's smiths (succession)
//   theft   5 thieves pick the capital's purses every quarter of an hour and fence what isn't coin
//   winter  10 players buy all the firewood they can in the autumn and keep it (start near day 250: --from 6006)
#include "RatwWorld.h"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ratw::econ
{
// One thing a player did (players.csv): what, with which good, how many, for how many pennies.
struct PlayerMove
{
    std::int64_t day = 0;
    std::string player, scenario, action, item;
    int quantity = 0;
    std::int64_t coins = 0;
};

class Players
{
  public:
    static const std::vector<std::string>& names();          // The scenarios, in the order they act.
    // The scenarios asked for, from `fromDay` for `days` days (0: to the run's end).
    Players(World& world, std::vector<std::string> scenarios, std::int64_t fromDay, std::int64_t days);
    // Every tick: acts when a quarter of an hour comes round (otherwise nothing).
    void step();
    std::vector<PlayerMove> takeMoves();
    // days.jsonl's block: each scenario's players, what they hold, and what they did since the last call.
    std::string takeDay();

  private:
    struct Shop
    {
        std::string keeper, town;                   // The keeper (whom a player trades with) and its town.
    };
    World& world_;
    std::vector<std::string> scenarios_;
    std::int64_t from_ = 0, days_ = 0, quarter_ = -1;
    bool started_ = false, ended_ = false;
    std::string capital_;
    std::vector<Shop> shops_;                       // Every shop with its keeper, as of the day (keepers die and change).
    std::int64_t shopsDay_ = -1;
    std::map<std::string, std::vector<std::string>> who_;   // Scenario -> its players.
    std::map<std::string, std::vector<std::string>> holds_; // Scenario -> its other accounts (a Hold's treasury, chests).
    std::vector<WorkGround> grounds_;                        // The hoarders' grounds, worked in turn.
    std::map<std::string, std::set<std::string>> towns_;     // A player -> the towns it trades in ("": all).
    std::map<std::string, std::set<std::string>> loot_;      // A thief -> goods it has taken, to fence.
    std::set<std::string> held_;                             // Thieves the watch holds.
    std::vector<std::string> staff_, posts_;                 // poach's hands; kill's posts left empty.
    std::vector<PlayerMove> moves_;
    std::map<std::string, std::map<std::string, std::pair<std::int64_t, std::int64_t>>> today_;   // Scenario -> action -> qty, coins.

    void start();
    void end();
    void act(const std::string& scenario, std::int64_t day, int quarter);
    void refreshShops(std::int64_t day);
    std::string join(const std::string& scenario, int n);    // A player made ("player-<scenario>-<n>").
    std::int64_t fund(const std::string& scenario, const std::string& to, std::int64_t coins);
    void note(const std::string& scenario, const std::string& player, const std::string& action, const std::string& item,
              int quantity, std::int64_t coins);
    static bool trades(const Shop& shop, const std::set<std::string>& towns);   // In one of its towns (none: any).
    int buyAll(const std::string& scenario, const std::string& player, const Shop& shop, const std::string& base);
    int sellAll(const std::string& scenario, const std::string& player, const std::string& item);
    void stow(const std::string& scenario, const std::string& player);   // Goods into its chests, past a pack's kinds.
};
} // namespace ratw::econ
