#pragma once
// Players who push (Docs/Design/64-economy-migration.md, Phase 13): econ_watch's scripted players, each scenario a thing
// players typically do that could upset the economy. They act as the game lets a player act, through its own entry
// points (Society::addPlayer, Society::trade for a shop, World::addPlayer and World::moveTo for a body, World::steal,
// World::helpAtWork at a bench, World::setDead, World::removePlayer), and every coin they have comes from the capital's
// treasury by a logged transfer, never made. The stand-ins, said where they are: hoard's yield is the residents'
// outworkers' (World::harvestAt), not a player's forage and hunt; the whale's chests are accounts of its own, free (no
// lodging, no rent); flood's catch is made in its packs; gift's coins go by Society::shift as Game::gifted's do, with
// no body walking up to each household. A player's own goods never spoil (Society::spoil passes players by), as in the
// game. Deterministic: no clock, no unseeded chance.
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
//   flood   10 players sell a staple (raw meat unless asked: --players-staple) to the capital's shops on the hour from 8
//           to 18 for two weeks, as many as they take, then stop (does the price come back, and do the capital's makers
//           of it keep their living?)
//   strike  players' Holds take a tenth of the capital's hands from their work for a month, as stationed companions, then
//           let them go (do hires and wages answer, and does the town come back?)
//   buyout  5 players with 30,000p each buy every piece of food on the capital's shelves on the hour from 6 to 20 for a
//           week, their purses spread over its hours so they last it, and hold it (a food corner: unlike corner's, all
//           its food, and never sold back)
//   drain   players take every place at every bench in a town (World::helpAtWork: two beside each maker, the most a
//           bench takes) each working day, paid by the beat from Society::tillOf's account for the maker (a hand's
//           own purse: its post has no till) (do the town's own hands lose their work?)
//   gift    a rich player gives 100p a day to each of the realm's 50 poorest households (does it lift prices, and is
//           every coin still there?)
//
// theft's and visit's bodies walk (World::moveTo up to a mark in the place they're in, acting on arrival; on into the
// next of the capital's places on the hour, or when theirs is empty), stand still where they arrived (a thief trying a
// purse that comes within reach, each purse once a day), or jump (World::teleport beside each mark: a Dungeon Master's
// move, not a player's; the first runs', kept to compare): Bodies. drain's stand at their benches.
#include "RatwWorld.h"

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ratw::econ
{
// One thing a player did (players.csv): what, with which good, how many, for how many pennies (pennies that moved; a
// Hold's shortfall, "short", has the pennies it lacked as its quantity, item "pennies lacking", and none moved).
struct PlayerMove
{
    std::int64_t day = 0;
    std::string player, scenario, action, item;
    int quantity = 0;
    std::int64_t coins = 0;
};

// econ_watch's stamp's check (doc 64, Phase 13; here so it can be tested): whether the code a binary was built from
// (Tests/econ_watch_stamp.cmake: its commit, and the SHA-1 of the changes from it to `paths`, the code it builds from)
// is still what `source` holds. "" when it is; else a line to print: a warning when the code has changed, or a note
// when it can't be told (built from no git checkout, or no git or no such commit here now). A docs edit is no change.
std::string sourceDrift(const std::string& source, const std::string& commit, const std::string& code, const std::string& paths);
// The Data econ_watch reads at run time (the catalogue's rule, RatwItems.cpp's itemsDir: RATW_DATA_DIR, else the
// working directory or one above it with Data/Items/items.json, else `source`'s Data); and run.txt's line on it: where,
// and its state in version control as the run found it (the checkout's describe, dirty or not, and the SHA-1 of the
// changes to it since its commit, its untracked files' names too), or that it is in no checkout. A line to read, never
// a warning: Data is read at run time, not built in.
std::string dataDir(const std::string& source);
std::string dataState(const std::string& dir);
// econ_watch --hold-pace's: a run's pace.csv as the society's day -> the realm's pace that day (Society::holdPace); none
// at all if any line isn't one.
std::map<std::int64_t, double> readPace(const std::string& text);

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
    static constexpr int FloodDays = 14, StrikeDays = 28;    // flood's selling, strike's month (from when they start).
    static constexpr int BuyoutDays = 7;                     // buyout's buying week (it holds what it bought after),
    static constexpr int BuyoutFrom = 6, BuyoutTo = 20;      // on the hour from six to eight in the evening.
    static constexpr int GiftHouseholds = 50, GiftEach = 100;   // gift's poorest households, and its pennies to each a day.
    // flood's staple unless asked (--players-staple): several pennies a piece, bought by the capital's shops (its
    // provisioners, butchers and inns) and brought in by its own hunters. Fish sat at the penny's floor at a 12p day
    // (Society::pennies: 1p at least), so its price could never be seen to fall.
    static constexpr const char* DefaultStaple = "raw_meat";
    // A staple flood may sell: a good of the catalogue priced by its sellers (not by the stores: meals, herbs, swords).
    static bool stapleKnown(const std::string& item);
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
    const std::vector<std::string>& staffOf(const std::string& scenario) const;      // poach's and strike's hands, held now.
    std::string markOf(const std::string& body) const;       // Whom a walking body is going up to ("": nobody).
    int aims() const { return aims_; }                       // World::moveTo calls the bodies have made (a test's count).
    void setStaple(const std::string& item) { staple_ = item; }   // flood's (before they start; stapleKnown).
    const std::string& staple() const { return staple_; }
    const std::string& drainTown() const { return drainTown_; }   // The town drain works in ("": not yet).
    std::string benchOf(const std::string& body) const;           // The maker a drain body works beside.

  private:
    struct Shop
    {
        std::string keeper, post, town;             // The keeper (whom a player trades with), its post and its town.
    };
    struct Walker
    {
        std::string mark;                           // Whom it walks up to ("": nobody).
        Vec2 aim;                                   // The mark's spot when it last aimed at it.
        double since = 0;                           // World seconds it set off (it gives up after a quarter of an hour).
        double aimedAt = 0;                         // World seconds it last aimed (World::moveTo): every AimEvery at most.
        std::int64_t tried = -1;                    // The quarter it last put its paw in a purse (one a quarter).
        std::string last;                           // The last it went up to (the next is someone else).
        std::int64_t day = -1;                      // Standing still: the day of `today`,
        std::set<std::string> today;                // the purses it has tried that day (each once a day).
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
    int aims_ = 0;
    std::set<std::string> leaving_;                          // Bodies to take out of the world, once out of a fight.
    std::map<std::string, std::vector<std::string>> staff_;  // poach's and strike's hands, held now.
    std::map<std::string, std::string> jobWas_;              // strike's hands -> the post they had when taken.
    std::vector<std::string> taken_;                         // strike's hands, all it took (held or let go).
    bool struck_ = false, flooded_ = false;                  // strike's month over (its hands let go); flood's selling.
    bool makersFound_ = false;
    std::string staple_ = DefaultStaple;                     // flood's staple.
    std::vector<std::string> posts_;                         // kill's posts left empty.
    std::vector<std::string> makers_;                        // flood's makers: the capital's posts that bring in the staple.
    std::string drainTown_;                                  // drain's town,
    std::vector<std::string> benches_;                       // its makers at a bench (a business that crafts),
    std::map<std::string, std::string> bench_;               // and its bodies -> the maker each works beside.
    double drainLooked_ = -1;                                // World seconds drain's bodies last looked about them,
    std::int64_t drainQuarter_ = -1;                         // and the quarter they last asked to work (once a quarter).
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
    std::int64_t companionWage(const std::string& id) const;   // A hired companion's (Game::wageFor): strike's.
    // Hands off their schedules (World: a leader, as a stationed companion's), held by a Hold: taken, paid each dawn
    // from its treasury (topped up from the capital's), and let go.
    void take(const std::string& scenario, const std::string& chapter, const std::vector<std::string>& hands);
    void payStaff(const std::string& scenario, const std::function<std::int64_t(const std::string&)>& wage);
    void letGo(const std::string& scenario);
    void note(const std::string& scenario, const std::string& player, const std::string& action, const std::string& item,
              int quantity, std::int64_t coins);
    static bool trades(const Shop& shop, const std::set<std::string>& towns);   // In one of its towns (none: any).
    int buyAll(const std::string& scenario, const std::string& player, const Shop& shop, const std::string& base,
               std::int64_t* budget = nullptr);           // (budget: what it may spend, less what it paid.)
    int sellAll(const std::string& scenario, const std::string& player, const std::string& item);
    void stow(const std::string& scenario, const std::string& player);   // Goods into its chests, past a pack's kinds.
    bool markable(const std::string& id) const;     // A purse a thief may try: the capital's grown, the watch aside.
    void lookAbout();                               // Walking and standing bodies, each tick or so.
    void setOff(const std::string& scenario, const std::string& body, std::int64_t day, int quarter);
    void tryPurse(const std::string& scenario, const std::string& thief, const std::string& mark);
    void leave();                                   // The bodies out of the world (World::removePlayer), when they may.
    std::string capitalNow() const;                 // The capital (before they start, the society's).
    void findMakers();                              // flood's makers, once the capital is known.
    static bool crafts(const std::string& workLabel, const std::string& item);   // Its business makes it (crafts.json).
    std::string floodDay() const;                   // flood's block: the staple's price in the capital, its makers.
    std::string strikeDay() const;                  // strike's block: its hands, and the town's going rate.
    void drainStart();                              // drain's town, its benches, and a body for each place at them.
    void drainLook();                               // drain's bodies at their benches (World::helpAtWork), each look.
    std::string drainDay() const;                   // drain's block: its town's benches, and its hands' hires.
    void giveAway(const std::string& scenario);     // gift's day: to the poorest households.
};
} // namespace ratw::econ
