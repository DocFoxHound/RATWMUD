#pragma once
// The economy orchestrator (Docs/Design/46-economy-orchestrator.md). Once a game day the society hands it a snapshot of the
// land (purses, food, work, prices, what each holder spent), and it works out a brief on a thread of its own: how hurt each
// town is and how, which holders (tills, farms, great houses, the church, the treasuries) are over their band and what they
// must spend, where that money should go and through which channels, and the prices, margin and wage floor it would set.
// It measures every day and acts once a week (the user, 2026-10-06): the evening of the weekly reckoning, after the taxes
// and tithes, it weighs the whole week's measures and decides (the pot, its orders, prices, the margin).
// Phase 1 (the shadow orchestrator) computes the brief and keeps it, and nothing is applied: the game runs as before.
//
// plan() is a pure function of the snapshot and the orchestrator's memory, so a run repeats exactly however many
// threads there are and whenever the thread finishes: the society waits for the brief when the day turns.
#include <condition_variable>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace ratw::orchestra
{
// A Dungeon Master's steer (doc 46, Part 10): what the orchestrator weighs, never what it does.
//   "pressure": the whole land's bands squeezed `strength` (0.5 gentle .. 3 hard);
//   "town":     a town's distress weighed by `strength` (0 .. 3);
//   "holder":   an account treated as holding `strength` times what it holds (1.5 .. 4), or spared (0: it may hoard);
//   "channel":  a channel weighed by `strength` (0 closes it .. 3);
//   "price":    `item` in a town (or "*", every town) dearer or cheaper by `strength` (0.5 .. 3).
struct Steer
{
    std::string id, kind, target, item, note, by;
    double strength = 1;
    std::int64_t from = 0, until = 0;               // Game days: in force from `from` until (not including) `until`.
};
bool validSteer(const Steer& steer, std::string& problem);
// A test's scripted steers (econ_watch --steer FILE): a JSON array of {"day", "kind", "target", "item", "strength", "days",
// "note"}, each set on that game day (in force from the next day's plan).
struct ScriptedSteer
{
    std::int64_t day = 0;
    Steer steer;
    int days = 7;
};
bool readSteerScript(const std::string& text, std::vector<ScriptedSteer>& out, std::string& problem);

enum class HolderKind
{
    Till,                                            // A house-owned business's till ("till:<position>").
    Keeper,                                          // An owner-run shop: its keeper's own purse (no till of its own yet).
    Producer,                                        // A farm or other producer: its worker's own purse (no till yet).
    House,
    Church,
    Treasury,                                        // A town's ("stores:<town>").
    Capital,                                         // The capital's treasury ("treasury").
    Buyer,                                           // A town's buyer ("town:<town>:works", the watch, the docks...).
    Bank,                                            // A town's bank ("bank:<town>": money that stops): its floor, its reserve.
};
const char* kindName(HolderKind kind);

struct HolderSnap
{
    std::string id, town;
    HolderKind kind = HolderKind::Till;
    std::int64_t cash = 0;
    std::int64_t spent = 0;                          // Its ordinary outgoings since the last snapshot (not orders, not
                                                     // what it passes to its own house).
    std::int64_t floor = 0;                          // The least its need may be (by kind and size).
};
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
struct GoodSnap
{
    std::string town, item;
    int stock = 0;                                   // In the town's shops.
    int kept = 0;                                    // What its shops mean to keep of it, together.
    double rate = 0;                                 // Sold a day (missed sales counting), all its shops together.
    double price = 0;                                // The going price now (the shops' mean), and the catalogue's.
    std::int64_t catalog = 0;
    int nourish = 0;                                 // What a piece feeds (0: not food).
    bool staple = false;                             // Plain food or firewood: kept affordable (doc 46, Part 4).
};
struct TownSnap
{
    std::string id;
    std::int64_t inflow = 0, outflow = 0;            // Coins into the town from elsewhere since the last snapshot, and out.
    int oddJobs = 0, oddJobSlots = 0;                // Odd jobs and hires posted, and how many hands they take.
    std::map<std::string, int> unfilled;             // Kind of post (wageKinds) -> places going begging: vacant posts,
                                                     // hires and odd jobs nobody took.
    int unpaid = 0, supported = 0;                   // Its workers owed wages; and paid today by wage support (Phase 6).
    std::int64_t rescueNeed = 0;                     // What its failing businesses lack of their floats (under half of it).
    std::int64_t granary = 0;                        // Food (nourishment) in its granary (Phase 7).
    std::map<std::string, std::int64_t> funds;       // Channel -> what its fund holds.
};

// The orchestrator's dials (Data/Economy/orchestrator.json; doc 46). All placeholders for the balance pass.
struct Dials
{
    std::string mode = "shadow";                     // "shadow" (plan, apply nothing), "on" (later phases) or "off".
    double nourishADay = 50;                         // A grown wolf's food a day, in nourishment.
    double needDays = 14;                            // A holder's need: this many days of its ordinary outgoings.
    double comfortable = 2, cap = 4;                 // The band's top, and the cap, in needs.
    double overShare = .40, overShareAtCap = .90;    // Of what is over the top, spent a week: just over, and near the cap.
    double capShareOfLand = .02;                     // Nobody holds over this share of the land's money.
    // Growth (doc 46, Phase 5): a holder above its floor that gained over the week sends this share of the week's gain out
    // at the decision, more as the land is in distress or pressed: what comes in with trade must go back out, not pool.
    double gainShare = .5;
    // And a holder that keeps growing week after week sends growthStep more of its gain for each week of it past the first
    // (doc 46, the long run's fixes): half, then 0.75, then all of it.
    double growthStep = .5;
    // Jobs before raises (the long run's fixes): a town where more than wageIdleLimit of those able to work earned nothing
    // lately raises no pay for its payers' gains, eases raised pay back, and weighs its hires and works double.
    double wageIdleLimit = .10;
    // A channel is sent at most one and a half times what it paid out last week, or channelFloor (land-wide) for one new or
    // idle, less what its funds hold (the long run's fixes).
    double channelFloor = 3000;
    // Each town's price level, moved against its shops' week's gain (the long run's fixes), within these.
    double priceLiftLeast = .6, priceLiftMost = 1.4;
    // Money that stops (doc 46): each treasury's tax level rises taxRaise times while it holds under its need, eases
    // taxEase times while over its comfortable band, between taxLeast and taxMost; it multiplies what its town takes.
    double taxRaise = 1.15, taxEase = .9, taxLeast = .5, taxMost = 3;
    // The town levy: levyShare a week of what a resident holds above levyLine wealth lines (times its tax level). A hand
    // about the home over helpLine of them; savings put in the bank above depositLine.
    double levyShare = .02, levyLine = 2, helpLine = 1.5, depositLine = 1;
    // The playbook (doc 46): a town under foodDaysLow days of food a head sends its hires to its food first; the ladder
    // steps up after ladderWeeks decisions at its most pressure with the shares still falling; the bank's terms (the
    // share of a saver's money above its line it takes a week) move between depositShareLeast and depositShareMost.
    double foodDaysLow = 2, ladderWeeks = 3, depositShareLeast = .05, depositShareMost = .5;
    // Days of food a post's payer gives in kind each day (the watch's mess): off its living floor.
    std::map<std::string, double> board = {{"guard", 1}};
    // Its own pressure (doc 46, Phase 5): at each decision, if the residents' share of the land's money fell over the week
    // by more than shareSlip, it presses autoRaise times harder (to autoMost); if it rose, it eases by autoEase (to 1).
    double shareSlip = .005, autoRaise = 1.3, autoEase = .85, autoMost = 3;
    // And the living floor (Phase 6): if the poorer half's share of the residents' money fell, it rises floorRaise times
    // (to floorMost); if it rose, it eases (autoEase, to 1).
    double floorRaise = 1.1, floorMost = 2;
    // Learning reach (Phase 7): each channel's weight moves reachStep of the way toward its reach against a fair half (the
    // poorer half's share of the people), between reachLeast and reachMost. A channel paid less than reachMinimum in the
    // week isn't judged.
    double reachStep = .25, reachLeast = .5, reachMost = 2, reachMinimum = 50;
    // The granary (Phase 7): in summer and autumn each town stores granaryDays of food a head, of what keeps.
    double granaryDays = 10;
    double distressComfortable = 1.5;                // The band's top at full distress (it falls toward this).
    double distressSpendBoost = 2;                   // And the share spent above it, times this at full distress.
    // (overShare and overShareAtCap are of a week: what a decision sends out over the week that follows it.)
    double landShare = .25;                          // Of the pot, shared by people alone, not distress.
    double channelMostShare = .5;                    // No channel takes more than this of a town's share.
    // Wages (Part 5; doc 46, Phase 4): each town's table of a day's pay by kind of post, starting from these, never under
    // the living floor (a day's food at the town's prices times wageFloorOverFood, and lodging; a quarter of it for an odd
    // job's share), moved at each week's decision: up wageRaise where posts of the kind go unfilled, down wageEase where
    // many are idle and none go begging, to at most wageMost times its start.
    double wageFloorOverFood = 2, lodgingADay = 0, wageRaise = .10, wageEase = .05, wageMost = 3;
    std::map<std::string, double> wageStart = {{"help", 16}, {"guard", 14}, {"labour", 12}, {"clergy", 16},
                                               {"keeper", 8},  {"hand", 12},  {"odd job", 4}};
    double stapleIncomeShare = .5;                   // A day's plain food costs at most this of a lowest-quarter earner's day.
    double priceMove = .25;                          // The most a price may move at a decision (a week).
    double priceLow = .6, priceHigh = 1.6;           // Against the catalogue.
    double marginStart = .55, marginLow = .4, marginHigh = .75, marginMove = .04;   // (Move: a week's.)
    double smoothDays = 3;                           // Distress smoothed over this many days.
    // Each sensor's weight in a town's distress.
    // Hardship (hungry, starving, short, poor) weighs most; the signs of trouble coming (idle, bare shelves, failing trade,
    // money draining away) less.
    double wHungry = 3, wStarving = 6, wShort = 1, wPoor = 1, wIdle = .5, wShelves = .25, wTrade = .25, wDrain = .25;
    // Floors of need, by kind: pennies a resident of the town (the church: of the land), or flat.
    double floorTownHead = 30, floorChurchHead = 10, floorHouse = 100, floorTill = 60, floorKeeper = 40, floorProducer = 40,
           floorBuyer = 40;
};
// Reads the dials from JSON text (missing fields keep their defaults); false with a problem if it is malformed.
bool readDialsText(const std::string& text, Dials& dials, std::string& problem);

struct Snapshot
{
    std::int64_t day = 0;                            // The game day the brief is for (the day after the snapshot's).
    int season = 0;
    // Whole days the society has counted spending and earnings for (0 at its first snapshot: a new or restored world).
    // Bands wait for a day of it, and idleness for three.
    int counted = 0;
    bool decide = false;                             // The week's decisions are due (the evening of the reckoning).
    std::int64_t moneySupply = 0;
    std::vector<HolderSnap> holders;
    std::vector<ResidentSnap> residents;
    std::vector<HomeSnap> homes;
    std::vector<ShopSnap> shops;
    std::vector<GoodSnap> goods;
    std::vector<TownSnap> towns;
    std::vector<Steer> steers;
    Dials dials;
    // On a decision's day (Phase 7): each channel's week, what it paid and how much of it reached the poorer half (paid
    // to them, or paid to a till in the share of its outgoings that went to them).
    std::map<std::string, std::pair<double, double>> reach;   // Channel -> reached, paid.
    // The banks (money that stops, doc 46): town -> its coins and its savers' savings.
    std::map<std::string, std::pair<std::int64_t, std::int64_t>> banks;
};

// What the orchestrator remembers from day to day (saved with the society).
struct Memory
{
    std::map<std::string, double> spent;             // Holder -> a slow average of its ordinary outgoings a day.
    std::map<std::string, double> distress;          // Town -> its distress, smoothed.
    std::map<std::string, double> price;             // "town|item" -> the price it last set (would set, in shadow).
    std::map<std::string, double> net;               // Town -> coins in less out a day, from other towns (a week's average).
    std::map<std::string, double> wage;              // "town|kind" -> a day's pay for that kind of post (Phase 4).
    double margin = -1;                              // -1: not yet set.
    std::int64_t day = -1;                           // The day of its last plan.
    // The week's measures since the last decision: town -> the sum of its days' distress, and how many; "town|kind" -> the
    // sum of each kind of trouble's score.
    std::map<std::string, std::pair<double, int>> week;
    std::map<std::string, double> weekKinds;
    std::int64_t decided = -1;                       // The day of its last decision.
    std::map<std::string, std::int64_t> weekStart;   // Holder -> what it held after the last decision (for its week's gain).
    std::map<std::string, int> growing;              // Holder -> the weeks in a row it has gained.
    std::map<std::string, double> priceLift;         // Town -> its price level, against its shops' books (1 to start).
    std::map<std::string, double> taxLevel;          // Treasury -> its tax level, against its band (1 to start).
    // The playbook: its rung on the ladder (0 to 3), the decisions at its most pressure with the shares falling, and those
    // with neither falling; the bank's terms.
    int ladder = 0, ladderWeeks = 0, calmWeeks = 0;
    double depositShare = .25;
    double residentShare = -1;                       // The residents' share of the land's money at the last decision.
    double autoPressure = 1;                         // Its own pressure, from how that share moves.
    double bottomShare = -1, floorLift = 1;          // The poorer half's share of it; the living floor's lift (Phase 6).
    std::map<std::string, double> support;           // "town|item" -> price support: a staple's gap under its price (Phase 6).
    std::map<std::string, double> learned;           // Channel -> its learned weight, from its reach (Phase 7; 1 to start).
};

struct TownReading
{
    std::string id;
    int people = 0, idle = 0;
    double distress = 0, raw = 0;                    // Smoothed and today's.
    double week = -1;                                // The week's mean (on a decision's day; -1 otherwise).
    std::string kind;                                // "" (well), "empty shelves", "empty purses", "no work", "failing trade", "draining".
    double foodCost = 0;                             // A day's plain food, pennies.
    double hungry = 0, starving = 0, short_ = 0, poor = 0;   // Shares of its people (short: of its households).
    double shopFoodDays = 0, takingsRatio = 0;
    std::int64_t netInflow = 0, wageFloor = 0, share = 0;
};
struct HolderBand
{
    std::string id, town, band;                      // band: "lean", "comfortable", "over" or "cap".
    HolderKind kind = HolderKind::Till;
    std::int64_t cash = 0, need = 0, toSpend = 0;
    std::int64_t gain = 0;                           // What it gained since the last decision (on a decision's day).
};
struct Order
{
    std::string from, town, channel;
    std::int64_t coins = 0;
};
// What a bank's savings the channels can't take, given to one who pays wages and is short (money that stops, doc 46).
struct Grant
{
    std::string from, to;
    std::int64_t coins = 0;
};
// One thing it found and did at a decision (the week's report): a town ("" for the land), the tool, and what.
struct Action
{
    std::string town, tool, detail;
};
struct PriceSet
{
    std::string town, item;
    std::int64_t catalog = 0;
    double now = 0, would = 0;
    double support = 0;                              // Price support a piece (Phase 6), when it holds a staple under its price.
};
struct Brief
{
    std::int64_t day = -1;
    std::string mode;
    bool decided = false;                            // The week's decisions: the pot, orders, prices and margin (else 0).
    double landDistress = 0, margin = 0, gini = 0;
    double residentShare = 0, autoPressure = 1;      // The residents' share of the land's money; its own pressure.
    double bottomShare = 0, floorLift = 1;           // The poorer half's share (by household, a head); the floor's lift.
    std::int64_t moneySupply = 0, pot = 0, median = 0;
    std::vector<TownReading> towns;
    std::vector<HolderBand> holders;                 // Every holder (the saved brief keeps only those over their band).
    std::map<std::string, int> bands;                // Band -> how many holders are in it.
    std::vector<Order> orders;
    std::map<std::string, std::int64_t> channels;    // Channel -> what the day's orders send through it.
    std::vector<PriceSet> prices;                    // Every price it would set.
    std::map<std::string, std::map<std::string, double>> wages;   // Town -> kind -> a day's pay (the table in force).
    std::vector<Steer> steers;                       // The steers it weighed.
    std::map<std::string, double> reach;             // Channel -> its reach this week (on a decision's day).
    std::map<std::string, double> learned;           // Channel -> its learned weight.
    std::map<std::string, double> taxLevels;         // Treasury -> its tax level (in force).
    std::vector<Grant> grants;                       // The grants from what the channels couldn't take (a decision's).
    std::vector<Action> actions;                     // The week's report (a decision's).
    int ladder = 0;                                  // Its rung on the ladder.
    double depositShare = .25;                       // The banks' terms.
    // Town -> its bank's coins and its savers' savings (the day's measure).
    std::map<std::string, std::pair<std::int64_t, std::int64_t>> banks;
};

// The channels (doc 46, Part 7), in a fixed order.
const std::vector<std::string>& channelNames();
// Whether a channel is built and spends (doc 46: Phase 5 the first five; price and wage support, rescue and opening in
// Phase 6). The plan sends nothing down one that isn't.
bool channelLive(const std::string& channel);
// The kinds of post the wage table pays (doc 46, Part 5): shop and house help, the watch, the town's labour, the clergy,
// a business's keeper, a hired hand, and a hand's share of an odd job.
const std::vector<std::string>& wageKinds();

// The day's plan: pure, given the same snapshot and memory, the same brief and memory after.
Brief plan(const Snapshot& snapshot, Memory& memory);

// The orchestrator's saved state (SocietyState::orchestrator): its memory, the steers in force, and its last brief in
// short (what the Dungeon Master's Money tab shows).
struct State
{
    Memory memory;
    std::vector<Steer> steers;
    std::int64_t nextSteer = 1;
    Brief last;                                      // The latest day's measures.
    Brief decision;                                  // The latest week's decisions.
    // The banks' books (money that stops, doc 46), kept by the society (not the plan's memory, which its thread
    // replaces): saver -> its town's bank and what it has in. (A bank keeps a reserve and the orchestrator sends the rest
    // out: its coins may be less than its savers have in.)
    std::map<std::string, std::pair<std::string, std::int64_t>> deposits;
};
// (Saved as JSON: RatwOrchestratorJson.h.) A brief as JSON text, in full or as saved.
std::string briefText(const Brief& brief, bool full);

// The orchestrator's own thread: one plan at a time. submit() hands it a snapshot and its memory; take() waits for the
// brief (and the memory after). Without a thread (threaded false) take() plans on the caller's thread: the same result.
class Runner
{
  public:
    explicit Runner(bool threaded);
    ~Runner();
    Runner(const Runner&) = delete;
    Runner& operator=(const Runner&) = delete;
    void submit(Snapshot snapshot, Memory memory);
    bool pending() const;
    std::int64_t pendingDay() const;                 // The day the pending plan is for (-1: none).
    std::pair<Brief, Memory> take();

  private:
    void work();
    mutable std::mutex lock_;
    std::condition_variable wake_, done_;
    std::optional<Snapshot> snapshot_;
    Memory memory_;
    std::optional<std::pair<Brief, Memory>> result_;
    std::int64_t pendingDay_ = -1;
    bool busy_ = false, stopping_ = false;
    std::thread thread_;
};
} // namespace ratw::orchestra
