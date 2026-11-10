#pragma once

// The Market Board (Docs/Design/79-a-staged-economy.md, sections 5 and 7; Phase 1): a pressure P for each market and good that players' trade at the tills pushes up or down and that decays back
// to 0; the clamp every price factor sits inside; the per-piece ladder; the per-holder decaying cap; the day's net
// positions and the morning's ripples through recipes; the shops' buying budgets, take allowances and the per-holder
// sell total. The pure parts (data, factors, decay, the ladder, links) are here; Society applies them (quote, trade, the
// morning restock: RatwMarket.cpp's Society part).

#include "RatwJsonDoc.h"

#include <array>
#include <cstdint>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace ratw::market
{
// The constants no rule may move (doc 79, 7.6): the clamp's ends, the sell share, the one-shop bound (I2), the
// backstop (I3), the stall's share, the crafting premium (I5), P's range.
inline constexpr double FactorLow = .80, FactorHigh = 1.28;
inline constexpr double SellShare = .55, OneShopShare = .6, BackstopShare = .95, StallShare = .9, CraftPremium = 1.6;
inline constexpr double PressureMost = .5;
inline constexpr double DepthLeast = 20, PassingTrade = 20;
inline constexpr double RippleMost = .15;             // Ripple-caused |P| on any good (7.4).
inline constexpr std::size_t RippleKeep = 6;          // Goods kept at each of the two steps (7.4).
inline constexpr std::size_t RecentKept = 50;
inline constexpr double DawnHour = 6;                 // The market day turns at dawn (budgets, allowances, ripples).
inline constexpr int SliderStepADay = 10;             // The slider the Board reads moves at most this a day (D17).
inline constexpr double SettingOutDays = 15. / 1440;   // A keeper "setting out the wares" at the restock: 15 game minutes.

// Data/Economy/market.json (missing or broken: these defaults, with a warning on stderr).
struct Data
{
    bool loaded = false;
    std::map<std::string, double> groups;            // Group -> depthBase.
    std::map<std::string, std::string> goods;        // Good -> group (explicit).
    std::map<std::string, std::string> categories;   // Catalogue category -> group.
    std::string fallback = "tools";
    std::map<std::string, std::map<std::string, double>> district;   // Quarter -> group (or "all") -> factor.
    std::map<std::string, std::array<double, 5>> times;               // Group -> factor at hard..boom.
    std::array<int, 4> timesCuts{-60, -20, 20, 60};
    double seasonSpanMost = 1.06;
    double regionMade = .95, regionNear = 1, regionFar = 1.08;
    double budget = 40;
    int routes = 3;
    std::map<std::string, std::vector<std::string>> buys;   // Business -> goods it also buys (D5).
    std::map<std::string, int> shelves;              // Base good -> its shelf (else the starting stock: 4, swords 3).
    std::map<std::string, int> rules;                // The rules' defaults (items::Rules' names), over Rules' own.
    // The seven season groups (7.1, moved from city_loop.json): each group's goods and, by season (Spring, Summer, Autumn,
    // Winter: the value at each season's middle day, blended between), its price and its shelf in percent of normal.
    struct Season
    {
        std::string id, name;
        std::vector<std::string> goods;
        std::array<int, 4> price{100, 100, 100, 100}, shelf{100, 100, 100, 100};
    };
    std::vector<Season> seasons;
    std::map<std::string, std::size_t> seasonOf;    // Base good -> its group (the first that names it).
};
const Data& data();
// A base good's group: its explicit one, else its category's, else the fallback; `how` (if given) says which
// ("good", "category", "fallback").
std::string groupOf(const std::string& base, std::string* how = nullptr);
double depthBase(const std::string& base);
// The Times word for a score (0 hard .. 4 boom), and its name.
int timesOf(double score);
const char* timesName(int times);
// A season group's value (percent of normal by season) on `yearDay` as a factor (100 = 1): each season's at its middle
// day, a straight line between, Winter's running on into Spring's; before the date is known (yearDay < 0), 1.
double seasonal(const std::array<int, 4>& by, double yearDay);
// The season's price factor the Board uses (7.1): `seasonal`, its swing (the dearest season over the cheapest) rescaled
// to at most data().seasonSpanMost, so that at a season's ends, with the region factors, players' pressure keeps room
// inside the clamp (the user, 2026-11-08: at least x1.10 up and x0.90 down; market_tests pins it).
double seasonFactor(const std::array<int, 4>& by, double yearDay);
const Data::Season* seasonOf(const std::string& base);

// The Board's rules as this world has them (Society::marketRules, from the rules of the same names).
struct Rules
{
    double halfLifePerishable = 2, halfLifeKeeping = 5, halfLifeDurable = 8;   // Days.
    double ripple = .5, depthScale = 1, pushMost = .15, takeAllowance = .5, shelfScale = 1;
    std::int64_t sellMost = 400, discountMost = 30;
};
// A good's half-life: perishable food (keeps a week or less) 2 days, keeping food 5, anything else 8 (doc 79, 7.3).
double halfLife(const std::string& base, const Rules& rules);
// `value` decayed from `since` to `now` (game days) at `halfLife` days: exact at any speed, so the lazy read and the
// nightly sweep agree.
double decayed(double value, double since, double now, double halfLife);
// The whole factor: base (season, region, district, Times) times exp(P), within the clamp; NaN or infinity reads 1.
double factor(double base, double P);
// The base goods a business makes a market's own (7.1's region `made`): its crafts' outputs and its crafts.json
// `supplies`, added to `made`.
void madeBy(const std::string& business, std::set<std::string>& made);
// A bought lot of `quantity` pieces whose ladder summed to `sum` (catalogue `each` a piece): rounded (at a stall × 0.9
// and rounded down), at least 1p, and never under the clamp floor's stall price, ceil(each × 0.80 × 0.9 × quantity)
// (I3, 7.2). The cheapest way to buy a lot is at a stall.
std::int64_t boughtLot(double sum, double each, int quantity, bool stall);
bool finite(double x);

// The Board (5.2), saved with the society. Only non-zero cells are kept.
struct Cell
{
    double P = 0, touched = 0;
    std::string cause;                               // players, dump, caravan, raid, ship, harvest, event, ripple.
};
struct Push
{
    double amount = 0, touched = 0;
};
struct Ripple
{
    double day = 0, delta = 0;
    std::string market, good, cause;                 // Cause words only, never a player's name.
};
struct Board
{
    // The DM's prosperity, the one slider of the staged economy (doc 79, 5.2): the smoothed value the Board's Times and
    // budgets and Phase 2's bands all read, where it heads, and the day it last stepped (-1: not yet). It moves at most
    // the status table's smoothing (SliderStepADay) a day (D17; Society::stepSlider).
    int slider = 0, sliderTarget = 0;
    std::int64_t sliderDay = -1;
    std::map<std::string, std::map<std::string, Cell>> cell;                 // Market -> base good.
    std::int64_t day = -1;                           // The market day the counts below are for.
    std::map<std::string, std::int64_t> budgetLeft;  // Till -> its buying budget left today (absent: the whole).
    std::map<std::string, std::map<std::string, int>> takenToday;           // Till -> base good -> pieces.
    std::map<std::string, std::map<std::string, std::map<std::string, Push>>> pushed;   // Market -> holder -> good.
    std::map<std::string, std::map<std::string, std::map<std::string, int>>> netToday;  // Market -> good -> holder:
                                                                                         // sold minus bought.
    std::map<std::string, std::int64_t> soldToday;   // Holder -> pennies paid by shops today.
    std::int64_t week = -1;                          // The game week the faction discounts are for.
    std::map<std::string, std::int64_t> discountWeek;   // Chapter -> pennies of discount paid this week.
    std::map<std::string, std::int64_t> restocked;   // Till -> the market day it last restocked.
    std::map<std::string, double> settingOut;        // Till -> the clock (game days) its keeper sets out the wares
                                                     // until, at the morning restock (7.7).
    std::deque<Ripple> recent;
    bool empty() const
    {
        return slider == 0 && sliderTarget == 0 && sliderDay < 0 && cell.empty() && day < 0 && budgetLeft.empty() && takenToday.empty() &&
               pushed.empty() && netToday.empty() && soldToday.empty() && week < 0 && discountWeek.empty() &&
               restocked.empty() && settingOut.empty() && recent.empty();
    }
};
json::Value encode(const Board& board);
// Lenient (doc 79, 5.2 and 14): anything malformed is dropped, numbers clamped, never a refusal.
Board decode(const json::Value& value);

// Markets, found at load (5.1): a trade point for each community with a merchant post (its id the community's), lone
// merchants joined to the nearest by path or to "outside", and the k nearest other markets by path.
struct TradePoint
{
    std::string id, place;                           // place: the cell of its thickest merchants.
    std::vector<std::string> tills;
    std::set<std::string> cells;                     // The cells of its community (law aside), and of the home regions
                                                     // it serves (the nearest market by path to a region of homes that
                                                     // is no market): where its households live (Society::marketOfHome).
    double catchment = PassingTrade;                 // Households, farm and works crews, and passing trade.
    std::vector<std::string> routes;                 // Other markets, nearest first.
    std::set<std::string> made;                      // Base goods made here: its workshops' and its areas'.
    std::set<std::string> sold;                      // Base goods its shops set out (not those they only buy).
};
struct Topology
{
    std::vector<TradePoint> markets;
    std::unordered_map<std::string, std::size_t> ofTill, ofId, ofCell;
    bool fromWorld = false;                          // Found by the world; else Society's own (one market).
    const TradePoint* find(const std::string& id) const;
    const TradePoint* ofTillId(const std::string& till) const;
    const TradePoint* ofCellId(const std::string& cell) const;
    void index();
};

// The links through goods (7.4), from crafts.json and recipes.json: each good's outputs and inputs with how many recipes
// use the link. Built once.
struct Links
{
    std::map<std::string, std::map<std::string, int>> outputs, inputs;
    // Each good's recipes with their counts (I5): what goes in for how many come out.
    struct Recipe
    {
        std::vector<std::pair<std::string, int>> in;
        int out = 1;
    };
    std::map<std::string, std::vector<Recipe>> makes;
};
const Links& links();
// The ripple's targets from `good` (7.4): `downstream` its outputs, else its inputs; two steps at most, each keeping at
// most RippleKeep goods (by recipes using the link, then the cheaper first), a visited set so cycles end; each with its
// step (1 or 2).
std::vector<std::pair<std::string, int>> rippleTargets(const std::string& good, bool downstream);
} // namespace ratw::market
