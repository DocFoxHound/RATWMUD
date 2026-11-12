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
// The Board's big moves (doc 79, 12: the year log's "big ripples"; Phase 9's last repair): a staged move of a good's
// pressure (an event, a ship, a storm, a wagon, a harvest, a raid: Society::setPressure) by more than BigMove is kept
// BigMoveDays with its cause, MovesKept at most. A single trade or ripple never moves that far (0.15 at most).
inline constexpr double BigMove = .2, BigMoveDays = 28;
inline constexpr std::size_t MovesKept = 200;
inline constexpr double DawnHour = 6;                 // The market day turns at dawn (budgets, allowances, ripples).
inline constexpr int SliderStepADay = 10;             // The slider the Board reads moves at most this a day (D17).
inline constexpr double SettingOutDays = 15. / 1440;   // A keeper "setting out the wares" at the restock: 15 game minutes.

// The seasons an event or a cause can happen in (market.json's `seasons`: "spring", "summer", "autumn", "winter"), a bit
// each; seasonBit(days) is the one the calendar is in; seasonsOf a list's (none, or none known: all four).
inline constexpr int SeasonsAll = 15;
int seasonBit(double calendarDays);
int seasonsOf(const json::Value& list);

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
    // The port's goods (`wharf`; 8.3, Phase 5): what ships bring (less `never`: the goods no ship brings) and take, and
    // the ships' names. Without the file: none, and the names below.
    std::vector<std::string> shipBrings, shipTakes;
    std::vector<std::string> shipNames{"the Gull", "the Saltwife"};
    // (The Phase 5 repair.) `wharf.never` (no ship brings these: they refill each morning as ever); `wharf.posts`, the
    // words in a post's title that make its market a port (lower case; the sea trade's other posts, its carriers and
    // store keepers, are a port's only where one of these is: a town with a "long store" and no wharf is no port); and
    // `wharf.cargoWords`, a good's words in a ship's cargo where the catalogue's name won't do ("hide": "hides").
    std::set<std::string> shipNever;
    std::vector<std::string> wharfPosts{"unloading ships", "working the cranes", "harbourmaster", "wharf office"};
    std::map<std::string, std::string> cargoWords;
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
    // Street life (doc 79, 6.5; Phase 6): the goods the rich buy (`luxuries`), and the services a household has done about
    // once every `every` days at a business of their kind (`services`; `for: patrons`: a patron's only, the rich here).
    std::vector<std::string> luxuries;
    struct Service
    {
        std::string id, business;
        int every = 30;
        bool patrons = false;
        bool poor = false;                          // Done for the poor too (6.5: "a barber rarely"): `poor: true`.
    };
    std::vector<Service> services;
    // Staged events (doc 79, 8.4; Phase 7): market.json's `events` list, each kind a market may see, and the slider's
    // causes (8.5) by direction. Without the file: the few below. A host's entry is checked as the loader takes it
    // (eventKindsOf, causesOf): a bad one is left out, the rest kept.
    struct EventKind
    {
        std::string id;                              // [a-z0-9_], 32 at most.
        std::string words, notice, cry, sign = "the crier";   // Its cause words; its notice and crier's line ("" none).
        std::string noticeOn;                        // Its notice once begun, where `notice` is posted ahead ("" the same).
        std::string noticeAfter;                     // Its notice once its ship has sailed, while it is still on ("": kept).
        int seasons = SeasonsAll;                    // The seasons it can happen in (a bit each: seasonBit).
        std::vector<std::pair<std::string, double>> goods;    // Base good -> the pressure it pushes toward (±0.3).
        double yields = 0, brings = 0;               // The pressure for its cart's goods (a farm's), or its ship's.
        int days = 2, ahead = 0;                     // Days it presses; days posted ahead. (No Times points: an event
                                                     // moves only its goods' prices and what people say; decided 2026-11-12.)
        double hour = 9, weight = 1;
        std::string needs;                           // "" any market; "city"; "port"; "cart"; "festival".
        std::string ship;                            // "" none; "docks" (it comes in with a ship); "lost" (a ship lost).
        bool refill = false, crowd = false, holdsCart = false;
    };
    std::vector<EventKind> events;
    double eventsPerWeek = 2;                        // A market's in a week (1 to 3 at 2), x the rule eventRate.
    struct Cause
    {
        std::string id, words, notice, cry;
        int seasons = SeasonsAll;                    // The seasons it can be picked in ("the hard winter": autumn, winter).
    };
    std::vector<Cause> causesDown, causesUp;
    // The words for a cause the Dungeon Master writes ({cause}, {Cause}: the words, the second capitalised).
    std::string causeNoticeDown = "Hard times ahead: {cause}.", causeNoticeUp = "Better times ahead: {cause}.";
    std::string causeCryDown = "Hear ye! {Cause}! Hard times ahead!", causeCryUp = "Hear ye! {Cause}! Better times ahead!";
    const EventKind* eventKind(const std::string& id) const;
};
const Data& data();
// A world's market.json (doc 79, 12; Phase 9: Society::market): `base` with a host's override over it, only its events
// (perWeek 0 to 3, the list, the causes) and the ships' names (wharf.ships), each checked as the loader takes them (a
// listed id a string names one of the base's own kinds; an object is a host's own, eventKindsOf's); and, when `never` is
// given, the goods no ship brings (known base goods only) in place of wharf.never. Anything else, or a part that won't do,
// is skipped with a line in `warnings`.
Data layered(const Data& base, const json::Value& over, const std::vector<std::string>* never, std::vector<std::string>* warnings);
// A host's staged events and causes as the loader takes them (8.4, 8.5): a kind with an id of [a-z0-9_] (32 at most,
// once), plain words (no line breaks; words 80, a notice 280, a cry 200 at most), at least one known good (its pressure
// within ±0.3) or a cart's or ship's goods, known needs; none with the forbidden word: its id held to the name rule
// (forbiddenInName: the letters anywhere), its words to the whole-word rule (forbiddenWord). 64 kinds at most.
std::vector<Data::EventKind> eventKindsOf(const std::vector<json::Value>& list);
std::vector<Data::Cause> causesOf(const std::vector<json::Value>& list);
// "{key}" in `text` filled with its value, and "{Key}" with it capitalised (an event's {market}, {Market}; a cause's
// {cause}, {Cause}).
std::string fillWords(std::string text, const std::map<std::string, std::string>& with);
// Plain words a host or the Dungeon Master wrote (an event's or a cause's): no control character, `most` bytes at most,
// and never the forbidden word (whole word).
bool plainWords(const std::string& text, std::size_t most);
// The forbidden-word check for world text made from data (a ship's name, a cargo's words; the user's rule, as
// tools/test_realm2.py's for the world's own): true if `text` has the whole word "dog" or "dogs", in any case.
bool forbiddenWord(const std::string& text);
// A name is held to more (a ship's: Phases 4 and 5 merged): true if `text` has those letters anywhere in it, in any
// case, so a compound made with the word is out too, and some honest names with it.
bool forbiddenInName(const std::string& text);
// The ships' names a list gives (market.json's `wharf.ships`, a host's own), as the loader takes them: plain names
// (letters, spaces, ' and -; 40 characters at most), each once, 64 at most, and none with the forbidden word in it.
std::vector<std::string> shipNamesOf(const std::vector<json::Value>& list);
// A ship's name as the loader would take it (shipNamesOf's rule, for one name: a saved ship's is held to it too).
bool shipNameFits(const std::string& name);
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
// Only its crafts' outputs (doc 79, 7.7's "goods made in the market": a workshop's; a shop that supplies a good doesn't
// make it, so a wagon or cart brings it: Phase 4's imports).
void craftedBy(const std::string& business, std::set<std::string>& crafted);
// A bought lot of `quantity` pieces whose ladder summed to `sum` (catalogue `each` a piece): rounded (at a stall × 0.9
// and rounded down), at least 1p, and never under the clamp floor's stall price, ceil(each × 0.80 × 0.9 × quantity)
// (I3, 7.2). The cheapest way to buy a lot is at a stall.
std::int64_t boughtLot(double sum, double each, int quantity, bool stall);
// I3's backstop's lots (7.6), for 1..upTo pieces (the rest 0): the least that many cost bought at a stall at `factor`
// in any split into lots (a lot rounded, so smaller lots may cost less than one). Each lot of k worked out once, then
// the least of two halves' in whole pennies: O(upTo²) additions, upTo boughtLots.
std::array<std::int64_t, 100> leastSplits(double each, double factor, int upTo);
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
// A committed departure (doc 79, 8.1 and 8.2): a wagon posted on both ends' boards two days ahead, with its hour, size,
// driver and manifest (only an announced cause may change it after), or a farm's cart rolled at the morning's turn. Kept
// until it sets out.
struct Departure
{
    std::string id, kind = "wagon";                 // "wagon" (between markets, posted) or "cart" (a farm's or works').
    std::string from, to;                           // Markets (a cart's `from` is its farm's id).
    std::string driver;                             // The carter it names (a wagon's: another of the yard's, if he can't).
    std::int64_t day = 0;
    double hour = 8;
    int size = 1;                                   // 1..3.
    std::vector<std::pair<std::string, int>> manifest;   // Base good -> pieces (for show: staged goods).
    std::string cause;                              // Why it sets out late ("" none): announced.
    double posted = 0;
};
// Something on the roads or the sea that bears on a market's Times (5.3): a wagon to it raided ("raid") or late ("late"),
// a ship of its port stormbound ("storm") or lost at sea ("lost"); each but a raid counts as a late wagon does.
struct Trouble
{
    double day = 0;
    std::string market, kind;
};
// What an arriving wagon brought (8.1: `world.news:<source>`): the source market and its top cause, in words.
struct News
{
    std::string from, words;
    double day = 0;
};
// The DM's override of an area's harvest (8.2): poor (0), fair (1) or good (2), until a game day.
struct HarvestOverride
{
    int word = 1;
    double until = 0;
    std::string cause;                               // Its words, the DM's ("blight at the orchards"; Phase 7).
    bool told = true;                                // Its notice posted (World::overrideHarvest's is not yet: tendCauses posts it).
};
// A committed ship (doc 79, 8.3; Phase 5): one line of a port's timetable, rolled a week ahead and saved, so a restart
// shows the same ships. Its goods are for show: what its notice names (`brings` of the goods its port waits for ships
// for, World::Port::imports; `takes` from market.json's `wharf.takes`). "due" until its hour on its day, "in" until it
// sails that evening, then "sailed" (kept through its week's end); "storm" if a storm kept it out (kept three days).
struct Ship
{
    std::string id;                                 // "s<N>" (Board::nextShip).
    std::string port;                               // The port's market.
    std::string name;                               // "the Gull".
    std::int64_t day = 0;
    double hour = 8, sails = 18;                    // When it docks and when it sails, that day.
    std::vector<std::pair<std::string, int>> brings, takes;   // Base good -> pieces.
    std::string status = "due";
    double posted = 0;                              // The day it was posted (whole: the same world, the same timetable).
    bool lost = false;                              // "storm" and lost at sea: a staged event's story (Phase 7, 8.4).
};
// A staged event (doc 79, 8.4; Phase 7): one of market.json's `events` at a market, rolled a week ahead by a seeded
// scheduler (or added by the Dungeon Master) and saved on the Board, so a restart shows the same events. "due" until its
// day and hour (a ship's: until its ship docks, or is lost), "on" while it presses its goods toward their targets (its
// days), then "over" (kept two days for talk), or "cancelled" (the DM's, or its ship never came). Its words are filled
// when it is rolled, so a host's later edit of the list changes no event already committed.
struct StagedEvent
{
    std::string id;                                 // "e<N>" (Board::nextEvent).
    std::string market, kind;
    std::int64_t day = 0;
    double hour = 9;
    int days = 2, ahead = 0;                        // Days it presses; days its notice is up before.
    bool crowd = false;                             // A crowd at its market's place (or its port's Wharf) while on.
    std::vector<std::pair<std::string, double>> goods;   // Base good -> the pressure it pushes toward.
    std::string words, notice, cry;                 // Its cause words ("the wedding up at the Heights"), notice, cry.
    std::string noticeOn;                           // Its notice once begun (a rich cargo's "is in"), while it is due.
    std::string noticeAfter;                        // Its notice once its ship has sailed ("has sailed"), till she has.
    std::string subject;                            // "ship:<id>" (it comes in with it), "lost:<id>" (the ship it loses),
                                                    // "farm:<id>" (the farm whose cart it holds); "" none.
    std::string status = "due";
    std::string by = "seeded";                      // "seeded" or "dm".
    bool announced = false;                         // Its notice is up (posted ahead, or as it began).
    double posted = 0;
};
// The slider's cause (8.5): the words a move of the slider came with (the DM's, else the seeded list's for its
// direction), its notice and crier's line, its direction, the day it was given and its number, and whether the markets
// have been told (their notices pinned).
struct SliderCause
{
    std::string words, notice, cry;
    int dir = 0;
    double day = -1;
    std::int64_t n = 0;
    bool told = true, dm = false;
};
// The DM's Times nudge for a market (12: "boom at the port"), with its words and expiry, and whether it was posted.
struct TimesNudge
{
    int points = 0;
    double until = 0;
    std::string cause;
    bool told = true;
};
// The DM's "reset prices" at a market (doc 79, 12; Phase 9: Society::resetPrices): every pressure there eased linearly to
// 0 from `from` to `until` (a game day), the roads' pass by the roads' pass (`last`: the clock at the last one), with its
// words; erased once it is over.
struct PriceReset
{
    double from = 0, until = 0, last = 0;
    std::string words;
};
inline constexpr std::size_t TroublesKept = 64, CommittedMost = 256;
// (A port's timetable: two weeks of at most a ship a day, and the few just sailed or stormbound. The Board's: 16 ports' worth.)
inline constexpr std::size_t ShipsAPort = 16, ShipsMost = 256;
inline constexpr std::int64_t BroughtInDays = 60;
// (A market's events: two weeks of at most one a day, and those just over. The Board's: 16 markets' worth.)
inline constexpr std::size_t EventsAMarket = 16, EventsMost = 256;
inline constexpr std::size_t EventGoodsMost = 4;     // An event's goods, at most.
// (A ship lost at sea keeps her name off the timetables a year: none of that name sails again soon. 64 kept at most.)
inline constexpr double LostShipDays = 365;
inline constexpr std::size_t LostShipsKept = 64;
inline constexpr double EventTargetMost = .3;        // An event's pressure, either way (inside the clamp, 7.6).
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
    std::deque<Ripple> moves;                        // The big staged moves of the last BigMoveDays (BigMove; setPressure).
    // Staged trade (doc 79, Phase 4; 8.1-8.2, 7.7): the departures posted or rolled and not yet gone, the last day the
    // routes were rolled for, the next departure's number; each till's back room of goods its market doesn't make (D27:
    // filled on arrivals, a fifth less each morning; absent: never stocked yet, filled as by an arrival); the DM's harvest
    // overrides; the roads' troubles of the last week; each market's Times shift from its harvests and troubles (set each
    // morning by the world); and each market's last news from an arriving wagon.
    std::vector<Departure> committed;
    std::int64_t rolledThrough = -1, nextDeparture = 1;
    std::map<std::string, std::map<std::string, int>> backRoom;   // Till -> base good -> pieces.
    std::map<std::string, HarvestOverride> harvestOverride;       // Area -> override.
    std::vector<Trouble> troubles;
    std::map<std::string, int> timesShift;                        // Market -> points added to its Times score.
    std::map<std::string, News> news;                             // Market -> the last wagon's news.
    // Market -> base good -> the market day a wagon or cart last brought it (T14: a wagon's manifest takes the far end's
    // thin imports brought longest ago first, so each comes in rotation); kept BroughtInDays.
    std::map<std::string, std::map<std::string, std::int64_t>> broughtIn;
    // The ports' timetables (8.3; Phase 5): the ships committed (this week's and the next's), each port's last week rolled
    // and the next ship's number. A world with no port keeps none of it.
    std::vector<Ship> ships;
    std::map<std::string, std::int64_t> shipsThrough;
    std::int64_t nextShip = 1;
    // Staged events and causes (8.4, 8.5; Phase 7): the events committed (this week's and the next's, and those just
    // over), each market's last week rolled and the next event's number; the slider's cause; the DM's Times nudges; and
    // each market's harvest part of its Times shift (the roads' morning sets both), for the cause's words.
    std::vector<StagedEvent> events;
    std::map<std::string, std::int64_t> eventsThrough;
    std::int64_t nextEvent = 1;
    SliderCause sliderCause;
    std::map<std::string, TimesNudge> nudges;
    std::map<std::string, int> harvestShift;
    // Ships lost at sea (Phase 7's repair): name -> the day she was lost; kept LostShipDays, so no ship of hers sails.
    std::map<std::string, double> lostShips;
    // The DM's price resets under way (Phase 9): market -> its reset.
    std::map<std::string, PriceReset> resets;
    bool empty() const
    {
        return slider == 0 && sliderTarget == 0 && sliderDay < 0 && cell.empty() && day < 0 && budgetLeft.empty() && takenToday.empty() &&
               pushed.empty() && netToday.empty() && soldToday.empty() && week < 0 && discountWeek.empty() &&
               restocked.empty() && settingOut.empty() && recent.empty() && moves.empty() && committed.empty() && rolledThrough < 0 &&
               nextDeparture == 1 && backRoom.empty() && harvestOverride.empty() && troubles.empty() && timesShift.empty() &&
               news.empty() && broughtIn.empty() && ships.empty() && shipsThrough.empty() && nextShip == 1 && events.empty() &&
               eventsThrough.empty() && nextEvent == 1 && sliderCause.n == 0 && sliderCause.words.empty() && nudges.empty() &&
               harvestShift.empty() && lostShips.empty() && resets.empty();
    }
};
json::Value encode(const Board& board);
// Lenient (doc 79, 5.2 and 14): anything malformed is dropped, numbers clamped, never a refusal.
Board decode(const json::Value& value);
// A saved timetable made one the game could have rolled (Phases 4 and 5 merged; decode's, and a restored state's:
// World::dropOrphanedShips), whatever a save says: one ship of an id and one a port a day (the first kept); a status the
// game knows, else due; hours inside the day; a name the loader would take (shipNameFits: else another of
// market.json's not at that port, else "a ship"); a cargo of goods, each once, 1 to 999 pieces, 5 at most, and none
// brought that no ship brings (`wharf.never`); ShipsAPort a port (the oldest sailed or stormbound go first, then the
// last still due), ShipsMost in all; and the next ship's id above every "s<N>" there, so none is given twice.
void mendShips(Board& board);
// A saved list of events made one the game could have rolled (decode's, and a restored state's): one event of an id; a
// status the game knows, else due; numbers in their ranges (hour, days 1 to 7, each good's pressure within
// EventTargetMost, six goods at most, known goods once each); words, notice and cry plain words (else the event is
// dropped: a lying save's forbidden word never reaches a board); EventsAMarket a market (the oldest over or cancelled go
// first), EventsMost in all; and the next event's id above every "e<N>" there.
void mendEvents(Board& board);

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
    std::set<std::string> bought;                    // Base goods its shops buy from players (a wagon may bring those too).
    std::set<std::string> crafted;                   // Base goods its workshops craft and its areas bring in (7.7: made here,
                                                     // refilled each morning; not what its shops only supply).
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
