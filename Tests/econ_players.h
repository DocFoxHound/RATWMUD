#pragma once
// Players who push (Docs/Design/64-economy-migration.md, Phase 13): econ_watch's scripted players, each scenario a thing
// players typically do that could upset the economy. They act as the game lets a player act, through its own entry
// points (Society::addPlayer, Society::trade for a shop, World::addPlayer and World::moveTo for a body, World::steal,
// World::setDead, World::removePlayer), and every coin they have comes from the capital's treasury by a logged
// transfer, never made. Two stand-ins, said where they are: hoard's yield is the residents' outworkers' (World::harvestAt),
// not a player's forage and hunt; the whale's chests are accounts of its own, free (no lodging, no rent).
// Deterministic: no clock, no unseeded chance.
//
//   hoard   20 players hunt and gather near the capital and sell what they bring in, keeping every coin (the mint's case)
//   corner  a rich player buys up the capital's bread, flour and meals every morning for a week, then sells them back
//   dump    5 players sell swords, iron swords and hides to the shops every day, as many as the shops take
//   whale   a player arrives with 5% of the realm's money and spends it on rapiers and finery
//   poach   a Hold takes 20 of the capital's hands from their posts at twice the going rate
//   kill    the key makers die: a town's only miller, the biggest farm's farmer, the realm's smiths (succession)
//   theft   5 thieves pick the capital's purses every quarter of an hour and fence what isn't coin
//   visit   5 players go about the capital as the thieves do and take nothing (theft's control: bodies on the stage)
//   winter  10 players buy all the firewood they can each autumn morning and keep it (start near day 250: --from 6006)
//
// theft's and visit's bodies walk (World::moveTo up to a mark in the place they're in, acting on arrival; on into the
// next of the capital's places on the hour, or when theirs is empty), stand still where they arrived (a thief trying
// any purse that comes within reach), or jump (World::teleport beside each mark: a Dungeon Master's move, not a
// player's; the first runs', kept to compare): Bodies.
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
    enum class Bodies
    {
        Walk,
        Still,
        Jump
    };
    static const std::vector<std::string>& names();          // The scenarios there are (they act in the order asked).
    static bool bodiesNamed(const std::string& name, Bodies& out);   // "walk", "still", "jump".
    // The scenarios asked for, from `fromDay` (or the run's first day, if later) for `days` days counted from when they
    // start (0: to the run's end).
    Players(World& world, std::vector<std::string> scenarios, std::int64_t fromDay, std::int64_t days, Bodies bodies = Bodies::Walk);
    // Every tick: walking bodies look about them; the rest acts when a quarter of an hour comes round.
    void step();
    std::vector<PlayerMove> takeMoves();
    // days.jsonl's block: each scenario's players, what they hold, and what they did since the last call.
    std::string takeDay();
    std::int64_t startDay() const { return startDay_; }      // The day they started (-1: not yet).
    bool ended() const { return ended_; }
    const std::vector<std::string>& playersOf(const std::string& scenario) const;
    const std::vector<std::string>& accountsOf(const std::string& scenario) const;   // A Hold's treasury, the chests.

  private:
    struct Shop
    {
        std::string keeper, post, town;             // The keeper (whom a player trades with), its post and its town.
    };
    struct Walker
    {
        std::string mark;                           // Whom it walks up to ("": nobody).
        Vec2 aim;                                   // The mark's spot when it last set off.
        double since = 0;                           // World seconds it set off (it gives up after a quarter of an hour).
        std::int64_t tried = -1;                    // The quarter it last put its paw in a purse (one a quarter).
        std::string last;                           // The last it went up to (the next is someone else).
    };
    World& world_;
    std::vector<std::string> scenarios_;
    std::int64_t from_ = 0, days_ = 0, quarter_ = -1, startDay_ = -1, firstDay_ = -1;
    Bodies bodies_ = Bodies::Walk;
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
    std::map<std::string, Walker> walkers_;                  // theft's and visit's bodies, as they go about.
    double lookedAt_ = -1;                                   // World seconds the bodies last looked about them.
    std::set<std::string> leaving_;                          // Bodies to take out of the world, once out of a fight.
    std::vector<std::string> staff_, posts_;                 // poach's hands; kill's posts left empty.
    std::vector<PlayerMove> moves_;
    std::map<std::string, std::map<std::string, std::pair<std::int64_t, std::int64_t>>> today_;   // Scenario -> action -> qty, coins.

    void start(std::int64_t day);
    void end();
    void act(const std::string& scenario, std::int64_t day, int quarter);
    void refreshShops(std::int64_t day, bool again = false);
    bool open(const Shop& shop) const;              // Its keeper living, and still at its post.
    std::string join(const std::string& scenario, int n);    // A player made ("player-<scenario>-<n>").
    std::int64_t fund(const std::string& scenario, const std::string& to, std::int64_t coins);
    std::int64_t poachWage() const;                 // Twice a Hold's (Game::keepWage).
    void note(const std::string& scenario, const std::string& player, const std::string& action, const std::string& item,
              int quantity, std::int64_t coins);
    static bool trades(const Shop& shop, const std::set<std::string>& towns);   // In one of its towns (none: any).
    int buyAll(const std::string& scenario, const std::string& player, const Shop& shop, const std::string& base);
    int sellAll(const std::string& scenario, const std::string& player, const std::string& item);
    void stow(const std::string& scenario, const std::string& player);   // Goods into its chests, past a pack's kinds.
    bool markable(const std::string& id) const;     // A purse a thief may try: the capital's grown, the watch aside.
    void lookAbout();                               // Walking and standing bodies, each tick or so.
    void setOff(const std::string& scenario, const std::string& body, std::int64_t day, int quarter);
    void tryPurse(const std::string& scenario, const std::string& thief, const std::string& mark);
    void leave();                                   // The bodies out of the world (World::removePlayer), when they may.
};
} // namespace ratw::econ
