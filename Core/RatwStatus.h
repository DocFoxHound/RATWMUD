#pragma once
// NPC economic status, the bands (Docs/Design/79-a-staged-economy.md, section 6, Phase 2): the table in
// Data/Economy/status.json, read leniently and checked, and the arithmetic of the cut that needs no society. The society
// scores and ranks its households (Society::stagedBands, RatwStagedAccounts.cpp); this says what the slider's shares are,
// how a pool is cut, how often a band goes short, and what kind of home a cell is.
#include "RatwJsonDoc.h"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ratw::status
{
// The four bands (D1: four), the talk library's `wealth` words, poorest first.
enum Band : int
{
    Destitute = 0,
    Poor = 1,
    Comfortable = 2,
    Rich = 3
};
constexpr int Bands = 4;
const char* bandName(int band);                     // "destitute" ... "rich"; "comfortable" out of range.
int bandOf(const std::string& word);                // -1 for none of them.
// No share of a band may be under this percent (doc 79, 6.4: a constant, never a host's setting).
constexpr double MinShare = 2;

// What a band does (doc 79, 6.5): one row of status.json each.
struct Row
{
    double meals = 3;                               // A day's food a head, in prepared meals' worth of nourishment.
    std::vector<std::string> food;                  // What it eats, in order: the first its food shop sells.
    double shortChance = 0;                         // Percent, at slider 0.
    std::int64_t purseLow = 0, purseHigh = 0;       // The purse drawn toward each night (at most 500).
    double budget = 1, theft = 0;                   // A keeper's buying budget times; the weight among thieves.
    std::int64_t pocketLow = 0, pocketHigh = 0;     // What a pickpocket finds.
    int goodsMost = 8;                              // Pieces of one good (not food) a wallet keeps overnight.
    // Kinds of goods (not food, not worn, not a maker's piece) a wallet keeps overnight, the fewest-held going first
    // (doc 79, Phase 11's soak: the household's everyday buys piled up a kind at a time, without end).
    int kindsMost = 12;
    // Street life (doc 79, 6.5 and 10; Phase 6): the pieces a day (a mean) a household buys of its market's everyday wares
    // (those its band can afford: at most its purse's top); the chance a week it buys a luxury; its services' frequency
    // times (by each service's `every`; the poor's only the barber's); and the chance a passing resident of the band
    // gives a beggar a penny.
    double shops = 0, luxury = 0, services = 0, alms = 0;
};
struct SharePoint
{
    double at = 0;                                  // The slider, -100..100.
    std::array<double, Bands> shares{};             // Percent of the pool, poorest first.
};
struct Table
{
    std::vector<SharePoint> shares;                 // By `at`, rising.
    double timesShift = 25, smoothing = 10;
    int hysteresisDays = 7;
    double hysteresisEdge = 2;                      // Percentiles past a band's edge before a household moves.
    double shortHard = 1.5, shortBoom = .5, shortPressureMost = 2, shortPressureAt = .5;
    std::array<Row, Bands> rows;
    // The score (6.3).
    std::map<std::string, double> classScore, homeScore, districtScore, events;
    double noEarner = -1.5, thinEarner = -.5, lot = .5, noise = .3;
    int thinMouths = 4, eventDays = 28;
    std::array<double, 5> times{-.6, -.3, 0, .3, .6};   // By Times: hard, lean, ordinary, good, boom.
    std::map<std::string, std::vector<std::string>> homes;   // "house", "flat", "room" -> words in a home's cell id.
    int holdOneOff = 3, holdSteady = 14, leanDays = 3;
    std::int64_t treasuryFloor = 200, treasuryCeiling = 2000;
    std::map<std::string, std::int64_t> caps;       // The faucets' caps (rewardsADay, theftADay, ...).
    std::map<std::string, std::int64_t> pay;        // A kind of post's pay word, pennies a day at slider 0.
    // Street life (doc 79, 10; Phase 6). Theft (10.1): the expected thefts a day in a market are theftBase times the sum
    // of its households' theft weights times the market's Times (theftTimes, hard .. boom), capped by the rule theftCap;
    // a thief is picked by its band's weight (a rich one with a sly honesty `slyRich`), times its honesty's (theftHonesty)
    // and twice if hungry. The dole (10.2): a day's food a head, in bread, for destitute and poor households at Restday's
    // chapel. Beggars (10.2, the user's decision of 2026-11-11): at most beggarsACell at one place (the chapel steps, the
    // notice board, a shop's door or a stall), places World::BeggarApart (8) tiles or more apart, and at most
    // beggarsInACell in a whole cell, by the slider (straight lines between the points, the market's Times moving it by
    // timesShift as it moves the bands' column; a fraction is one more on that share of the cells, by a lot of the cell
    // and the day): 5 only at the worst, about 0-1 in a boom. Who begs is drawn by need (the household's food want,
    // Society::foodWant) times its class's weight (beggarsClass), each group's next beggar (elder or of working age, by
    // home kind) keeping beggarsSpread of its weight for each one drawn from that group already, and the elders at most
    // beggarsElders of a market's beggars while others are in need. Rough sleepers: at most beggarsACell by one warm
    // wall. (The name beggarsACell is kept from "the most in a cell".)
    // Decided 2026-11-12: over the cell's most, a market's whole count is held to beggarsInAMarket by the slider (the same
    // straight lines and Times shift; the points are the realm's largest market's, a smaller one's its share of that by
    // its households, never under the cell's own most: 12 only at the worst, about 1 in a boom); and on Restday, their day
    // off, the working poor may beg too: one with a post, of a household in want, at beggarsPosted of its weight (below
    // the postless), never on a working day, never one whose post works on Restday, never at its own place of work.
    double theftBase = .03;
    std::array<double, 5> theftTimes{1.5, 1.2, 1, .85, .7};
    std::map<std::string, double> theftHonesty{{"frank", .2}, {"plain", 1}, {"guarded", 1.5}, {"sly", 3}};
    double slyRich = .02, doleDays = 1;
    int beggarsACell = 2;
    struct CellPoint
    {
        double at = 0, most = 0;                    // The slider, -100..100; the most beggars in a cell there.
    };
    std::vector<CellPoint> beggarsInACell{{-100, 5}, {-50, 3.5}, {0, 2}, {50, 1}, {100, .5}};
    std::vector<CellPoint> beggarsInAMarket{{-100, 12}, {-50, 8}, {0, 5}, {50, 2}, {100, 1}};   // (The largest market's.)
    double beggarsPosted = .5;                      // A posted beggar's weight times this (Restday only).
    std::map<std::string, double> beggarsClass{{"lower", 1}, {"middle", .5}, {"upper", .25}};
    double beggarsSpread = .5, beggarsElders = .5;
    std::int64_t cap(const std::string& name) const;   // 0 for one not set.
    double event(const std::string& name) const;       // 0 for one not set.
};
// The table this build ships (the numbers of doc 79, 6.4-6.5), with nothing read.
Table builtIn();
// Data/Economy/status.json (RATW_DATA_DIR's, else the working directory's or one above it, else the source tree's) over
// the built-in table; the built-in table where the file is missing or isn't JSON. Read once.
const Table& defaults();
// A status table read over a base (lenient on read: doc 79, 12): every field checked and clamped, a missing one the
// base's; a share table that sums wrong is normalised, and one that has a share under MinShare, isn't monotone (destitute
// never rising, rich never falling as the slider rises) or doesn't run from -100 to 100 falls back to the base's whole,
// never half-applied. Each fallback or clamp adds a line to `warnings`.
Table read(const json::Value& v, const Table& base, std::vector<std::string>* warnings = nullptr);
// A host's override of status.json (doc 79, 12; Phase 9: live.economy.rules' `status`) made one the game takes: only the
// keys a host may change (HostKeys); a share table whose rows don't each sum to 100, aren't monotone or have a share
// under MinShare dropped (the default's stands, never normalised); each faucet cap held to CapCeiling times the shipped
// default (grantsADay to GrantsMost; welcomeDays, a period, to no less than its default), an unknown cap dropped. Each
// change adds a line to `warnings`. read() takes the rest.
inline constexpr const char* HostKeys[] = {"shares", "rows", "caps", "pay", "street", "score", "hold", "leanDays", "timesShift",
                                           "smoothing", "hysteresis", "shortHard", "shortBoom", "shortPressureMost",
                                           "shortPressureAt"};
inline constexpr double CapCeiling = 5;
inline constexpr std::int64_t GrantsMost = 2500, GrantsADay = 500;
json::Value hostOverride(const json::Value& v, std::vector<std::string>* warnings = nullptr);
// Whether a share table passes the loader's checks (after normalising): why not in `why`.
bool sharesValid(const std::vector<SharePoint>& shares, std::string* why = nullptr);
// The shares at a slider (straight lines between the rows, clamped to -100..100); with Never Broke, destitute's share is
// poor's.
std::array<double, Bands> sharesAt(const Table& t, double slider, bool neverBroke = false);
// A pool of n cut by shares (percent): counts by largest remainder (ties to the poorer band), summing to n.
std::array<int, Bands> cut(const std::array<double, Bands>& shares, int n);
// A band's chance of a short day (0..1) at a slider: its row's, times shortHard at -100 and shortBoom at +100 (straight
// lines between).
double shortChance(const Table& t, int band, double slider);
// The most beggars in a cell (10.2) at a slider, the market's Times (-2 hard .. 2 boom) moving it by timesShift as the
// bands' column moves: beggarsInACell's straight lines, clamped to -100..100; a fraction is left for the caller's lot.
double beggarsInACellAt(const Table& t, double slider, int times = 0);
// The most beggars in a whole market (10.2, decided 2026-11-12), the same way: beggarsInAMarket's straight lines at the
// slider moved by the market's Times, times the market's share of the realm's largest by households (0..1), and never
// under the cell's own most there; a fraction is left for the caller's lot.
double beggarsInAMarketAt(const Table& t, double slider, int times = 0, double share = 1);
// What kind of home a cell is (6.3): "house", "flat" or "room" by the words of its id, else "cottage"; "" for none.
std::string homeKind(const Table& t, const std::string& homeCell);
// The cut (6.3), without a society: each household as the society scored it, and the bands it takes.
struct Candidate
{
    std::string key, market;
    double score = 0;
    bool reason = false;                            // It may be destitute: no earner, a lost post, a death, no home.
    bool recent = false;                            // An event in eventDays: its band may stray from its home's.
    int implied = Comfortable;                      // What its class and home imply (a band at most one away, 6.3).
    int fixed = -1;                                 // A band set outright: a DM's override, the homeless, a House's.
    bool overridden = false;                        // (Fixed by the DM: no clamp, no hysteresis.)
    int atMost = Rich, atLeast = Destitute;         // The Sump's, the Heights' and the estates'.
    bool lifted = false;                            // Lifted by a player's help: outside the pool.
    int previous = -1;                              // Its band before (-1: none), and since when.
    std::int64_t since = 0;
};
struct CutOptions
{
    double slider = 0;                              // The smoothed prosperity.
    std::map<std::string, int> times;               // A market's Times, -2 (hard) .. 2 (boom); missing: ordinary.
    bool neverBroke = false;
    std::int64_t day = 0;
    bool now = false;                               // The DM's "now": no hysteresis.
    bool keepPrevious = false;                      // At restore: a household keeps its saved band.
};
// Every household's band. The pool (no guarantee, override or lift) ranked by score within each market and cut by the
// slider's shares (the market's Times moving its column by timesShift), destitute's places going to poor where there is no
// reason; those outside the pool placed by their score among it; then the guarantees, the band near its home, the
// hysteresis (at most one band a week, only past an edge), and the realm's minimum (one destitute and one rich where
// there are ten households or more, Never Broke allowing).
std::vector<int> assign(const Table& t, const std::vector<Candidate>& households, const CutOptions& o);
// A household's fortune in its score (6.3): a stable lot in [-lot, lot] and a noise in [-noise, noise] by the week, eased
// between weeks (nothing saved, nothing piles up).
double fortune(const Table& t, const std::string& household, std::int64_t day);
// A lot from 0 to 1 fixed by its words, the same on every platform and thread (FNV-1a).
double lot(const std::string& words);
std::uint64_t hash(const std::string& words);
} // namespace ratw::status
