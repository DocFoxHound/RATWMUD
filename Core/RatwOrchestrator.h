#pragma once
// The economy's monitor: what is left of the economy orchestrator (Docs/Design/46-economy-orchestrator.md) since it
// stepped back (Docs/Design/64-economy-migration.md, Phase 12). Once a game day, at SnapshotHour, the society hands it a
// snapshot of the land (purses, food, work, the shops' food and prices, coins between towns), and it works out the day's
// measures: how hurt each town is and how, and how the land's money is spread. The evening of the weekly reckoning it
// also weighs the week. It measures and shows; it sets nothing. The society's own rules run the economy (doc 64), and the
// Dungeon Master has three levers of its own (the tax rate, the mint, a relief grant: Society::setTaxRate and the rest).
//
// measure() is a pure function of the snapshot and the monitor's memory, so a run repeats exactly.
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ratw::orchestra
{
constexpr double NourishADay = 50;                   // A grown wolf's food a day, in nourishment.

struct ResidentSnap
{
    std::string id, town, home;
    int age = 30;
    std::int64_t cash = 0;
    double hunger = 0;
    std::int64_t earned3 = 0, earned7 = 0;           // What it earned (wages, sales, odd jobs) in the last 3 and 7 days.
    bool dependent = false;                          // A child, the retired, one keeping the house: not looking for work.
};
struct HomeSnap
{
    std::string home, town;
    int members = 0;
    std::int64_t cash = 0;                           // Its members' purses together (a keeper's till aside).
    std::int64_t nourishment = 0;                    // Food at home: the larder and its members' pockets.
};
struct ShopSnap
{
    std::string till, town;
    std::int64_t takings = 0, running = 0;           // Today's takings, and a day's running (its float / FloatDays).
};
// A food on a town's shelves: what its shops hold of it together, and its going price there (the shops' mean).
struct GoodSnap
{
    std::string town, item;
    int stock = 0;
    double price = 0;
    int nourish = 0;                                 // What a piece feeds.
};
struct TownSnap
{
    std::string id;
    std::int64_t inflow = 0, outflow = 0;            // Coins into the town from elsewhere since the last snapshot, and out.
    // Players' help (doc 57, 5), since the last snapshot: coins players spent there (purchases, gifts to its projects), coins
    // it paid players (its shops buying from them), and goods players sold or handed in there, at its price. Shown only.
    std::int64_t playerIn = 0, playerOut = 0, playerGoods = 0;
};

struct Snapshot
{
    std::int64_t day = 0;                            // The game day the measures are for (the day after the snapshot's).
    // Whole days the society has counted earnings for (0 at its first snapshot: a new or restored world). Idleness waits
    // for three.
    int counted = 0;
    bool decide = false;                             // The week is weighed (the evening of the reckoning).
    std::int64_t moneySupply = 0;
    std::vector<ResidentSnap> residents;
    std::vector<HomeSnap> homes;
    std::vector<ShopSnap> shops;
    std::vector<GoodSnap> goods;
    std::vector<TownSnap> towns;
};

// What the monitor remembers from day to day (saved with the society).
struct Memory
{
    std::map<std::string, double> distress;          // Town -> its distress, smoothed.
    std::map<std::string, double> net;               // Town -> coins in less out a day, from other towns (a week's average).
    std::int64_t day = -1;                           // The day of its last measure.
    // The week's measures since it was last weighed: town -> the sum of its days' distress, and how many; "town|kind" ->
    // the sum of each kind of trouble's score.
    std::map<std::string, std::pair<double, int>> week;
    std::map<std::string, double> weekKinds;
    std::int64_t decided = -1;                       // The day the week was last weighed.
};

struct TownReading
{
    std::string id;
    int people = 0, idle = 0;
    double distress = 0, raw = 0;                    // Smoothed and today's.
    double week = -1;                                // The week's mean (when the week is weighed; -1 otherwise).
    std::string kind;                                // "" (well), "empty shelves", "empty purses", "no work", "failing trade", "draining".
    double foodCost = 0;                             // A day's plain food, pennies.
    double hungry = 0, starving = 0, short_ = 0, poor = 0;   // Shares of its people (short: of its households).
    double shopFoodDays = 0, takingsRatio = 0;
    std::int64_t netInflow = 0;
    std::int64_t playerIn = 0, playerOut = 0, playerGoods = 0;   // Players' help (doc 57): TownSnap's, as measured.
};
struct Brief
{
    std::int64_t day = -1;
    bool decided = false;                            // The week weighed: each town's week and the kind of trouble it mostly had.
    double landDistress = 0, gini = 0;               // (The Gini: of the residents' purses.)
    double residentShare = 0;                        // The residents' share of the land's money.
    double bottomShare = 0;                          // The poorer half's share of the households' money (by household, a head).
    std::int64_t moneySupply = 0, median = 0;
    std::vector<TownReading> towns;
};

// The day's measures: pure, given the same snapshot and memory, the same brief and memory after.
Brief measure(const Snapshot& snapshot, Memory& memory);

// The monitor's saved state (SocietyState::orchestrator): its memory, the latest day's measures (what the Dungeon
// Master's Money tab shows) and the latest week's.
struct State
{
    Memory memory;
    Brief last;                                      // The latest day's measures.
    Brief decision;                                  // The latest week's.
};
// (Saved as JSON: RatwOrchestratorJson.h.) A brief as JSON text.
std::string briefText(const Brief& brief);
} // namespace ratw::orchestra
