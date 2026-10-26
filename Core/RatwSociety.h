#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "RatwAppearance.h"
#include "RatwOrchestrator.h"

namespace ratw
{
namespace items
{
struct Craft;
struct Item;
struct Producer;
} // namespace items
}

namespace ratw
{
struct EconomyAccount
{
    std::int64_t cash = 0;
    std::map<std::string, int> stock;
};
struct LifeBody
{
    std::string cell;
    double x = 0, y = 0;
    bool companion = false;
    int age = 30;                                   // Their age now (the authored age is when they were written).
    // Out of every player's sight (offstage, World::setTiered): it decides and does its day in steps of
    // Society::UnseenStep seconds, each step a whole step's worth (Docs/Design/31, "Unseen residents").
    bool unseen = false;
};
// The most residents a world may author: the world loader, the society and its saves all hold this many. With
// simulation tiers (World::setTiered) a resident far from every player costs almost nothing per tick.
constexpr std::size_t MaxResidents = 16384;
// Economy accounts: every resident, the treasury, and up to this many player characters.
constexpr std::size_t MaxPlayerAccounts = 8192;
// Facility accounts: towns' stores, caravans, contracts, Chapters' treasuries, and every home's four stores (doc 36).
constexpr std::size_t MaxFacilityAccounts = 32768;
constexpr std::size_t MaxAccounts = MaxResidents + MaxPlayerAccounts + MaxFacilityAccounts + 1;

struct ResidentLife
{
    std::string role, task = "idle", reason;
    double hunger = 20, fatigue = 20, progress = 0;
    std::string goalCell;
    double goalX = 0, goalY = 0;
    int wagesToday = 0;
    std::string homeCell, relocationCell;
    double homeX = 0, homeY = 0, relocationX = 0, relocationY = 0;
    // When it last decided (the calendar day; -1 not yet), so a step is as long as the seconds since. Not saved.
    double decidedAt = -1;
};
struct EconomyEntry
{
    std::int64_t sequence = 0, day = 0, coins = 0;
    std::string kind, from, to, item;
    int quantity = 0;
};
// A player character's ID ("player-..." in development saves, "wolf-<32 hex>"), as opposed to a resident's.
bool playerAccountId(const std::string& id);
// The accounts of things rather than people (Phase 5): a town's stores ("stores:<town>"), a caravan's load
// ("caravan:<id>"), a bandit camp's loot ("bandits:<id>"), a contract's reward held in trust ("contract:<id>").
bool facilityAccount(const std::string& id);
// Goods the economy knows: herbs, meals, the sword, and the catalog's wearables (doc 35).
bool itemValid(const std::string& item);
// The most kinds of goods one account holds (a wardrobe of clothes, a jeweller's counter).
constexpr std::size_t MaxGoodsKinds = 64;

struct Spot
{
    std::string cell;
    double x = 0, y = 0; // Tile-center coordinates.
};
// What the calendar and the sky ask of a community's residents today (Docs/Design/26-living-npcs.md, Phase 9),
// worked out by the world, which knows the places.
struct DayPlan
{
    std::string kind = "work";                      // "work", "market", "rest" or "festival".
    std::string name;                               // The festival's name.
    std::vector<Spot> stalls, crowd;                // Where merchants set up, and where the townsfolk stand.
    bool foul = false;                              // Too foul at the square for stalls or a gathering.
    // Restday's service (doc 42, Phase 6): where the townsfolk sit in the church, and where its preacher stands.
    std::vector<Spot> pews;
    Spot pulpit;
};
// Ground out of town a wolf can make a living from (Docs/Design/42-money-in-circulation.md, Phase 3b): where to stand,
// and the trade: "gathering", "hunting", "woodcutting" or "fishing". The world finds them, by community.
struct WorkGround
{
    std::string trade, ground;                      // ground: doc 41's forage ground ("broadleaf"), or "" for hunting.
    Spot spot;
};
// What a spell of work at a ground brings in, given by the world (from the same finite sources players use).
using Harvest = std::function<std::vector<std::pair<std::string, int>>(const std::string& who, const WorkGround& ground, int season)>;
// A city's markets (Docs/Design/60-the-new-map.md): where a community has more than one, each keeps its own Marketday,
// and a resident goes to the one nearest home, a merchant to the one nearest the shop. Worked out by the world.
struct Markets
{
    struct Place
    {
        std::string community;
        double x = 0, y = 0;                        // Where it is on the map: an outdoor cell's corner, or an interior's door.
        bool outdoors = false;                      // (Outdoors, a spot's own position is added.)
    };
    std::unordered_map<std::string, Place> places;  // By cell, in communities with more than one market.
    std::unordered_map<std::string, std::vector<std::pair<std::string, Spot>>> squares;   // By community: plan key, middle
                                                                                          // on the map (the first its own).
    // The plan key of the market nearest a spot (null where its community has one market, or none).
    const std::string* nearest(const std::string& cell, double x, double y) const
    {
        const auto place = places.find(cell);
        if (place == places.end())
            return nullptr;
        const auto list = squares.find(place->second.community);
        if (list == squares.end())
            return nullptr;
        const double px = place->second.x + (place->second.outdoors ? x : 0), py = place->second.y + (place->second.outdoors ? y : 0);
        const std::string* best = nullptr;
        double nearest = 0;
        for (const auto& [key, at] : list->second)
            if (const double d = std::hypot(at.x - px, at.y - py); !best || d < nearest)
                best = &key, nearest = d;
        return best;
    }
};
struct LifeDay
{
    std::map<std::string, DayPlan> plans;           // By community (a region), and a city's other markets ("<city>#1"...).
    std::shared_ptr<const Markets> markets;         // (Null: one market to a community.)
    std::shared_ptr<const std::map<std::string, std::vector<WorkGround>>> grounds;   // By community (doc 42, 3b).
    Harvest harvest;
    std::function<std::string(const std::string& cell)> communityOf;
    std::function<int(const Spot& at)> sky;  // There: -1 indoors, 0 fair, 1 wet (rain), 2 harsh (storm, snow).
};
// Careers (Docs/Design/26-living-npcs.md, Phase 4). A position is a job the town has, built from the authored
// residents (the job each founding resident was written with); it outlives whoever holds it.
struct Position
{
    std::string id;       // "job:" + the founding resident's ID.
    std::string founder, title, role;
    Spot work, serve;
    double startHour = 8, endHour = 17;
    std::string route;
    bool paid = true;
    double level = 0;     // The skill its trade needs (Society::tradeLevel; doc 64, Phase 8).
};
// Who holds a position now, who is learning it, and since when it has stood empty (-1: it hasn't).
struct PositionState
{
    std::string holder, apprentice, lastHolder;
    double vacantSince = -1;
    bool newcomerAsked = false;   // A stranger has been sent for (see ResidentRequest).
};
struct Mourning
{
    double until = 0;
    std::string whom;     // Who is mourned (an ID).
};
struct CareerState
{
    std::map<std::string, PositionState> positions;   // By position ID.
    std::map<std::string, double> skill;              // "resident|position" -> 0..100.
    std::map<std::string, Mourning> mourning;         // By resident.
    std::map<std::string, double> estates;            // The dead whose estates wait to be settled -> day of death.
    std::map<std::string, std::string> spouses;       // Both ways.
    std::map<std::string, std::vector<std::string>> parents;   // Child -> parents.
    std::map<std::string, double> lastBirth;          // "a|b" (a < b) -> day their last child was born.
    std::map<std::string, int> births;                // "a|b" -> how many children they have had.
    // Each town's people, as it should stay (doc 62: the user, 2026-10-13): its living residents the first day this was
    // kept. Children are born while a town is short of it; the old die; so the world stays about the size it was made.
    std::map<std::string, int> population;
    std::int64_t day = -1;                            // The last day careers were tended.
};
// A resident the world should gain: a stranger for a post nobody here can fill, or a child. The society decides that
// one is wanted; the host makes them (a live world adds a row to live.npcs, cloned from `templateId`), then calls
// welcome() with their ID.
struct ResidentRequest
{
    std::string kind;             // "newcomer" or "birth".
    std::string templateId;       // Whose record the newcomer's is cloned from (appearance, voice, manner).
    std::string name;
    int age = 0;
    Spot home;
    std::string positionId;       // Newcomer: the post they come for.
    std::vector<std::string> parents;
};
// Skill families (a placeholder until skills are designed): what a job's skill is mostly about, so that some of it
// carries to a similar job. "keeping the inn" and "keeping the stall" are both trade; a sawyer and a quarryman labour.
const char* skillFamily(const std::string& title);
// How long paid work takes at a skill (1 at 50; faster for the skilled, slower for beginners): 0.75..1.25.
// How long a spell's work takes against the usual: a hand at its trade's level (doc 64, Phase 8; 50 when unknown)
// works at the usual pace, a better one up to a quarter quicker, a worse one up to a quarter slower.
double workPace(double skill, double level = 50);
// Something that happened to a career, for the world's event log.
struct CareerNote
{
    std::string kind, actor, target, detail;
};
// What the society asks of the world when tending careers: who is alive, how old, how one regards another (0..100).
struct CareerWorld
{
    std::function<bool(const std::string&)> alive;
    std::function<int(const std::string&)> age;
    std::function<double(const std::string& who, const std::string& ofWhom)> regard;
    // Whether a home in one cell is within reach of work in another (the same town); null: anywhere is.
    std::function<bool(const std::string& workCell, const std::string& homeCell)> near;
};

// The month's books (Docs/Design/42-money-in-circulation.md, Phase 1): what each resident had when the month began, and
// what came in or went out since that was neither earned nor spent (an inheritance, a gift, last month's tax). Profit
// is the difference less that. Every MonthDays, a tenth of a profit goes to the town and a tenth to its church.
struct MonthBooks
{
    std::int64_t month = -1;                        // Which reckoning period (day / ReckonDays: a week) these are; -1: not begun.
    std::map<std::string, std::int64_t> start;      // Resident -> cash when the month (or their time here) began.
    std::map<std::string, std::int64_t> unearned;   // Resident -> money in less money out, neither earned nor spent.
    std::int64_t revision = 0;                      // Counts changes, for the save's journal.
};
constexpr int MonthDays = 28;
constexpr int ReckonDays = 7;                       // How often tax and tithes are reckoned: weekly (the user, 2026-10-05).
// Great houses and the businesses they own (Docs/Design/42-money-in-circulation.md, Phase 5b). A house is its head's
// (a resident "ruling House Fell"); it owns some of its town's businesses, each with a till of its own
// ("till:<position>") kept apart from its manager's purse. Which house owns which, and how often each till has been
// propped up lately, are saved.
struct HouseState
{
    bool founded = false;                           // Ownership has been given out (once).
    std::map<std::string, std::string> owner;       // Position (a business) -> its house ("house:<id>").
    std::map<std::string, std::vector<double>> propped;   // Position -> the days its till ran low lately.
    std::int64_t day = -1;                          // The last day the houses were tended.
    std::int64_t revision = 0;                      // Counts changes, for the save's journal.
    // The rules ownership was last given out by (the houses there were, and Data/Economy/houses.json, doc 61): when
    // they change, a save's businesses are given out again, once (Society::sortHouses).
    std::string plan;
};
struct House
{
    std::string id, name, head, community;          // id: "house:fell"; head: the head's position.
    std::string trade;                              // Its trade (Data/Economy/houses.json, doc 61), or none.
};

// What the economy remembers from day to day (doc 42): who has gone unpaid since when (Phase 3), and each treasury's,
// church's and house's usual spending a day, for its reserve (the rule against hoarding). Saved with the society.
// An employer's book (Docs/Design/64-economy-migration.md, Phase 6): its own day's wage, the days this week a post of
// its stood open, its calm weeks in a row (none open), and whether it fell short paying a wage since its last review.
struct EmployerBook
{
    double wage = 0;
    int openDays = 0, calm = 0;
    bool shortPay = false;
};
// A field's year (doc 64, Phase 3b: seasonal fields, with work by the hours): the acres it plans to sow, those sown and
// those still to bring in, a part-yield carried over, the year whose harvest's end it last planned at, and whether its
// store ran dry this year; its yields a day wanted over the year (a slow average, the model's year's sales: doc 64,
// Phase 13), the day it was founded and whether its founding store has been topped up since; and its yields a day
// wanted over the last month (the model's daily_sales), what its founding spring and next year are planned by: a
// running average over FieldMonthWeeks, a week at a time (each week's days alike, whatever day its caravan buys on), set
// right for the weeks it has run; this week's so far, and its days in all (monthDays).
struct FieldYear
{
    double acres = 0, sown = 0, toHarvest = 0, part = 0;
    std::int64_t year = -1;
    bool dry = false;
    double rate = 0;
    std::int64_t founded = -1;
    bool toppedUp = true;
    double month = 0, week = 0;
    int monthDays = 0;
};
// A thing an NPC owns and wears or carries (doc 64, Phase 9): what it is, how whole (1 new, 0 worn out but kept and
// mendable), whether it's an heirloom, and whether it's in its purse (finery, a blade) or worn (clothes, an issued sword).
struct Belonging
{
    std::string item;
    double cond = 1;
    bool heirloom = false, held = false;
};
// The mint's book (doc 64, Phase 11; RatwMint.cpp): on or off (the Dungeon Master's lever), the money in use it keeps
// to, what it has struck and not yet melted, the going wage week by week (eight), the hoarded money week by week
// (four), and each player's or Hold's spending a day (a month's average).
struct MintBook
{
    bool on = true;
    double target = 0, struck = 0;
    std::vector<double> wages, recent;
    std::map<std::string, double> spending;
};
struct EconomyMemory
{
    std::map<std::string, double> unpaidSince;      // Worker -> the day its employer first couldn't pay.
    std::map<std::string, double> outgoing;         // Collector -> a slow average of its days' spending.
    std::map<std::string, double> condition;        // Community -> its buildings' repair, 0 to 100 (the Town Works).
    int purses = 0;                                 // The grant of starting money the world has had (Society::PursesFounded).
    int tills = 0;                                  // Owner-run businesses have tills of their own (Society::TillsFounded, doc 46).
    // Business rescue (doc 46, Phase 6): a failing business's till -> what its town's rescue fund lent it, and the day it
    // was first lent. No more are lent since the orchestrator stepped back (doc 64, Phase 12); one still owed may be paid
    // off by a friend (repayRescue), and after MonthDays it is written off.
    std::map<std::string, std::pair<std::int64_t, std::int64_t>> loans;
    // Households (the user, 2026-10-05): who keeps the house in a comfortable one (home -> resident); each home's run of
    // comfortable days (counting up) or poor ones (down); and the poor homes whose stay-at-home members go to work.
    std::map<std::string, std::string> keeper;
    // Farmhands lodging at a farm's bunkhouse while hired (doc 42, "Farmhands"): resident -> its own home and the last
    // day of its hire ("cell|x|y|until").
    std::map<std::string, std::string> lodging;
    std::map<std::string, int> comfort;
    std::set<std::string> toWork;
    // Improvements to a business's premises (position -> level, 0 to MostImprovement), and the weeks in a row each has
    // gone without its upkeep (two, and it loses a level).
    std::map<std::string, int> improved, neglected;
    // What sellers' stock cost them and their margins (Docs/Design/64-economy-migration.md, Phase 4): "account|item" ->
    // the average cost a piece (pennies), and "account|good" -> the seller's margin. Copied from the society's own once
    // a day (Society::keepPrices), so they're saved with the rest.
    std::map<std::string, double> costs, margins;
    // Businesses and owners (doc 64, Phase 5): what each business's owner has put into its till lately (till -> pennies,
    // a quarter's worth, wearing away by a thirteenth a week and paid back by its profits), and trade credit ("buyer's
    // till|seller's till" -> what it owes and the day it first owed it).
    std::map<std::string, double> putIn;
    std::map<std::string, std::pair<std::int64_t, std::int64_t>> credit;
    std::map<std::string, EmployerBook> employers;   // Employer (Society::employerKey) -> its book (doc 64, Phase 6).
    std::map<std::string, double> going;             // Town ("" the realm) -> its going rate, as last measured (Phase 13).
    std::set<std::string> closed;                   // Posts a business has let go and keeps empty (doc 64, Phase 6).
    double pace = 0;                                 // The realm's way to the calibrated hours, 0 to 1 (Society::PaceStep).
    // The realm's food in hand at the day passes of the last week, days a head, each with its day (a day it missed has
    // none: the week is still a week).
    std::vector<std::pair<std::int64_t, double>> paceCover;
    std::map<std::string, FieldYear> fields;         // A farm's, an orchard's or a vineyard's till -> its year (Phase 3b).
    std::map<std::string, std::vector<Belonging>> belongings;   // A grown wolf -> what it owns (Phase 9).
    // Households' pieces founded (doc 64, Phase 13: once, in a new world's first days with work by the hours; never into a
    // world that has lived), and makers' mending and builders' works, hours a day (Society::keepRates).
    int piecesFounded = 0;
    std::map<std::string, double> mendRate, worksRate;
    // A household waiting for a piece its fund runs to and no shelf has (its want fund's key -> the day it began): a few
    // days at most (Society::PieceWaitDays), then its works go on while it looks (Phase 13).
    std::map<std::string, std::int64_t> pieceWait;
    MintBook mint;                                   // The mint (Phase 11).
    double taxRate = .08;                            // The tax on a week's income (Society::TaxRate to start): the Dungeon
                                                     // Master's lever (Phase 12, Society::setTaxRate).
    // Households and institutions (doc 64, Phase 7): each account's earned income this week (taxed at the reckoning)
    // and over the last month (a running sum, a 28th off a day); each home's want fund and help budget; each
    // institution's relief set aside for the week, its relief given this week, and the wages it paid this week.
    std::map<std::string, double> weekIncome, monthIncome, wantFund, helpBudget, reliefLeft, reliefWeek, wagesWeek;
    std::int64_t revision = 0;
};

struct SocietyState
{
    bool enabled = false;
    std::map<std::string, EconomyAccount> accounts;
    std::map<std::string, ResidentLife> residents;
    std::vector<EconomyEntry> ledger;
    std::int64_t minted = 0, sunk = 0, nextEntry = 1, budgetDay = 0;
    int exportsRemaining = 8, importsRemaining = 4, herbPatch = 40;
    // Which grant of crafting materials the shopkeepers have had (doc 35, Phase 5): an older save's makers are given
    // their starting materials once, when it is first run with crafting.
    int craftingStocked = 0;
    double decisionRemainder = 0;
    CareerState careers;
    MonthBooks books;
    HouseState houses;
    EconomyMemory memory;
    orchestra::State orchestrator;                  // The economy's monitor: its memory and its last measures (doc 46).
};
struct EconomyResult
{
    bool ok = false;
    std::string message;
    std::int64_t unitPrice = 0, total = 0;
};

// Which resident population a world runs: none, the hardcoded demo six, or
// residents authored in a world file (Atlas Workshop exports, e.g. Greyfen).
enum class Roster
{
    None,
    Demo,
    Authored
};

// One authored resident. Roles: "merchant" keeps shop during its hours,
// "guard" is on watch during its hours (walking its route, or holding its
// work post), "civilian" works during its hours and socializes after.
struct ResidentSpec
{
    std::string id, name, role, workLabel, description, greeting;
    std::string personality, backstory; // For the live-dialogue backend; never simulation state.
    int age = 30, speakingColor = 0;
    Appearance appearance;
    Spot home, work, evening;
    Spot serve; // Merchants only: where customers stand. Derived at load.
    double startHour = 8, endHour = 17; // May wrap past midnight, e.g. 18 -> 6.
    std::string route;                  // Guards only; empty holds the work post.
    bool paid = true;
    std::int64_t purse = 30;
    int herbs = 0, meals = 1;
    // A painted wander area (Dungeon Master): open tiles a civilian roams in work hours and evenings.
    std::vector<Spot> wander;
    // May travel with a player's party (Atlas; Docs/Design/32, 2.3), at any hour.
    bool joinable = false;
    // Can't be attacked by players, nor robbed of coin (Atlas; Docs/Design/57-changing-the-world.md, 6).
    bool protectedNpc = false;
};
struct PatrolRoute
{
    std::string id;
    std::vector<Spot> posts;
};
struct EconomySpec
{
    std::int64_t treasury = 1000;
    int storeHerbs = 100, storeMeals = 50, dailyHerbs = 10, dailyMeals = 12;
};
// A family as authored (doc 62, decision 8): `a` is `b`'s spouse, or `a` is `b`'s parent.
struct Kin
{
    std::string a, b, kind;
};
struct AuthoredRoster
{
    std::vector<ResidentSpec> residents;
    std::vector<Kin> kin;
    std::map<std::string, PatrolRoute> routes;
    EconomySpec economy;
};

// Deterministic needs + finite stock/cash. No dialogue model, wall clock, or
// client-provided prices. World supplies physical body locations and navigates
// the returned goals; effects occur only at the required local position.
class Society
{
  public:
    explicit Society(bool demo = true);
    explicit Society(Roster roster);
    void reset(bool demo);
    void reset(Roster roster);
    // Installs an authored population and resets to it.
    void configure(const AuthoredRoster& roster);
    // Families as authored, replaced (doc 62: a DM's edit, read again on voice.sync); taken in at the next day's turn.
    void setKin(std::vector<Kin> kin) { authored_.kin = std::move(kin); }
    const std::vector<Kin>& kin() const { return authored_.kin; }
    Roster roster() const { return roster_; }
    const AuthoredRoster& authored() const { return authored_; }
    const ResidentSpec* spec(const std::string& id) const;
    // Careers (see Position): the town's positions, the one a resident holds (or null), and what holds each.
    const std::vector<Position>& positions() const;
    const Position* position(const std::string& id) const;
    const Position* jobOf(const std::string& resident) const;
    const Position* apprenticedTo(const std::string& resident) const;
    double skill(const std::string& resident, const std::string& position) const;
    // Work at a position makes one better at it, more slowly near mastery (rate: skill per call, before slowing).
    double practise(const std::string& resident, const std::string& position, double rate);   // The skill now.
    // Family: the same household (home) and surname. Household: the same home.
    bool family(const std::string& a, const std::string& b) const;
    bool household(const std::string& a, const std::string& b) const;
    const Mourning* mourning(const std::string& resident) const;
    // A resident has died: their position stands empty and their estate waits a day to be settled.
    std::vector<CareerNote> died(const std::string& resident, double day);
    // Brought back: the estate is theirs still, and their position too if nobody has taken it.
    std::vector<CareerNote> revived(const std::string& resident);
    void mourn(const std::string& resident, const std::string& whom, double until);
    // Once a game day: estates settled, empty positions filled (apprentice, then family, then anyone local out of
    // work), apprentices taken on and finished, mourning ended. Returns what happened.
    std::vector<CareerNote> tendCareers(double day, const CareerWorld& world);
    // A resident moves house (on marrying, say): they walk there and it becomes home on arrival. Unlike relocate()
    // (the Dungeon Master's migrations, which spare those in essential work), anyone may.
    bool moveHome(const std::string& resident, const std::string& cell, double x, double y);
    // Two herb bundles become one prepared meal, in `who`'s stock (a player at a camp's cookfire). False without them.
    bool cook(const std::string& who);
    // Moves existing money and goods between accounts, recorded as `kind` (a caravan's load, an escrowed reward, a
    // town's tithe). Nothing is made or lost; what the receiver can't hold stays put. False if nothing moved.
    bool shift(const std::string& from, const std::string& to, const std::string& item, int quantity, std::int64_t coins,
               const std::string& kind);
    // A Gift lent to a maker (doc 43: a Gifted wolf's Forge Heat at a smithy, Clay Hand at a potter's...): its next batch
    // made by `untilDay` scores `lift` more toward a better quality. Not saved.
    void lendGift(const std::string& maker, double lift, double untilDay);
    double giftLiftOf(const std::string& maker) const;
    // A paw lent to a maker (doc 53, 3): a player handing at a resident's workshop. Its batches begun by `untilDay` take
    // their time divided by `rate` (the joint's), and now and then one input isn't used up. Not saved.
    void lendPaw(const std::string& maker, double rate, double untilDay);
    // A sale: goods from `seller` to `buyer` at `price` each, paid at once (a caravan buying and selling, doc 42).
    bool sale(const std::string& seller, const std::string& buyer, const std::string& item, int quantity, std::int64_t price,
              const std::string& kind)
    {
        return transfer(seller, buyer, item, quantity, price, kind);
    }
    // Whether materials short at home come only by caravan (doc 42, Phase 7: a world of towns), or are bought from
    // another community's shops at once, carted in (a world of one town, or a society alone).
    void setTradeByCaravan(bool on) { tradeByCaravan_ = on; }
    // Goods used up (eaten by bandits, say): gone from the world, recorded as `kind`.
    int consume(const std::string& account, const std::string& item, int quantity, const std::string& kind);
    // A facility account (see facilityAccount), empty to begin with; closing one needs it empty.
    bool openAccount(const std::string& id);
    bool closeAccount(const std::string& id);
    // Which store each cell's merchants restock from (default: the treasury, the one store of a single town).
    void setStores(std::map<std::string, std::string> byCell) { storeForCell_ = std::move(byCell); }
    // Home storage (Docs/Design/36-home-storage.md): each household's larder (food), chest (goods), wardrobe (wear)
    // and woodpile (fuel), an account each ("home:<home cell>:<kind>"), owned by everyone who lives there.
    static constexpr const char* StoreKinds[] = {"larder", "chest", "wardrobe", "woodpile"};
    static std::string homeStore(const std::string& homeCell, const std::string& kind) { return "home:" + homeCell + ":" + kind; }
    // Opens the stores of these homes where they don't exist yet, and stocks a new larder and chest for the
    // household: a few days' meals and some herbs each, and a meal carried by anyone who has none.
    void furnishHomes(const std::set<std::string>& homeCells);
    // Where each home's stores stand, by home cell and kind (the world places them); a store not placed yet is
    // reached at its household's home spot.
    void setHomeStores(std::map<std::string, std::map<std::string, Spot>> spots) { homeStores_ = std::move(spots); }
    const std::map<std::string, std::map<std::string, Spot>>& homeStores() const { return homeStores_; }
    // Where each resident sleeps (the world gives each a place on a bed in their home, up to four to a bed); without
    // one, their home spot.
    // The world's towns changed (World::setupTowns): what the residents' records keep of their communities is forgotten.
    void forgetPlaces() { ++rosterRevision_; }
    // Each farm worker's bunkhouse (the world finds them: a room "<site> Bunkhouse" for a worker "... at <site>").
    static bool farmWork(const std::string& producer);   // A farm, orchard, vineyard, herd or flock (RatwFarmhands.cpp).
    // Seasonal fields (doc 64, Phase 3b; the model's): with work by the hours, a farm, an orchard or a vineyard sows in
    // spring (days of the year before SowEnd) and brings in at the harvest (HarvestStart to HarvestEnd) what it sowed,
    // FieldYields of its yields an acre, a third of an acre's hours at sowing and the rest at the harvest; it sells from
    // its store all year. At the harvest's end it plans next year's acres: a year and a harvest of its sales a day, a
    // tenth over (FieldMargin), half again after a year its store ran dry (DryYear), less what its store still holds.
    static bool fieldWork(const std::string& producer);
    static constexpr int SowEnd = 92, HarvestStart = 213, HarvestEnd = 274, FieldYields = 20;
    static constexpr int WinterStart = 275;         // (The calendar's: days of the year from 275 to its end.)
    static constexpr double SowShare = 1. / 3, FieldMargin = 1.1, DryYear = 1.5;
    // The mint (doc 64, Phase 11; RatwMint.cpp), daily in the day's pass and weighing up on the week's first day: a
    // player's purse or a Hold's treasury (`hoarder`), whose spending it follows (noteSpent, from record).
    static bool hoarder(const std::string& account);
    void noteSpent(const std::string& from, std::int64_t coins);
    void tendMint(std::int64_t day);
    static constexpr double MintBand = .05, MintStrikeMost = .01, MintMeltMost = .02, MintWageFall = .03, MintIdle = .1,
                            MintHungry = .01;
    // Wear and mending for NPCs (doc 64, Phase 9; RatwBelongings.cpp): a piece worn to MendAt is seen to, mended if the
    // quote is under MendIf of new; a guard's issued sword wears out in WatchWearDays of daily use.
    static constexpr double MendAt = .35, MendIf = .6, WatchWearDays = 120;
    static std::string belongingKind(const std::string& item);   // "finery", "sword", "rapier" or "" (of a held item).
    void tendBelongings(std::int64_t day, const std::map<std::string, LifeBody>& bodies);
    double mendQuote(const std::string& maker, const items::Craft& k, const Belonging& b, const std::string& kind) const;
    bool seeTo(const std::string& id, Belonging& b, const std::string& kind, bool issued, std::int64_t day,
               const std::vector<std::string>& shops);
    // 0 out of season, 1 sowing, 2 the harvest, for a day of the calendar.
    static int fieldWindow(double absoluteDay);
    bool fieldsOn() const { return workByHours_; }
    // A field's yields a day wanted: the most of any of its food's sales a day (missed sales counting), by its count
    // (doc 64, Phase 13: what feeds, or goes only into food or flour, grownToEat; the rest comes with it, as hides with
    // the model's sheep; a field that grows no food, by all it grows).
    double fieldNeed(const std::string& till, const items::Producer& p) const;
    bool grownToEat(const std::string& item) const;
    bool plannedBy(const items::Producer& p, const std::string& item) const;   // Whether its acres are planned by the good.
    // Its year planned (at the harvest's end, or when it is first seen, with a store to last it to the harvest). It is
    // first seen after FieldFoundDays of sales, founded on its sales a day over its whole weeks of them (foundingRate:
    // 0 before a week), and in its founding spring its acres are looked at again weekly and its store topped up once,
    // FieldTopUpDays on (reviewField). Daily, each field's month's and year's rates (tendFields).
    double foundingRate(const std::string& till, const items::Producer& p) const;
    bool fieldTill(const std::string& till) const;   // A till of a farm's, orchard's or vineyard's.
    void planField(const std::string& id, const std::string& till, const items::Producer& p, double absoluteDay, bool founding);
    void reviewField(const std::string& id, const std::string& till, const items::Producer& p, double absoluteDay);
    void tendFields();
    double fieldHeld(const std::string& till, const items::Producer& p) const;   // In yields of what it is planned by.
    void storeYields(const std::string& id, const std::string& till, const items::Producer& p, double yields);   // "A year's store".
    static constexpr int FieldFoundDays = 7, FieldTopUpDays = 14, FieldMonthWeeks = MonthDays / 7, FieldMonthDaysMost = 10000000;
    // What none of a town's sellers had to sell (doc 64, Phase 13; the model's unmet asks): counted at the town's
    // producers of the good as sales missed, shared among them, so the land plans and hires for it.
    void missedAt(const std::string& town, const std::string& item, int n);
    void setBunkhouses(std::map<std::string, Spot> byWorker)
    {
        bunkhouses_ = std::move(byWorker);
    }
    void setBeds(std::map<std::string, Spot> beds)
    {
        beds_ = std::move(beds);
        ++rosterRevision_;                          // (The residents' records keep their beds.)
    }
    const std::string& storeFor(const std::string& cell) const;
    // How dear goods are at each store's markets (1: as ever), set by the world from how much each town has; a
    // merchant's prices follow the store they restock from.
    void setPriceFactors(std::map<std::string, std::map<std::string, double>> byStore) { priceFactors_ = std::move(byStore); }
    double priceFactor(const std::string& merchant, const std::string& item) const;
    // Marriage: both must be unmarried; from now on they are family.
    bool marry(const std::string& a, const std::string& b);
    const std::string* spouse(const std::string& resident) const;
    // Residents wanted since the last call (see ResidentRequest), and a made one taking their place in the society.
    std::vector<ResidentRequest> takeRequests();
    CareerNote welcome(const ResidentRequest& request, const std::string& id);
    // The best a resident is at any job of this family (see skillFamily).
    double familySkill(const std::string& resident, const std::string& family) const;
    // A player asks to learn a position's trade from its holder.
    CareerNote apprentice(const std::string& player, const std::string& positionId, const CareerWorld& world, double day);
    // A player's help with a resident's trouble (Docs/Design/57-changing-the-world.md, 3): a vacant post given to a resident
    // now, as succession would give it (leaving any other post it held); a youth apprenticed now to a position's holder,
    // under the weekly chance's own rules (12 to 25, no master yet, no work elsewhere; a master of 35 or skilled 70, not
    // the watch); a business's rescue loan paid off from `payer`'s purse into its town's treasury. "refused" notes, or
    // what changed (the paid-off coins).
    CareerNote appoint(const std::string& positionId, const std::string& resident, const CareerWorld& world, double day);
    CareerNote apprenticeTo(const std::string& positionId, const std::string& youth, const CareerWorld& world, double day);
    std::int64_t repayRescue(const std::string& till, const std::string& payer, std::int64_t most);
    void addPlayer(const std::string& id);
    const EconomyAccount* account(const std::string& id) const;
    const ResidentLife* resident(const std::string& id) const;
    const SocietyState& state() const { return state_; }
    // Restores a checkpoint. The world may have gained or lost residents since it was written: those still here keep
    // their saved lives and money, new ones start fresh with their authored purse (minted), and the coins of those
    // who have gone return to the treasury, so the money supply stays conserved.
    bool restore(const SocietyState& candidate);
    // Sends a resident home to their authored bed (their saved home no longer exists). False without such a resident.
    bool rehome(const std::string& id);
    void tick(double seconds, double absoluteDay, int season, const std::map<std::string, LifeBody>& bodies);
    // Whether tick(seconds) would decide anything: the bodies it is given go unread otherwise.
    bool decidesWithin(double seconds) const { return state_.enabled && state_.decisionRemainder + seconds + 1e-8 >= 1.; }
    // Unseen residents decide every UnseenStep seconds, a step's worth at a time; those a player may see, every second.
    static constexpr int UnseenStep = 5;
    // Whether tick(seconds) at absoluteDay needs the bodies at all: anyone seen (`anySeen`), the unseen's step, a new day,
    // or the society's first decisions. Otherwise it is given none, and nothing happens to anyone that second.
    bool needsBodies(double seconds, double absoluteDay, bool anySeen) const;
    // Whether the last decisions included the unseen residents' (their errands follow: World::continueSchedules).
    bool unseenDecided() const { return unseenDecided_; }
    // The day's plans, for the decisions that follow (the world sets them before each tick; without, every day is
    // an ordinary working day under a fair sky).
    void setDay(LifeDay day) { day_ = std::move(day); }
    const LifeDay& day() const { return day_; }
    // How the residents' deciding is spread over threads (the world's runner, World::setParallel): run(count, job) calls
    // job(i) for every i below count and returns when all are done. Unset, the game thread decides alone, the same way.
    using Parallel = std::function<void(std::size_t, const std::function<void(std::size_t)>&)>;
    void setParallel(Parallel run) { parallel_ = std::move(run); }
    bool atStall(const std::string& merchant) const { return atStall_.count(merchant) > 0; }
    EconomyResult quote(const std::string& player, const std::string& merchant, const std::string& item,
                        int quantity, bool buy) const;
    EconomyResult trade(const std::string& player, const std::string& merchant, const std::string& item,
                        int quantity, bool buy);
    EconomyResult gather(const std::string& player); // World validates physical herb-patch reach.
    EconomyResult eat(const std::string& player);
    // Operator-only adapters call these; neither is a player command.
    EconomyResult operatorTransfer(const std::string& from, const std::string& to,
                                  const std::string& item, int quantity, std::int64_t coins);
    bool relocate(const std::string& npc, const std::string& cell, double x, double y);
    // Live changes (Dungeon Master): takes resident `id` as `from` (a validated candidate society) defines them.
    // A new resident arrives with their authored purse; a changed one keeps purse and needs but re-plans; one
    // `from` no longer has leaves, their purse and goods returning to the treasury. Routes follow `from`.
    bool adoptResident(const Society& from, const std::string& id);
    // A resident made in the running world (a birth, a newcomer: doc 64, Phase 8), with its purse (new money).
    bool addResident(const ResidentSpec& resident, std::int64_t purse);
    // Takes patrol routes and every resident's wander area from `from` (a validated candidate society).
    void adoptLayers(const Society& from);
    static int stock(const EconomyAccount& account, const std::string& item);
    // A good in every quality held (doc 35, Part 4): a maker's hides, common, crude, fine and masterwork together.
    static int stockAll(const EconomyAccount& account, const std::string& base);
    static std::vector<std::string> kindsHeld(const EconomyAccount& account, const std::string& base);
    // Food (RatwDemand.cpp; doc 35, Part 7): anything of the catalog that feeds (drinks aside), how much, and the best
    // an account holds to eat.
    static bool edible(const std::string& item);
    static int nourishment(const std::string& item);
    // Whether a good rots (spoil): a food that keeps so many days (fish, bread), not one that keeps but feeds nobody
    // (milk, hides).
    static bool rots(const items::Item* good);
    static std::string bestFood(const EconomyAccount& account);
    // What to eat first: the food that spoils soonest (Item::keeps), the most nourishing of those (doc 42, "Spoilage").
    static std::string eatFirst(const EconomyAccount& account);
    // The same by the age of what it holds (its oldest batch of each food): what an account eats first.
    std::string eatFirst(const std::string& id, const EconomyAccount& account) const;
    bool forSale(const std::string& id, const std::string& item) const;   // A producer's own goods, for the shops.
    static bool hasFood(const EconomyAccount& account);   // !bestFood(account).empty(), without building it.
    // Goods made (not bought: a smith's work, a grant): only goods, never money.
    bool create(const std::string& account, const std::string& item, int quantity, const std::string& reason);
    bool merchant(const std::string& id) const;
    // A smith: a merchant whose trade is the forge (its work says "smith" or "forge"; the demo's Ash). A smith deals in
    // swords, starting with SmithSwords and forging another while it has fewer.
    bool smith(const std::string& id) const;
    // What a merchant sells (Docs/Design/39). A shop of a kind in Data/Items/businesses.json (found by its keeper's work
    // label: "baker at The Amber Loaf") sells a handful of that kind's cheap goods, the same handful every day but not
    // every shop of the kind the same; a smith adds swords; a shop of food adds meals, an herbalist herbs. Anyone else
    // (an innkeeper, a market trader, the demo's keeper) deals in herbs and meals.
    std::vector<std::string> wares(const std::string& merchant) const;
    // A shop of food (a general store, a provisioner, a stall, a bakery, a butcher, a fishmonger, a brewery, an inn): it
    // sells any wholesome food it has in, besides its own wares (Docs/Design/42, "Food between towns").
    bool foodShop(const std::string& merchant) const;
    // The first of its wares that feeds and that it makes or supplies, or "" for none (a brewery's: its drinks feed
    // nobody, and nobody there makes the meal every food shop sells): where a missed meal is counted (doc 64, Phase 13).
    const std::string& foodMade(const std::string& shop) const;
    // How many of a good a business uses a day (doc 61), by its till ("till:<position>"): through each of its crafts that
    // takes it, as fast as what the craft makes sells (missed sales too); a supplier, as fast as it sells the good.
    double useRate(const std::string& till, const std::string& item) const;
    // The store of a craft's material a maker keeps (doc 64, Phase 13; the model's lay_in): `least` (its batches'
    // worth), or for what goes into food or flour, days of its use, StapleInputDays for a staple (bread, porridge),
    // InputDays otherwise, never longer than a material that rots keeps.
    int inputStore(const std::string& till, const items::Craft& k, const std::string& item, int least) const;
    static constexpr int StapleInputDays = 7, InputDays = 4;
    // How many of a good a seller keeps (doc 64, Phase 3b): output follows sales. Its days' keeping (keepDays) of what
    // it sells a day (food, over the shut days ahead too, as far as it keeps: Phase 13), missed sales counting, a fifth
    // over for a staple; with `orders`, at least its share of a week of the standing orders on its town's makers or
    // producers of it and half a supplier's store; never under `least` (the old fixed store, now a floor, so a new shop
    // still has a few in).
    int shelfTarget(const std::string& till, const std::string& item, int least, bool orders = true) const;
    // The roads' standing orders (doc 61), told by World each day: by selling town and good, how many go a week (its
    // makers share that much more of a good that keeps, kept for the caravan); by buyer's till, what its orders cost a
    // week (a house's business keeps that in its till, floatOf, to pay for them when they come).
    void setStandingOrders(const std::map<std::pair<std::string, std::string>, int>& perWeek, std::map<std::string, std::int64_t> costs);
    bool sellsFood(const std::string& merchant, const std::string& item, const std::vector<std::string>& wares) const;
    // Producers sell their town's food shops what food they bring in beyond ProduceKept, daily, a shop taking up to
    // FoodShelf of a food, at a shop's buying price (doc 15: a little over half), as far as it can pay.
    void producersSell(std::int64_t day);
    // Spoilage (doc 42, the user, 2026-10-06): each account's food in batches by the day it came in; once a day, what has
    // come in since is a batch of today, what has gone went from the oldest, and a batch older than its food keeps
    // (Item::keeps) spoils. Goods bought are fresh to their buyer; on the road, in a caravan, they age. Players' packs
    // don't spoil (not yet). The batches aren't saved: a loaded world's food is all fresh.
    void spoil(std::int64_t day);
    // The town's going price (doc 64, Phase 4): a good's worth at the town's labourer's wage, the yardstick for a stall, a
    // contract or a measure (each seller prices its own: priceAt). Meals and herbs by the town's stores (World::tendPrices).
    double townPrice(const std::string& town, const std::string& item) const;   // Pennies a piece (quality counted).
    // A price in whole pennies: to the nearest, a penny at least (doc 46, Phase 8: rounded up, a 1p good priced at 1.4p
    // sold for 2p, twice its catalog's).
    static std::int64_t pennies(double price) { return std::max<std::int64_t>(1, std::llround(price)); }
    // An amount in labourer's days (LD), in whole pennies at the town's going rate ("" the realm's), a penny at least
    // (doc 64, Phase 13: the realm is founded at ladder.json's `foundingDay`, 60p). The amounts here that were written as
    // pennies when a day was the catalogue's 12p are kept so, in LD (5 / 12. for 5p): at 12p each is what it was.
    std::int64_t pennies(double days, const std::string& town) const { return pennies(days * goingRate(town)); }
    // A good's catalogue price (in the catalogue's pennies: a day of `catalogueDay`), at the town's going rate. Meals,
    // herbs and swords are still priced so (storePriced).
    double atGoingRate(double catalogue, const std::string& town) const;
    static constexpr double PieceMost = 1000 / 12.;  // The dearest piece the ledger moves, in LD (1,000p at a 12p day).
    // What a buyer in the town pays the land for a good, by the yardstick: its going price less the usual margin.
    double buyingPrice(const std::string& town, const std::string& item) const;
    double townFactor(const std::string& town, const std::string& item) const;   // townPrice against the catalog's.
    std::string shopTown(const std::string& shop) const;   // The town a shop works in.
    // Meals, herbs and swords: priced by the town's stores (World::tendPrices), not the orchestrator, for now; at their
    // catalogue prices at the going rate (atGoingRate).
    static bool storePriced(const std::string& item) { return item == "meal" || item == "herbs" || item == "sword"; }
    // Daily: each shop's meals sold (what it restocks) and each shop's town.
    void tendPrices(std::int64_t day);
    static constexpr int ProduceKept = 5, FoodShelf = 30;     // (Keeps 5: doc 42, "Pressure".)
    static constexpr int SmithSwords = 3;
    static constexpr int CheapPrice = 6;              // A shop's goods cost at most this, for now (pennies).
    static constexpr int GoodsKept = 4;               // How many of each good a shop keeps, making more as they sell.
    // Crafting (Data/Items/crafts.json; doc 35, Phase 5, first part). Placeholders for the balance pass: a maker keeps
    // materials for MaterialBatches batches, buys more from the town's suppliers below MaterialsLow batches' worth, and
    // a supplier starts with SuppliesKept of each ingredient it sells.
    static constexpr int MaterialBatches = 8, MaterialsLow = 2, SuppliesKept = 40;
    static constexpr int SeasonalStore = 6;         // A seasonal good's store, against an ordinary one (the harvest's).
    // A producer (a farmer, a fisher) keeps at most ProducerKept of each thing it brings in, for the town to buy.
    // Goods fetched from another community cost CartedIn times the price (they come a long way).
    static constexpr int ProducerKept = 40;          // (40, not 20: the land sat idle. Doc 42, "Pressure".)
    // Output by sales (doc 64, Phase 3b): a staple's store a fifth over its sales (StapleSlack, as its hands), and no
    // store over ShelfMost of a good.
    static constexpr int ShelfMost = 2000;
    static constexpr double CartedIn = 1.5;
    // The current grant (SocietyState::craftingStocked): 1 the starter crafts, 2 the workshops (mills, tanneries...), 3
    // masking oil (apothecaries' and perfumers' wormwood and resin, herbalists' wormwood).
    static constexpr int CraftingStock = 3;
    // Food to start with (doc 36): a new player's own meals; a new household's larder, meals for each who lives there,
    // and its chest, herbs for each. Placeholder amounts for the balance pass.
    static constexpr int StartingMeals = 3, LarderMealsEach = 3, ChestHerbsEach = 2;
    // How many days' food a household's larder holds (the user, 2026-10-06: 2 to 5), its own steady habit, so the
    // world's households don't all shop on the same day; and a new larder holds FoundingLarderDays of meals to begin
    // with (in place of LarderMealsEach for everyone): meals keep three days, and the 2 to 5 it once held mostly spoiled
    // on the fifth (doc 64, Phase 13).
    static int stockingDays(const std::string& homeCell);
    static int shutAhead(std::int64_t today);       // Days from tomorrow the shops aren't properly open (Restday, a festival).
    static constexpr int FoundingLarderDays = 2;
    static const char* itemName(const std::string& id);
    // The economy's monitor (Docs/Design/46-economy-orchestrator.md; doc 64, Phase 12; RatwOrchestrate.cpp). In an authored
    // world, once a game day at SnapshotHour, the society copies what it reads (purses, food, work, the shops' food, coins
    // between towns) and measures it; when the day turns, before the day's pass, those measures become the latest
    // (orchestrator().last), so a run repeats exactly. The evening of the weekly reckoning the week is weighed too
    // (orchestrator().decision). It sets nothing.
    static constexpr double SnapshotHour = 23;
    const orchestra::State& orchestrator() const { return state_.orchestrator; }
    // Every day's measures since the last call (econ_watch); kept only while someone takes them.
    std::vector<orchestra::Brief> takeBriefs();
    void keepBriefs(bool keep) { keepBriefs_ = keep; }
    // The Dungeon Master's levers (doc 64, decision 5; RatwOrchestrate.cpp), each logged in the ledger: the tax rate (0 to
    // MostTaxRate of a week's income, from the next reckoning); the mint on or off; and a one-off relief grant of `coins`
    // from the capital's treasury to a town's ("a relief grant").
    static constexpr double MostTaxRate = .5;
    double taxRate() const { return state_.memory.taxRate; }
    EconomyResult setTaxRate(double rate, const std::string& by);
    EconomyResult setMint(bool on, const std::string& by);
    EconomyResult reliefGrant(const std::string& town, std::int64_t coins);
    std::int64_t moneySupply() const;
    bool conserved() const;
    // Every ledger entry recorded since the last call, oldest first (the saved ledger keeps only the latest 128).
    // For the world's event log; never saved. Keeps at most JournalKept if nobody collects it.
    static constexpr std::size_t JournalKept = 50000;
    std::vector<EconomyEntry> takeJournal();

  private:
    LifeDay day_;
    Parallel parallel_;
    std::map<std::string, double> mealsSold_;       // A shop's till -> the meals it sells a day (a running average).
    // Doc 64, Phase 4 (RatwPrices.cpp): account -> item -> what its stock cost a piece, on average; seller's account ->
    // good -> its margin; and each seller's normal cost of a good it makes (by keeper or account), worked out once a day.
    std::unordered_map<std::string, std::unordered_map<std::string, double>> cost_, margins_;
    mutable std::unordered_map<std::string, std::unordered_map<std::string, double>> unitCost_;
    // Goods coming into a costed account: its average cost a piece, `had` held before, `n` more at `each`.
    void addCost(const std::string& account, const std::string& item, int had, int n, double each);
    void reviewMargins(std::int64_t day);           // Each seller's weekly review, on its own day of the week.
    void keepPrices();                              // Into the saved memory (daily), and back from it (restore).
    void loadPrices();
    std::string accountTown(const std::string& account) const;
    std::map<std::string, int> mealsToday_;         // A shop's till -> the meals it sold today.
    std::map<std::string, std::string> shopTown_;   // A shop's till -> its community (made daily).
    // What each larder gives its household a day (nourishment taken from it, a running average, Society::spoil): the
    // household lays in by what it eats from home, not by what its members would eat if they never ate out (not saved).
    std::unordered_map<std::string, double> larderUse_;
    std::unordered_map<std::string, int> larderTaken_;      // Today's, so far.
    std::map<std::string, std::map<std::string, std::deque<std::pair<std::int64_t, int>>>> batches_;
    // What each shop sells of each good a day, and the sales it missed for having none (a running average; Society::spoil
    // keeps it daily): a maker short of materials makes first what is wanted (TRIAL make_what_sells, doc 42). Not saved.
    std::unordered_map<std::string, std::unordered_map<std::string, double>> sellRate_;
    // What each seller sold out of today (its last one sold): the sales it missed after are counted as half again its
    // day's (doc 64, Phase 3b), so a shelf that empties every day keeps more until it doesn't. Not saved.
    std::unordered_map<std::string, std::set<std::string>> ranOut_;
    std::map<std::pair<std::string, std::string>, int> ordersFrom_;    // (Town, good) -> each maker's share of a week's orders.
    std::map<std::string, std::int64_t> ordersCost_;                     // Buyer's till -> its standing orders' cost a week.
    std::unordered_map<std::string, std::unordered_map<std::string, int>> soldToday_;   // Account -> food -> (day, n).
    // A shop's food wanted today when it had none left: a hungry buyer's meal, in its first food's pieces (doc 64,
    // Phase 13), counted with its sales (Society::spoil). Not saved. With work by the hours, a wolf's meal counted once a
    // day (missedBy_), at a shop it found bare, or at one of its town's open food shops when none had food
    // (decideAuthored); only at a shop that makes or supplies a food (foodMade: not a brewery's, whose meal nobody there
    // makes). (Also a piece a household's fund ran to and found on no shelf: Society::wants.)
    std::unordered_map<std::string, std::unordered_map<std::string, int>> missedToday_;
    std::unordered_set<std::string> missedBy_;
    void missedMeal(const std::string& resident, const std::string& shop);
    // Each resident by number (its place in ID order), and what its decisions every second reach through it, kept while
    // nothing they rest on changes (refreshRecords): the roster, the careers, the specs, the positions, the accounts.
    struct ResidentRecord
    {
        std::pair<const std::string, ResidentLife>* entry = nullptr;
        const ResidentSpec* spec = nullptr;
        EconomyAccount* wallet = nullptr;           // (Null: none.)
        const Position* job = nullptr;              // jobOf, apprenticedTo.
        const Position* learning = nullptr;
        std::string home, larder, community;        // Its home as last seen, that home's larder and community,
        const EconomyAccount* larderAccount = nullptr;   // and its account (null for none, yet).
        double* skill = nullptr;                    // Its skill at `job`, once it has one.
        const Spot* bed = nullptr;                  // Its place on a bed (doc 36), or null.
    };
    std::vector<ResidentRecord> records_;
    std::unordered_map<std::string, std::size_t> recordOf_;   // A resident's number, by ID.
    // The shops (merchant positions) with someone in the careers to keep them, and the keeper's number (or -1): kept
    // with the records.
    struct Shopkeeping
    {
        const Position* position;
        const std::string* holder;                  // (Its holder now: "" while the shop has none.)
    };
    std::vector<Shopkeeping> shopkeeping_;
    mutable std::uint64_t rosterRevision_ = 1;      // Moved on whenever something the records rest on changes.
    std::uint64_t recordsRevision_ = 0;             // The revision records_ were made at,
    const Society* recordsOf_ = nullptr;            // and by which society (a copy makes its own).
    void refreshRecords();
    // A resident's skill at a position, where it is kept (made, at 0, with `make`; else null for none yet).
    double* skillSlot(const std::string& resident, const std::string& position, bool make);
    std::int64_t secondsDecided_ = 0;               // Seconds decided since this society began (for UnseenStep; not saved).
    bool unseenDecided_ = false;
    // Each home's household (resident IDs), for the residents' deciding (RatwResidents.cpp): made again each day, and when
    // residents come or go.
    std::unordered_map<std::string, std::vector<std::string>> households_;
    std::int64_t householdsDay_ = -1;
    std::size_t householdsOf_ = 0;
    std::set<std::string> atStall_;                 // Merchants trading from a market stall right now.
    std::set<std::string> feasted_;                 // Fed at today's festival already.
    std::int64_t feastDay_ = -1;
    SocietyState state_;
    Roster roster_ = Roster::Demo;
    AuthoredRoster authored_;
    std::vector<EconomyEntry> journal_;
    // spec() by resident ID, built on first use: every decision looks up every resident's spec. Anything that
    // adds, removes or replaces authored_.residents must call forgetSpecs().
    mutable std::unordered_map<std::string, std::size_t> specIndex_;
    mutable std::unordered_map<std::string, std::pair<std::string, std::vector<std::string>>> waresCache_;   // id -> (label, wares)
    std::vector<Position> positions_;               // From authored_; rebuilt with it (buildPositions).
    std::unordered_map<std::string, std::size_t> positionIndex_;
    mutable std::unordered_map<std::string, std::string> heldBy_, learning_;   // resident -> position (cache).
    mutable bool careersIndexed_ = false;
    std::vector<ResidentRequest> requests_;
    void buildPositions();
    void indexCareers() const;
  public:
    // Builds the lazy indexes (careers, specs) so readers on several threads at once only read them (doc 31, Phase 4).
    void prepareReading() const
    {
        spec(std::string());
        indexCareers();
    }
  private:
    void forgetCareers() { careersIndexed_ = false; }
    void defaultCareers();
    void reconcileCareers();
    bool bequeath(const std::string& from, const std::string& to, const std::string& item, int quantity, std::int64_t coins);
    std::map<std::string, std::map<std::string, double>> priceFactors_;
    std::map<std::string, std::string> storeForCell_;   // Cell -> the store its merchants restock from (Phase 5).
    std::map<std::string, std::map<std::string, Spot>> homeStores_;
    std::map<std::string, Spot> beds_;
    static constexpr std::int64_t MoneyCap = 1000000000;
    static constexpr int StockCap = 10000;
    mutable bool specsIndexed_ = false;
    void forgetSpecs()
    {
        specsIndexed_ = false;
        suppliers_.clear();
    }
    // Crafting (RatwCrafting.cpp): a maker at work makes a batch of whatever has run low, from their own materials,
    // buying more from a supplier in the same community first if they are running out. Never from nothing.
    std::unordered_map<std::string, double> craftNext_;    // Maker -> the game day they next look at their shelves.
    std::unordered_map<std::string, std::pair<double, double>> giftLift_;   // Maker -> a lent Gift's lift, and until when (doc 43).
    std::unordered_map<std::string, std::pair<double, double>> pawLent_;    // Maker -> a lent paw's rate, and until when (doc 53).
    // Maker -> the batch at work (Data/Items/crafts.json id), done at craftNext_. Not saved: after a restart the batch
    // is begun again, its materials taken only when it is done.
    std::unordered_map<std::string, std::string> craftAtWork_;
    // Ingredient -> who has it to sell: suppliers, then workshops that make it, then producers (Seller's kind).
    struct Seller
    {
        std::string id;
        int kind = 0;                                // 0 supplier, 1 workshop, 2 producer.
    };
    mutable std::unordered_map<std::string, std::vector<Seller>> suppliers_;
    const std::vector<Seller>* sellersOf(const std::string& item) const;   // (Null for nobody.)
    // Doc 64, Phase 13 (not saved): each till's day it last laid in its food's materials (inputStore); each
    // "till|good"'s materials bought today and the day its want of it was last counted as missed (missedAt); each till's
    // days of sales in its running average (sellRate_, set right in a field's month's rate), and how many day passes
    // this society has made its fields' rates in (a restart's first week isn't folded in: tendFields); what goes into
    // food (grownToEat, worked out on first asking). And a field not yet founded, its sales since its first (each good's,
    // missed ones counting) and the days of them, and its sales over its whole weeks of them: what it is founded on
    // (foundingRate), each weekday counted alike (a farm's caravan buys once a week, and its missed sales are counted
    // the same day).
    struct Unmet
    {
        std::int64_t day = -1, counted = -1;        // (The day `bought` is of, and the day a want was last counted.)
        int bought = 0;
    };
    std::unordered_map<std::string, std::int64_t> laidIn_;
    std::unordered_map<std::string, Unmet> unmetOn_;
    std::unordered_map<std::string, int> salesDays_;
    int fieldPasses_ = 0;
    mutable std::unordered_map<std::string, bool> grownToEat_;
    struct FieldSales
    {
        int days = 0;
        std::unordered_map<std::string, double> sold, weeks;
    };
    std::unordered_map<std::string, FieldSales> fieldSales_;
    std::unordered_map<std::string, double> produceNext_;   // Producer -> the game day of its next yield.
    // Work done (Docs/Design/64-economy-migration.md, Phase 3): the hours a business's hands have put in (each paid
    // spell of its help, the keeper's time at the counter) and a producer's own, drawn on by each batch or yield at its
    // crafts.json `hours`. At most WorkBankedMost in hand: work not used is work not done. Not saved.
    std::unordered_map<std::string, double> workBanked_;    // Till (or producer) -> hours of work in hand.
    std::unordered_map<std::string, double> lastAtWork_;    // Keeper -> the game day its time at work was last counted.
    void creditWork(const std::string& account, double hours);
    static constexpr double WorkBankedMost = 24;
    // Those whose work in hand was full at the last turn of the day (idleBusiness: full then and now). Not saved.
    std::unordered_set<std::string> fullBanks_;
    bool bankFull(const std::string& account) const;
    // What each maker made of its crafts' first goods a day (till -> good -> pieces, a fortnight's running average, and
    // today's so far), and the crafts it found itself short of materials for, today and yesterday (till -> craft ids):
    // handsWanted plans a short craft by what it could make (doc 63's model, feasible; doc 64, Phase 13). Not saved.
    std::unordered_map<std::string, std::unordered_map<std::string, double>> madeRate_, madeToday_;
    std::unordered_map<std::string, std::set<std::string>> shortToday_, shortYesterday_;
    // The hours a batch or a yield takes: its calibrated crafts.json `hours` once work is by hours, else its old
    // `seconds`' worth (the pace the game had: doc 64, Phase 3, until Phase 6 lets businesses take on the hands that
    // calibrated hours need).
    double hoursFor(const items::Craft& k) const;
    double hoursFor(const items::Producer& p) const;
    // The hours a good is costed at (doc 64, Phase 13; doc 63's normal_cost): with work by the hours, those it takes now
    // (hoursFor, at the realm's pace), so a price is what its work cost; otherwise the calibrated `hours` (Phase 4).
    double costedHours(const items::Craft& k) const;
    double costedHours(const items::Producer& p) const;
    static inline bool workByHours_ = false, costAtPace_ = false;
    double workCredited_ = 0, workLost_ = 0;
    std::unordered_map<std::string, std::pair<double, double>> workBy_;   // Account -> hours credited, lost (econ_watch).

  public:
    // The realm's pace (doc 64, Phase 6): with work by the hours, a craft's or yield's hours go from the old pace to the
    // calibrated over some eight weeks (PaceStep a day) while the realm is fed, and back a little faster (PaceBack a day)
    // while more than PaceHungry of it goes hungry: the labour market needs weeks to move hands to the land, and the
    // founding's stores last days. Where it settles is what the realm's hands can carry.
    // Steadied (Phase 13): it goes on only while the realm's food in hand (homes and shelves, days a head, a town's at
    // most PaceCoverCap) hasn't fallen over the week (today's reading against the one exactly a week ago: PaceWeek
    // days, ends included) and the land's hands keep up with what its sales want (short by PaceShortHands at most; its
    // producers' want without the half again of a shelf near bare, which theirs always are, the goods going on to the
    // shops; one with no work for more hands, at the hands it has; a realm with no land, kept up); it steps back while
    // the week's food falls by over PaceSlide days a head or averages under PaceFloor days; and back faster while the
    // realm goes hungry, as before. (A week's food "didn't fall" within PaceNoise days a head: at three and a half days
    // a head the week's reading wanders by a few hundredths, and a fall of 0.002 held it as a fall of 0.1 would. Doc 64,
    // Phase 13, 2026-10-26.)
    static constexpr double PaceStep = 1. / 56, PaceBack = 1. / 14, PaceHungry = .02;
    static constexpr int PaceWeek = 8;
    static constexpr double PaceSlide = .15, PaceNoise = PaceSlide / 3, PaceFloor = 1, PaceCoverCap = 4, PaceShortHands = .05;
    // Whether residents' work takes the calibrated hours (econ_watch --hours; on in play from doc 64's Phase 6).
    static void setWorkByHours(bool on) { workByHours_ = on; }
    static bool workByHours() { return workByHours_; }
    static void setCostAtPace(bool on) { costAtPace_ = on; }   // (A test's: ladder.json's costAtPace, whatever the data says.)
    // Hours of work credited to businesses and producers so far, and those lost to a full bank (econ_watch's measures).
    double workCredited() const { return workCredited_; }
    double workLost() const { return workLost_; }
    const std::unordered_map<std::string, std::pair<double, double>>& workBy() const { return workBy_; }
    // And the labour market's (doc 64, Phase 13): each town's going rate ("" the realm's), an employer's town, and an
    // account's hours of work in hand.
    const std::map<std::string, double>& goingRates() const { return state_.memory.going; }
    std::string employerTownOf(const std::string& key) const
    {
        const auto found = employerTown_.find(key);
        return found == employerTown_.end() ? std::string() : found->second;
    }
    double workBankedOf(const std::string& account) const
    {
        const auto found = workBanked_.find(account);
        return found == workBanked_.end() ? 0. : found->second;
    }

  private:
    int season_ = 0;                                // The season of the latest decision (0 spring .. 3 winter).
    void craft(const std::string& id, const std::string& workCell, double absoluteDay);
    // Buys up to `wanted` of an item for `id`: in its own community at the price, then from anywhere, carted in.
    // A supplier restocking (`forSupplier`) buys from workshops and producers, never from another supplier. Without
    // `credit`, only with what its till can spare.
    int buyMaterials(const std::string& id, const std::string& workCell, const std::string& item, int wanted,
                     bool forSupplier = false, bool credit = true);
  public:
    // What a workshop selling `item` keeps back of it, its common sort (doc 64, Phase 13): GoodsKept for its shelf, or,
    // when its own crafts work with it (a chandler's cord for its candles), MaterialsLow batches of each of them, the
    // point it would buy more at. Not of what rots: held back, it would only spoil.
    int keepsBack(const std::string& seller, const std::string& item) const;
    // A producer's till's wear on a tool so far, in tools (toolsWorn_: bought new when worn through).
    double toolWear(const std::string& till, const std::string& tool) const
    {
        const auto worn = toolsWorn_.find(till);
        const auto t = worn == toolsWorn_.end() ? nullptr : &worn->second;
        return t && t->count(tool) ? t->at(tool) : 0.;
    }

  private:
    // A producer's work done (`hours` of it: a paid spell is one): what it brings in from the land, a yield for every
    // crafts.json `hours` of work in hand (doc 64, Phase 3), while it has fewer than ProducerKept.
    bool produce(const std::string& id, double absoluteDay, double hours);
    std::string communityOfResident(const std::string& id) const;
    // Tops a shopkeeper's stock up to a good store of what they make things from and what they supply; returns how
    // many goods were added.
    int stockMaterials(const std::string& id);
    // What townsfolk buy (RatwDemand.cpp): whether a shop has food for sale; a hungry resident's purchase at it (enough
    // for now, and a couple of days more with a larder), returning how many it bought (and, laying in, how far its
    // household's food is still under the point it lays in at: `under`); and each household's day of errands (firewood,
    // candles, clothes... crafts.json `households`).
    bool shopHasFood(const std::string& merchant) const;
    int buyFood(const std::string& resident, const std::string& seller, bool stocking, int* under = nullptr);
    void householdShopping(std::int64_t day, int season, const std::map<std::string, LifeBody>& bodies);
    // Children's stipends (the user, 2026-10-05; RatwDemand.cpp): once a day each child living with grown family is given
    // a little. With work by the hours (doc 64, Phase 13): MostStipend at most, out of its household's want fund (the
    // one rule), as far as what the household holds over the rule's target and the tax and tithe it owes spares; none to
    // a child with a fortnight's pennies; and a child spends a little, one cheap thing a day, a bite when hungry or else
    // a trinket, kept (one lost past TrinketsKept). With the hours off (the game's default), as it was: a stipend by what
    // the household spares over two weeks' food, a penny more for every hundred, and up to half a child's pennies on
    // anything of 3p or less, kept.
    void childrenAndStipends(std::int64_t day, const std::map<std::string, LifeBody>& bodies);
    // Wants (the user, 2026-10-05; doc 64, Phases 7 and 13; doc 63's model). With work by the hours: one want a day a
    // household (each lodger its own), out of its want fund, bought by the member whose turn it is; first the pieces it
    // keeps, a rapier for the rich (the top sixth by income, holding twice a rapier's price) with none, a sword for a
    // household that works with no blade, finery while it holds fewer pieces than it has members (two more for the
    // rich), the dearest the fund runs to; all kept, and worn and mended as belongings. Only a piece its fund runs to at
    // the cheapest its town sells it for; one it could pay for and found on no shelf, its sellers hear of, and it waits
    // for it PieceWaitDays at most, no works meanwhile. With none of those to buy, works on its home with up to half of
    // what the fund holds (worksOnHome); and on a day with none (no builder had the hours, or a piece waited for), one
    // thing it fancies by its own taste, used up (a treat, a dish, never in a town gone hungry and only off a full shelf;
    // a drink; soap and scent; a pastime; something for the home). With the hours off (the game's default, the old pace),
    // as it was but one thing at a time (wantsAtTheOldPace): each grown wolf its share of the fund, on a fancy (a treat
    // any food of 3p or more; jewellery and finery among them, kept while the buyer holds FineryKept pieces or fewer).
    void wants(std::int64_t day, const std::map<std::string, LifeBody>& bodies);
    void wantsAtTheOldPace(std::int64_t day, const std::map<std::string, LifeBody>& bodies);
    static constexpr int TrinketsKept = 3, FineryKept = 6;
    // Works on the home (doc 63's model: what a household with nothing left to want has done to its home, bought by the
    // amount; RatwInstitutions.cpp): a unit is WorksHours of a building business's time and a batch of its own building
    // goods, at its town's hour and the goods' cost with the ladder's markup. With a `fund` that covers a unit, up to
    // half of it (and `most`), from WantSample of the town's `builders` by lot, the cheapest first that has the hours in
    // hand (booked into them as a mend is). Counts toward their hands (worksRate_), and the goods as its sales (so it
    // makes them again); what found no hours, a unit at most, they hear of between them. What it cost.
    std::int64_t worksOnHome(const std::string& payer, const std::vector<std::string>& builders, std::int64_t fund, std::int64_t most,
                             std::int64_t day);
    // A building trade (a carpenter, a sawmill, a mason, the brick and plaster works) that makes building goods: not
    // the smiths, tinkers and potters who make a few of them among their wares.
    bool builds(const std::string& shop) const;
    static constexpr double WorksHours = 11;
    // Households (the user, 2026-10-05; RatwHouseholds.cpp), once a day: the grown members share one purse (a shopkeeper's
    // till aside), and a household that has stayed comfortable a week keeps one of them at home (to mind the children,
    // shop and look about), sending it back to work when money runs low; a poor one sends everyone who can to work.
    void tendHouseholds(std::int64_t day, const std::map<std::string, LifeBody>& bodies);
    // Its food a day, in pennies at its town's going rate ("" the realm's).
    std::int64_t householdNeed(std::size_t members, const std::string& town = "") const
    {
        return members > 0 ? pennies(FoodADay * double(members), town) : 0;
    }
  public:
    // A wolf's food a day, as a household reckons it, in LD (5p at a 12p day; doc 64, Phase 13): pennies().
    static constexpr double FoodADay = 5 / 12.;
    // A shop's price to the townsfolk: its own (priceAt, doc 64, Phase 4), in whole pennies.
    std::int64_t shopPrice(const std::string& shop, const std::string& item) const;
    // What things cost, and prices (Docs/Design/64-economy-migration.md, Phase 4; RatwPrices.cpp). Each seller prices
    // at its normal cost times one and its margin:
    //  - a good it makes: a batch's hours at a labourer's wage (more for a skilled trade), its materials at what they
    //    cost it, carted to the bench, shared among the batch by worth; a producer's yield the same, its tools' wear
    //    counted;
    //  - a good it buys and sells on: what it paid, on average;
    //  - one with no record: its worth on the ladder at the town's labourer's wage, less the calibration's margin.
    // The margin follows its shelf, weekly, and wears back toward the ladder's 15% between (reviewMargins). A seller
    // buying from a player or a wolf pays its normal cost (its price less its margin).
    static bool costed(const std::string& account);      // Whether an account keeps what its stock cost (sellers').
    const std::unordered_map<std::string, std::unordered_map<std::string, double>>& margins() const { return margins_; }
    double costOf(const std::string& account, const std::string& item) const;   // Pennies a piece.
    // What a material cost it, or with no record what it would pay for it (the town's going price).
    double inputCost(const std::string& account, const std::string& item) const;
    double normalCost(const std::string& seller, const std::string& item) const;
    double marginOf(const std::string& seller, const std::string& item) const;
    double priceAt(const std::string& seller, const std::string& item) const;   // Pennies a piece, not rounded.
    double offerAt(const std::string& seller, const std::string& item) const { return normalCost(seller, item); }
    double labourDay(const std::string& town) const { return goingRate(town); }
    // Households and institutions (Docs/Design/64-economy-migration.md, Phase 7; doc 63's model; RatwInstitutions.cpp).
    //  - Income: what an account earned (wages, sales, profit, rent; for a town its tax, for the church its tithes and
    //    plate), a week's (taxed) and a month's (its rule's measure). Gifts, estates and relief aren't income.
    //  - Tax taxRate() (TaxRate to start, the Dungeon Master's lever) and tithe TitheRate of each resident's and House's
    //    week's income, at the reckoning.
    //  - The household rule: what its purse holds over a month's income and HomeNeedDays of its needs goes, at its own
    //    rate (HomeShareLeast..Most a month), half into a want fund (its wants, works on its home) and half to help
    //    about the home; holding over ServantMonths of its income, it keeps servants.
    //  - Institutions (a town's treasury, the church, a House) spend each week a quarter of their month's income and
    //    a share of what they hold over k months of it: the town (k 2, 0.4) and the church (k 2, 0.4) first set aside
    //    the week's relief, then their own wages, then works (materials, and crews hired for the week); a House (k 4,
    //    0.3) its retainers, finery, works and an estate crew.
    //  - Daily relief: a household short of a day's food money is given it, by the church and then its town, out of
    //    what each set aside.
    static constexpr double TaxRate = .08, TitheRate = .06, HomeShareLeast = .3, HomeShareMost = .7;
    // Lives, skills and masters (doc 64, Phase 8; doc 63's model; RatwCareers.cpp, RatwWorld.cpp's tendOldAge).
    //  - Deaths by age: a yearly chance of DeathA * e^(DeathB * years), and InfantDeath under five.
    //  - Each wolf retires at its own age, RetireLeast..RetireMost (a lot by its id): it leaves its post.
    //  - A post's trade has a level (tradeLevel: the skill its business's hardest craft needs). Learning at work, a
    //    year of it: LearnBeside beside a better hand (up to five short of it, or the level where it's there), LearnHead
    //    for a shop's head short of its level, LearnAlone otherwise; never past SkillTop.
    //  - Apprentices (every trade: the user, 2026-09-27) learn beside a qualified master on ApprenticePay of the business's
    //    wage, until they reach the trade's level (TradeSkilled for a plain trade); a masters' trade (TradeMasters)
    //    takes the young from plain work too.
    //  - Succession: the apprentice; for a masters' trade a master idle or working below its level; family or a local
    //    as skilled as the trade needs; else (the user's rule, 2026-10-14) the best-trained wolf about who isn't in a
    //    master's place or an important post, who heads it and works toward mastership.
    //  - A household of more than HouseholdMost sends a young grown member to a free home in its town.
    static constexpr double DeathA = .0004, DeathB = .085, InfantDeath = .03;
    static constexpr int RetireLeast = 58, RetireMost = 66, HouseholdMost = 6;
    static constexpr double LearnBeside = 8, LearnHead = 4, LearnAlone = .3, SkillTop = 85, ApprenticePay = .5;
    static constexpr double TradeSkilled = 30, TradeMasters = 50;
    static int retireAge(const std::string& resident);
    double tradeLevel(const Position& p) const;
    bool important(const std::string& resident) const;     // In a master's place, keeping its own shop, or the clergy.
    void splitHouseholds(std::int64_t day, const CareerWorld& world, std::vector<CareerNote>& notes);
    static constexpr int HomeNeedDays = 10, ServantMonths = 6;
    static constexpr int PieceWaitDays = 3;         // Days a household waits for a piece no shelf has (Society::wants).
    double monthIncomeOf(const std::string& account) const;
    static bool unearnedKind(const std::string& kind);
    // Carting a piece `legs` steps (ladder.json carryPerWeight: labourer's hours a unit of weight a step), in pennies.
    double carriage(const std::string& item, double legs, const std::string& town = {}) const;
    static constexpr double CarriageLegs = 10;       // Steps between towns, for what's carted from one to another.
    static constexpr double MarginLeast = -.3, MarginMost = .6, StapleMarginMost = 1.;
    static constexpr double MarginStep = .04;
    // How many days of sales a seller keeps of a good: food by how long it keeps, else by its worth.
    static double keepDays(const std::string& item);
    // Improvements to premises (RatwOddJobs.cpp): a business's level (0 to MostImprovement); each makes its batches (a
    // workshop) or its yields (a producer) ImprovementPace quicker.
    static constexpr int MostImprovement = 3;
    static constexpr double ImprovementPace = .15;
    int improvement(const std::string& positionId) const
    {
        const auto found = state_.memory.improved.find(positionId);
        return found == state_.memory.improved.end() ? 0 : found->second;
    }
    // Odd jobs (the user, 2026-10-05; RatwOddJobs.cpp): a town or church with money to spare posts a day's menial work,
    // paid when done: deliveries (food bought for the watch's mess or the church), repairs (the Town Works' materials
    // carried out and used), gathering and hunting trips (what the land gives, sold cheaply to the town's makers for
    // the poster), scouting, and a hand at a farm or other producer short of hands (a spell's yield for it). Wolves
    // without other work and children seek them out, the poor and hungry first. Kept for the day only.
    // A job takes 1 to `slots` wolves (the user: 1-5, split pay): each does its share (the trip, the carrying, the
    // mending) and is paid its share, `pay` / `slots`.
    struct OddJob
    {
        std::string id, payer, community, kind, what;
        Spot from, to;
        int slots = 1;
        std::map<std::string, int> stage, progress;  // Taker -> 0: on the way to `from`, 1: to `to`; and its spell's work there.
        std::string source, buyer, producer;
        std::size_t ground = 0;
        std::int64_t pay = 0;                        // For all its slots, a day.
        bool forChildren = true;
        bool field = false;                          // A field's hire at sowing or the harvest: the land calls (Phase 3b).
        // A business's hire runs HireDays (the user, 2026-10-05: semi-permanent work at shops that are hiring): whoever
        // took it comes back each day to it, paid each day, until then. Other odd jobs are the day's.
        std::int64_t until = -1;
        // A hire ended early (its business couldn't pay its share, or had no work for it: doc 64, Phase 13): shut for
        // the rest of the day, to its holders and to those looking, and gone with the day's.
        bool ended = false;
    };
    static constexpr int HireDays = 7;
    // A business's hire is a day's work (HireSpells at its premises), paid by the day like wages, at its town's pay for a
    // hand (Society::dayWage, doc 64, Phase 6).
    static constexpr int HireSpells = 8;           // (A day's hours, as a post's: doc 64, Phase 6. It was 4.)
    // Children's friend groups (the user, 2026-10-05): in each town its children, by age, in fours; they play together
    // and take odd jobs together. A child's group ("" for none).
    const std::string& friendGroup(const std::string& child) const;
    const std::vector<OddJob>& oddJobs() const { return oddJobs_; }
    // An odd job posted from outside the society (Docs/Design/57-changing-the-world.md: a town project's hands, kind
    // "project"): `slots` wolves for the day, `pay` for all of them, paid by `payer` as each finishes a spell's work at
    // `at`. Taken like any odd job (the jobless first). Its id, or "" if `payer` can't cover it.
    std::string postOddJob(const std::string& payer, const std::string& community, const std::string& kind, int slots,
                           std::int64_t pay, const Spot& at, const std::string& what);

    static constexpr double MostStipend = 3 / 12.;  // A child's stipend a day at most, in LD (3p at a 12p day).
    // A household is comfortable with ComfortDays of food in its purse for everyone at home, poor under PoorDays'; it keeps
    // a member at home after a week comfortable, and sends it back under KeeperDays' (and everyone to work after a week poor).
    static constexpr int ComfortDays = 28, PoorDays = 7, KeeperDays = 10, StreakDays = 7;
    static bool homemaking(const std::string& title);   // "keeps the house", "keeping the house".
    // A barracks, a bunkhouse, quarters or lodgings: many under one roof who aren't a family (RatwHouseholds.cpp).
    static bool communalHome(const std::string& home);
  private:
    // Once a game day (RatwDemand.cpp; doc 42, Phase 5): tools the trades wear out, businesses' upkeep (a stables'
    // horses), and pennies for beggars.
    void tradeUpkeep(std::int64_t day, const std::map<std::string, LifeBody>& bodies);
  public:
    // A town's own buyers (RatwDemand.cpp): the Town Works, the watch and the church of each community ("town:<it>:works"),
    // funded by the treasury, using up their baskets and buying from the town's shops; what they can't find, they ask
    // for (takeProcurements: the world posts it as a contract for goods).
    struct Procurement
    {
        std::string account, community, buyer, item;    // buyer: "the Town Works"; item: the common kind.
        int quantity = 0;
        std::int64_t price = 1;                          // A piece, at the going price (doc 64, Phase 4).
    };
    void townBuyers(std::int64_t day, const std::map<std::string, LifeBody>& bodies);
    std::vector<Procurement> takeProcurements();

    // Town purses and the month's reckoning (Docs/Design/42-money-in-circulation.md, Phase 1; RatwReckoning.cpp).
    // The capital is the community whose treasury is "treasury" (the world sets it; empty in a world of one town).
    void setCapital(std::string community) { capital_ = std::move(community); }
    const std::string& capital() const { return capital_; }
    // A community's treasury: its own store's account ("stores:<it>"), or else the capital's ("treasury").
    std::string treasuryOf(const std::string& community) const;
    // The church a treasury's townsfolk tithe to: "town:<its town>:church".
    std::string churchOf(const std::string& treasury) const;
    // Where a resident pays its tax: the treasury of the community it works in (or lives in).
    std::string treasuryOfResident(const std::string& id) const;
    // What a town's reckoning brought in (for the world's event log).
    struct Reckoning
    {
        std::string treasury, church;
        int payers = 0, residents = 0;
        std::int64_t tax = 0, tithes = 0, toCapital = 0;
        std::int64_t wealthTithe = 0, fromCapital = 0;   // Its folk's wealth tithe; what the capital sent it, for its poor.
    };
    // Money back out where it pools (Docs/Design/42, "Where money pools", the user, 2026-10-06):
    // - each week a WealthTitheShare-th of what anyone holds above a comfortable line goes to the church (wealthLine);
    // - each week the capital sends the towns half of what it holds above four weeks' spending, by how many of their
    //   folk are short of a week's food;
    // - a great house keeps only its floor and spends a HouseSurplusShare-th of the rest a day; what it holds above that
    //   floor raises the wages it pays (wageFor), rather than being given away.
    // (The banded tax and the dole went with doc 64's Phase 7.)
    static constexpr int WealthTitheShare = 20, HouseSurplusShare = 10, KeeperSurplusShare = 5;
    // Above what a wealth tithe is paid: a resident's four weeks' food; a keeper whose purse is its shop's till, that, its
    // food money and a float; a great house, its floor (HouseFloor and a float for each business: as for its surplus).
    static constexpr double HouseFloor = 100 / 12.;  // (In LD: 100p at a 12p day.)
    std::int64_t wealthLine(const std::string& id) const;
    std::int64_t houseFloor(const std::string& house) const;
    // The month's reckoning, if one is due on `day` (at most once a month): every resident who made a profit pays a
    // tenth to its town and a tenth to its church, and the towns send the capital a tenth of their tax; then new
    // books are opened. `force` reckons now (the Dev Console). Residents without books yet get them.
    void reckon(std::int64_t day, bool force = false);
    std::vector<Reckoning> takeReckonings();
    // Who pays a position's wages (doc 42, Phase 2; RatwWages.cpp): an account, and in words whom (an empty account:
    // nobody, as for a child or a farmer, who lives by what it brings in).
    struct Payer
    {
        std::string account, whom;
    };
    Payer payerOf(const std::string& resident, const Position& job, int age) const;
    // Wages by kind of post (help, guard, labour, clergy, keeper, hand, odd job): a share of the town's going rate (doc 64,
    // Phase 6), paid a PaidSpells-th a spell of work.
    double dayWage(const std::string& town, const std::string& kind) const;
    // The labour market (Docs/Design/64-economy-migration.md, Phase 6; doc 63's model; RatwLabour.cpp). Every employer
    // pays its own wage: a business (its till), and the town's watch, works and posts and the church ("<account>|<kind>").
    // A new one starts at the going rate for its skill: the labourer's wage (the middle of what the unskilled trades that
    // make things pay, the makers and the land's producers but its seasonal fields, each as a labourer's day, counted by
    // the hands its work wants up to those it has: doc 64, Phase 13), a town's own once TownRateWeight hands count in it,
    // else the realm's; times 1 + skill / skillWage, the Town Works at 0.85 of it, the clergy 1.1. Saved (memory.going).
    // Reviewed weekly on its own day: up when a post of its stood open four days or more (3% to 10% a week, the more
    // of its posts open the more, never past twice the going rate for its skill); down a little after six calm weeks
    // while wolves stand idle; sliding back while over 1.3 times it with nothing open; never under 0.85 times it; down
    // after a week it fell short paying. Its hands judge it by what it paid them lately: paid under 60% of what it owed
    // them, a wolf looks for better each morning; under 40%, it leaves (unless it's the only hand). A wolf in a post that pays
    // well enough is drawn off by a hire paying a tenth more on 4% of mornings, unless its employer is short of hands
    // or it's the only hand. A post left is open again at once (tendCareers fills it); a post whose holder is away on
    // another business's hire is open while it's away, unless its business has no work for it (Phase 13).
    std::string employerKey(const Payer& payer, const Position& job) const;
    double employerWage(const std::string& key) const;
    double goingRate(const std::string& town) const;          // A labourer's day in the town (labourDay).
    double employerSkill(const std::string& key) const;       // The skill its trade needs.
    int vacanciesOf(const std::string& key) const;
    int handsOf(const std::string& key) const;
    double paidLately(const std::string& resident) const;     // The share of its wages owed it lately that were paid.
    // What a wolf's own post weighs against a hire this morning (0: it isn't looking).
    std::int64_t lookingFor(const std::string& resident, const Payer& payer, const Position& job, double day) const;
    static constexpr double WageUp = .03, WageUpShort = .10, WageDown = .02, WageFloor = .85, WageMost = 2, WageHigh = 1.3,
                            LeaveBelow = .4, LookBelow = .6, PoachGap = .1, PoachChance = .04, WorksShare = .85, ClergyShare = 1.1;
    static constexpr int CalmWeeks = 6, OpenDays = 4, TownRateWeight = 40;
    // Hands by sales (doc 64, Phase 6), at its weekly review: the hours its sales take a day (its crafts' or yields'
    // hours for what it sells, a fifth more for staples, half again with its shelf near bare, a fifth less with it
    // glutted), in hands of PaidSpells hours; at least its keeper. Short, it opens the posts it let go, then hires for
    // HireDays, the day it's short (a farm a quarter over its wage at sowing and harvest, from the idle, other trades
    // and hands whose business has no work for them: the harvest call); over, at its weekly review, it lets its hires
    // run out, then lets help go (half the excess, one at least), keeping their posts empty. Never more than doubling
    // its hands (and two) at once. (Phase 13:) a craft by its first good, a good several recipes make once; a craft
    // short of materials lately no more than MadeSlack over what it could make; a business with more work in hand than
    // its hands use hires none, and its hires end; one that makes nothing (a stall) neither hires nor lets go.
    static constexpr double StapleSlack = 1.2, HarvestPay = 1.25, MadeSlack = 1.1;
    std::vector<std::string> helpPosts(const std::string& till) const;   // Its help's posts (at its premises).
    // (`bare`: half again for a shelf near bare, as its maker plans; without, its sales as they are: the pace's measure
    // of the land, whose producers' shelves are always near bare.)
    int handsWanted(const std::string& till, bool bare = true) const;
    bool idleHand(const std::string& resident, const Payer& payer) const;   // Its business has no work for it.
    // A business whose hands have more work in hand than it can use (its bank full at this turn of the day and the last:
    // its shelves full, its materials short; a producer's store full), or a field with none in its window (Phase 13).
    bool idleBusiness(const std::string& till) const;
    bool fieldIdle(const std::string& till, const FieldYear& f, const items::Producer& producer) const;   // (Its field's.)
    // A business whose hands make something: one with crafts (and so its mending), or a producer; not a stall, whose
    // help sells and whose hours are no work in hand (Phase 13).
    bool makesThings(const std::string& till) const;
    std::string wageKind(const Payer& payer, const Position& job) const;   // "help", "guard", "labour", "clergy".
    // A living for every grown wolf (doc 42, Phase 3): one out of work labours for the Town Works; at RetireAge, retired.
    static constexpr const char* LabourTitle = "labouring for the Town Works";
    static constexpr int PayrollDays = 7;          // Days of wages a town's treasury keeps before funding its buyers (a
                                                   // week's: the market dues come in weekly).
    // Spells of paid work a day (a spell: a game hour at the post): paid for the hours worked, up to a full day's
    // (the user, 2026-10-05: wages must cover a day's living; it was 3, a third of the day). A placeholder.
    static constexpr int PaidSpells = 8;
    static constexpr int RetireAge = 65;
    // Working out of town (doc 42, Phase 3b): the ground a grown wolf without a post works today, or null (it labours
    // for the Town Works instead). A ground takes OutworkRoom wolves; the choice is the wolf's own, steady day to day.
    const WorkGround* outworkOf(const std::string& resident, const std::string& community) const;
    // outworkOf without choosing one: `known` is false while this resident hasn't been given its ground today.
    const WorkGround* outworkKnown(const std::string& resident, const std::string& community, bool& known) const;
    void rollOutwork() const;                       // A new day: everyone chooses its ground afresh.
    static constexpr int OutworkRoom = 2;
    static std::string outworkTitle(const std::string& trade);   // "gathering in the wild", "hunting"...
    static bool outworkTitled(const std::string& title);
    static bool idlePost(const std::string& title);   // "idling in the square", "sits and watches": no work at all.
    // Sells what a wolf brought in to a shop that buys it, at the price a player gets; returns the pennies it made.
    std::int64_t sellBroughtIn(const std::string& resident, const std::string& shop);
    // A wolf's catch from the wild for its own household's larder, up to LarderDays of a wolf's day's food
    // (DayNourishment) a head (doc 64, Phase 10); returns the nourishment put away.
    int keepForTheLarder(const std::string& resident);
    static constexpr int LarderDays = 3, DayNourishment = 47;
    bool carriesForSale(const std::string& resident) const;
    bool sellsTo(const std::string& resident, const std::string& shop) const;   // The shop takes something it carries.
    // Great houses (doc 42, Phase 5b; RatwHouses.cpp): the houses, each business's till (the keeper itself unless a house
    // owns it), and the house that owns a position's business ("" for none).
    const std::vector<House>& houses() const;
    std::string tillOf(const std::string& keeper) const;
    std::string ownerOf(const std::string& positionId) const;
    // A manager's wage a day, the days of running costs a till keeps (its float), and how many of the last 28 days a
    // till may run low (propped up by its house, or not) before the house sells the business on. Placeholders. The
    // wages, and a float's help and materials, in LD (8p, 6p and 30p at a 12p day: floatOf).
    static constexpr double ManagerWage = 8 / 12., HelpWage = 6 / 12., FloatMaterials = 30 / 12.;
    static constexpr int FloatDays = 3, ProppedDays = 10;
    // A great house's fortune in an older world (before the realm's money stood: doc 64). Rent is RentShare now.
    static constexpr std::int64_t HouseFortune = 3000;
    std::int64_t floatOf(const std::string& positionId) const;
    // Once a game day: the managers' wages, the takings above each till's float to its house, a struggling till propped
    // up, and a business that keeps losing sold to another house. Gives the houses their businesses the first time.
    void tendHouses(std::int64_t day);
    // Every business its own till (doc 46, Phase 2; RatwTills.cpp): an owner-run shop, workshop, farm or site keeps
    // "till:<position>" apart from its keeper's purse, as a house's business does (tillOf finds either). Its keeper is
    // paid its business's wage and its owner the month's profit (doc 64, Phases 5 and 6). Founded when a world first runs
    // (an older save: the first time it runs with tills): its keepers move their shop's goods and money (all but a
    // month's living) into it once (TillsFounded). A great house that takes a business on later (foundHouses, a sale)
    // takes its till with it.
    static constexpr int OwnersShare = 3, TillsFounded = 1;
    bool ownsTill(const Position& p) const;         // A business that keeps a till of its own (not a house's).
    void indexTills();                              // tills_ from the accounts (a reset, a restore).
    std::string ownTill(const std::string& positionId) const;   // Its till ("" for none, or a house's business).
    void foundTills();
    void tendTills(std::int64_t day);               // Daily: the keepers' wages, then the owners' business (tendOwners).
    // Businesses and owners (Docs/Design/64-economy-migration.md, Phase 5; doc 63's model; RatwTills.cpp). A business
    // keeps its working money: a month of its staffing (its keeper, its help, and up to two of its posts standing open,
    // at the town's wages) and of the materials its sales use. On its own day of the month the rest goes to its owner
    // (its House, or its keeper). On its own day of the week a business on a House's land pays it RentShare of the
    // week's takings, and one short of a week's working money is topped up by its owner from what it can spare; an
    // owner stops after putting in two months' working money that hasn't come back. A business short of money buys its
    // materials on credit, up to a week of its turnover, repaid first from its takings and written off by the seller
    // after CreditWrittenOff days.
    static constexpr double RentShare = .15;
    static constexpr int CreditDays = 7, CreditWrittenOff = 56;
    std::int64_t workingMoney(const std::string& positionId) const;
    // A seasonal field's hands' wages for its window's work still ahead (doc 64, Phase 13): kept in its working money
    // from a month before the window opens to its end, so the land pays the hands it hires when they're wanted.
    double fieldWages(const std::string& till) const;
    // What a till sells of each good a day (its running average, missed sales counting) and its standing orders' cost a
    // week: what its working money is reckoned from (for econ_watch).
    const std::unordered_map<std::string, double>* sellRates(const std::string& till) const
    {
        const auto found = sellRate_.find(till);
        return found == sellRate_.end() ? nullptr : &found->second;
    }
    std::int64_t ordersCostOf(const std::string& till) const
    {
        const auto found = ordersCost_.find(till);
        return found == ordersCost_.end() ? 0 : found->second;
    }
    // A business's own day of the month (its profit to its owner) and of the week (rent, top-ups): 0-based.
    static int ownersDay(const std::string& positionId) { return int(std::hash<std::string>{}(positionId + "|owner") % MonthDays); }
    static int ownersWeekday(const std::string& positionId) { return int(std::hash<std::string>{}(positionId + "|owner") % 7); }
    std::string ownerOfBusiness(const std::string& positionId) const;   // Its House, or its keeper ("" for none).
    std::string landlordOf(const std::string& positionId) const;        // The House whose land it works, or "".
    // What the materials (or, bought to sell on, the goods themselves) of one piece cost a seller, without its hours.
    double materialsCost(const std::string& seller, const std::string& item) const;
    std::int64_t creditLeft(const std::string& till) const;              // What it may still owe its suppliers.
    std::int64_t owed(const std::string& till) const;
    // Goods bought on credit: the seller's stock to the buyer at `price` a piece, owed rather than paid.
    bool onCredit(const std::string& seller, const std::string& buyer, const std::string& item, int quantity, std::int64_t price);
    // A business changes hands (the Dungeon Master, or a sale): to `house`, for `price` from it to the old owner.
    bool sellBusiness(const std::string& positionId, const std::string& house, std::int64_t price);
    // The rule against hoarding (doc 42; RatwSurplus.cpp): once a game day, each town treasury, church and great house
    // spends a SurplusShare-th of what it holds above its reserve (four weeks of its usual spending, at least a floor),
    // hiring hands, buying and using up goods, giving alms. What each spent, for the event log:
    struct Spending
    {
        std::string collector, community;
        std::int64_t total = 0;
        std::string detail;
    };
    static constexpr int SurplusShare = 10;
    // TRIAL (doc 42, "Pressure", 2026-10-06; temporary): whether a named trial is on, from the RATW_TRIAL environment
    // variable (names, comma separated), for econ_watch runs comparing ways of keeping money moving. Off in play.
    static bool trial(const char* name);
    std::int64_t landMedian_ = 0;                   // The land's median resident purse (tendPrices, daily).
    void spendSurpluses(std::int64_t day);
    std::vector<Spending> takeSpendings();
    // A town's buildings falling into disrepair (under 50) or mended again, since the world last asked.
    struct TownNews
    {
        std::string community, what;                // what: "disrepair" or "mended".
    };
    std::vector<TownNews> takeTownNews() { std::vector<TownNews> out; out.swap(townNews_); return out; }
    // A town mended by other hands than the Town Works' (Docs/Design/57-changing-the-world.md: a town project's work):
    // `points` more repair, its news when it crosses half, as the Town Works' own mending makes.
    void mendTown(const std::string& community, double points);
    // Town projects' accounts and their towns (doc 57), for counting players' gifts to them as help to that town.
    void setProjectTowns(std::map<std::string, std::string> towns) { projectTowns_ = std::move(towns); }
    double condition(const std::string& community) const
    {
        const auto found = state_.memory.condition.find(community);
        return found == state_.memory.condition.end() ? 100 : found->second;
    }
    // Restday's service (doc 42, Phase 6): from ServiceStart to ServiceEnd a third of the town (a steady per-wolf share,
    // different week to week) sits in its church while its clergy preach; the plate goes round as it ends.
    static constexpr double ServiceStart = 9, ServiceEnd = 11;
    static bool goesToChurch(const std::string& resident, std::int64_t day);
    static bool clergy(const std::string& title);     // A priest's, a chapel keeper's, an acolyte's work.
    static bool houseHead(const std::string& title);  // The head of a great house: "ruling House Fell".
    static constexpr int TaxShare = 10, TitheShare = 10, CapitalShare = 10;     // "a tenth": 1 / these.
    // Starting money (the user, 2026-10-05; RatwFounding.cpp): every resident and every body that keeps a purse begins with
    // a reasonable one, made once when the world is founded (an older save: the first time it runs with this). Placeholders:
    // a child ChildPurse and half a penny a year of its age; a beggar PoorPurse; a grown wolf AdultPurse (some five days'
    // food); one retired ElderPurse; a shopkeeper whose shop is its own KeeperPurse, or its shop's float if more. A town's
    // treasury TreasuryHead a resident who pays it, its church ChurchHead (or four weeks of its basket, if more), a town's
    // buyers their working funds, a great house's business its till's float. In pennies as written: in a realm of
    // WorldMoneyResidents they are only the seed WorldMoney makes up, each kind to its share, so the founding's spread is
    // the model's whatever the day (doc 64, Phases 1 and 13). A child's first pennies at its birth are ChildPurse at the
    // going rate (atGoingRate).
    static constexpr std::int64_t ChildPurse = 5, PoorPurse = 10, AdultPurse = 30, ElderPurse = 60, KeeperPurse = 80;
    static constexpr std::int64_t TreasuryHead = 40, ChurchHead = 10;
    // One church across the land (the user, 2026-10-06): every town's church keeps one purse, shared at once, so what
    // one takes in another can give (churchOf). Its floor is ChurchHead for every resident in the land. Each town's
    // church still works for its own town: its alms, odd jobs, candles and clergy.
    // Its goods (candles, bread for alms, bandages) are each town's church's own, in its storehouse (churchStore): bought
    // at that town's shops with the shared money, given and used there.
    static constexpr const char* SharedChurch = "town:all:church";
    static std::string churchStore(const std::string& community) { return "town:" + community + ":church"; }
    // Older saves' churches each kept money of their own: it goes into the shared purse (on the next day), their goods
    // staying in their storehouses.
    void joinChurches();
    // The realm's money as it stands on its first day (Docs/Design/64-economy-migration.md, Phase 1; the user,
    // 2026-10-15: 5,000,000p in whole pennies). Once the purses are made, what the realm holds is made up to WorldMoney,
    // each kind of holder to its share, as doc 63's model holds it: households WorldHomesShare percent (unevenly, as
    // households are), businesses WorldBusinessShare (by their floats), the great houses WorldHousesShare (by what they
    // own: old families, theirs from before the story starts), the church WorldChurchShare, the treasuries the rest
    // (by their residents). Only a land of WorldMoneyResidents or more: a village made for a test keeps its own starting
    // money. After this nothing is made but a newcomer's purse (NewcomerWageDays of its town's labourer's wage) and,
    // later, the mint's (doc 64, Phase 11).
    static constexpr std::int64_t WorldMoney = 5000000;
    static constexpr std::size_t WorldMoneyResidents = 500;
    static constexpr int WorldHomesShare = 40, WorldBusinessShare = 15, WorldHousesShare = 30, WorldChurchShare = 5;
    static constexpr int NewcomerWageDays = 2;
    static constexpr int PursesFounded = 2;          // (1: the purses; 2: the realm's money as it stands.)
    std::int64_t startingPurse(const std::string& resident, int age) const;
    // A keeper whose shop is its own (its purse is the till) never spends its last KeeperReserve on the business
    // (materials, restocking, help's wages): it keeps its food money (the user, 2026-10-05). What an account may spend so.
    // In LD (20p at a 12p day: pennies()).
    static constexpr double KeeperReserve = 20 / 12.;
    std::int64_t spendable(const std::string& account) const;

  private:
    std::vector<OddJob> oddJobs_;
    std::int64_t nextOddJob_ = 0;
    // An index of the odd jobs: who holds one (resident -> its place in
    // oddJobs_), each town's (places, in order), and the farms' hires (anyone from a city may take one). Remade when the
    // list changes (oddVersion_, moved on wherever a job is posted or the day's are cleared), before residents decide.
    std::unordered_map<std::string, std::size_t> oddHeld_;
    std::unordered_map<std::string, std::vector<std::size_t>> oddByTown_, oddHiresByTown_;   // (And its businesses' hires.)
    std::vector<std::size_t> oddFarmHires_;
    std::uint64_t oddVersion_ = 1, oddIndexed_ = 0;
    void indexOddJobs();
    // What each shop's till took in today (wages follow takings: RatwSurplus.cpp). Not saved.
    std::map<std::string, std::int64_t> takings_;
    std::unordered_map<std::string, std::int64_t> weekTakings_;   // A till's takings since its rent was last paid (doc 64).
    // The labour market's day (doc 64, Phase 6; RatwLabour.cpp): each employer's posts open and filled, and its town;
    // each town's going rate; the idle share of grown wolves; each wolf's pay today and lately, and whose it is.
    std::unordered_map<std::string, int> vacancies_, hands_;
    std::unordered_map<std::string, std::string> employerTown_, payKey_, postKey_;   // (postKey_: a post -> its employer.)
    std::unordered_map<std::string, double> paidToday_, owedToday_, pay7_;   // (The going rates: memory.going.)
    double slack_ = 0;
    void tendLabour(std::int64_t day, const std::map<std::string, LifeBody>& bodies);
    // Households and institutions (doc 64, Phase 7; RatwInstitutions.cpp).
    std::unordered_map<std::string, double> earnedToday_;   // What each account earned today (into memory.weekIncome).
    void tendIncome();                                      // Daily, first: today's earnings into the week's and month's.
    void flushEarnings();                                   // (Those so far, without the month's daily decay.)
    void institutionsWeek(std::int64_t day);                // At the reckoning: each institution's week's budget, spent.
    void dailyRelief(std::int64_t day, const std::map<std::string, LifeBody>& bodies);
    // `lodger`: one wolf of a barracks, a bunkhouse, quarters or lodgings, its own household by its own purse (`home` its
    // ID): all its spare to its wants, no help about the home or servants for a bunk.
    void householdRule(const std::string& home, const std::vector<std::string>& members, std::int64_t pot, std::int64_t day,
                       bool lodger = false);
    void leftovers(std::int64_t day);                       // Bakers' bread that would spoil overnight, to the hungry.
    // Hands hired by the week (an institution's works, masons, retainers; a household's servants): `slots` hands at
    // `wage` a day each, for `days`, at `at`. Returns what a day of them costs.
    std::int64_t hireForWeek(const std::string& payer, const std::string& community, const std::string& kind, const std::string& what,
                             const Spot& at, int slots, std::int64_t wage, int days, std::int64_t day);
    Spot townSpot(const std::string& community, const std::string& salt) const;   // Somewhere public in town.
    // Spends up to `budget` of `payer`'s on goods its town's shops sell that `is` picks, used up ("used in <what>") or
    // kept; returns what it spent.
    std::int64_t buyAndUse(const std::string& payer, const std::string& community, const std::function<bool(const std::string&)>& is,
                           std::int64_t budget, const std::string& what, bool keep);
    void reviewHands(const std::string& till, std::int64_t day, bool weekly, int wanted);   // (wanted: handsWanted's.)
    void notePay(const std::string& resident, const std::string& key, std::int64_t coins, std::int64_t owed);
    void tendOwners(std::int64_t day);
    std::map<std::string, std::string> friendGroups_;   // Child -> its group (rebuilt each day: childrenAndStipends).
    // Farmhands (doc 42, the user, 2026-10-06): every farm, orchard and vineyard hires hands for a few days when it has the
    // money, from its town or from any city; one from elsewhere lodges in the farm's bunkhouse (BunkBeds), fed from its
    // larder, which the farm stocks. A city: a community of CityResidents or more. (With work by the hours, every
    // producer's hire is the land's, a farm's or not: doc 64, Phase 13.)
  public:
    bool farmHire(const OddJob& j) const;          // A hire posted by the land's (a farm, or with the hours any producer).
  private:
    static constexpr int BunkBeds = 10;
    static constexpr std::size_t CityResidents = 150;
    std::map<std::string, Spot> bunkhouses_;
    std::set<std::string> cities_;
    void postFarmHires(std::int64_t day);
    void lodgeHand(const std::string& id, const OddJob& job);
    void endLodgings(std::int64_t day);
    void stockBunkhouses(std::int64_t day);
    // A farm's upkeep (crafts.json `farmUpkeep`): what it wears out a day, bought in its town, the fractions owed.
    std::unordered_map<std::string, std::map<std::string, double>> upkeepOwed_;
    // A producer's till -> its tools' wear (crafts.json `tools`), in tools, by its hours of work (doc 64, Phase 3b).
    std::unordered_map<std::string, std::map<std::string, double>> toolsWorn_;
    // A maker's till -> its mending, hours a day (a month's running average), toward its hands (Phase 9); a builder's,
    // its works on homes (Phase 13). Kept in the saved memory daily, and back from it on a restore (keepRates,
    // loadRates): a restarted world's builders keep their posts.
    std::unordered_map<std::string, double> mendRate_, worksRate_;
    void keepRates();
    void loadRates();
    std::unordered_map<std::string, double> hoarderSpent_;   // A hoarder's spending today (Phase 11's mint).
    std::unordered_map<std::string, double> wageCarry_;   // Worker -> the fraction of a penny its wages owe it (not saved).
    // Each Restday, every improved business keeps its premises up (the user, 2026-10-05): UpkeepALevel a level a week in
    // materials bought from its town's makers, from its till (or its house). Two weeks without, and it loses a level.
    void keepUpPremises();
  public:
    static constexpr double UpkeepALevel = 5 / 12.;   // (In LD: 5p at a 12p day.)
  private:
    // The odd job this resident holds, claiming one if it is free to (nullptr for none). With `claims`, nothing is claimed:
    // it is set if one would be (deciding at once, RatwResidents.cpp).
    const OddJob* oddJobFor(const std::string& id, const Position& job, bool jobless, int age, bool poor, double hour,
                            std::int64_t ownDayPay = 0, bool* claims = nullptr);
  public:
    // A craft's or a producer's hours of work now (the old pace, or with work by the hours the realm's pace toward the
    // calibrated: hoursFor), for work done outside the society's own businesses (a Hold's workshops, doc 64, Phase 11).
    double workHours(const items::Craft& k) const { return hoursFor(k); }
    double workHours(const items::Producer& p) const { return hoursFor(p); }
    // A producer's tools worn by `hours` of its work (doc 64, Phase 3b; Society::produce), bought new when worn through.
    void wearTools(const std::string& till, const std::string& producer, double hours);
    // Called to the fields this morning at sowing or the harvest (Phase 3b), LandCall of the hands of trades that aren't
    // staples a day: it may take a field's hire whatever it pays.
    bool calledToTheLand(const std::string& resident, const Position& job, int age, std::int64_t day) const;
    static constexpr double LandCall = .1;
  private:
    // Arrived where its odd job takes it: the work done there, and paid when finished.
    void advanceOddJob(const std::string& id, int seconds = 1);   // (A step's seconds of its work.)
    // Made into `account` as starting money: counted as made, and kept out of the month's profit.
    void startingMoney(const std::string& account, std::int64_t coins);
    // The realm's labourer's day as it is founded (doc 64, Phase 13): in a land of WorldMoneyResidents or more, whose
    // money is made up to WorldMoney, the going rate starts at ladder.json's `foundingDay` (the realm's and every town's,
    // memory.going), so its tills' floats, its first wages and employers' books and its prices start there. Before its
    // purses are made, once; a village made for a test keeps the catalogue's day.
    void foundWage();
    // Tops every purse up to its starting money, once (EconomyMemory::purses).
    void foundPurses(const std::map<std::string, LifeBody>& bodies);
    // Makes what the world holds up to WorldMoney (once, at the end of the first day's pass after the purses are founded).
    void worldMoney(const std::map<std::string, LifeBody>& bodies);
    std::string capital_;
    bool tradeByCaravan_ = false;
    std::map<std::string, std::int64_t> spentToday_;       // Collector -> its ordinary spending today (not saved).
    std::int64_t surplusDay_ = -1;
    std::vector<Spending> spendings_;
    std::vector<TownNews> townNews_;
    void noteOutgoing(const std::string& from, const std::string& kind, std::int64_t coins);
    void noteIncoming(const std::string& to, const std::string& kind, std::int64_t coins);
    std::map<std::string, std::int64_t> incomeToday_, wagesToday_;
    // Buys what `wanted` from what the shops sell (onSale), up to `budget`, never a shop's last few; what was got, by item.
    std::int64_t buyForSurplus(const std::string& buyer, const std::vector<std::string>& shops,
                               const std::function<bool(const std::string& item)>& wanted, std::int64_t budget,
                               const std::string& kind, std::map<std::string, int>* got, bool lastToo = false);
    // Whether a shop sells a good it holds (doc 64, Phase 13): its wares, and food at a food shop (as the hungry buy it,
    // sellsFood); never what it holds to make things from (a provisioner's rabbits for smoking: its own crafts'
    // materials, but for those among its wares). `sold`: its wares.
    bool onSale(const std::string& shop, const std::string& item, const std::vector<std::string>& sold) const;
    // A shop's wares as wares() keeps them, not a copy (`made`, where it keeps none).
    const std::vector<std::string>& waresKept(const std::string& shop, std::vector<std::string>& made) const;
    // What townsfolk buy one at a time (doc 64, Phase 13; the model's buy, from the cheapest of a few sellers). A town's
    // goods on sale (onSale) of what `wanted` picks, past a shelf's last few, in an order by lot (`salt`): worked out
    // once for a day's buying. buyOne takes, from the buyer's own place in that order (by lot), the first WantSample
    // still on the shelf that `budget` and its purse run to, and buys the cheapest of them (with `dearest`, the dearest);
    // with `fullShelf`, only off a shelf holding its days' keeping of its sales (a treat: never what the hungry will come
    // for). The item got, or "".
    struct Offer
    {
        std::string shop, till, item, base;
    };
    std::vector<Offer> offersOf(const std::vector<std::string>& shops, const std::function<bool(const std::string& item)>& wanted,
                                const std::string& salt) const;
    std::string buyOne(const std::string& buyer, const std::vector<Offer>& offers, std::int64_t budget, const std::string& kind,
                       const std::string& salt, bool dearest = false, bool fullShelf = false);
    static constexpr std::size_t WantSample = 4;
    mutable std::vector<House> houses_;                     // From the heads' positions (houses(), built on first use).
    mutable bool housesKnown_ = false;
    void foundHouses();
    // The house a business belongs to (doc 61): the one named in it, else the one whose trade it is, else none (its
    // keeper's own); houses with no trade (a world without houses.json) own industry, and half the shops, by lot.
    std::string ownerFor(const Position& business, const std::vector<const House*>& townHouses) const;
    std::string housePlan() const;                  // The rules ownership is given out by now (HouseState::plan).
    bool ownTrade(const std::string& positionId, const std::string& houseId) const;   // Of the house's trade (doc 61).
    void sortHouses();                              // Gives a save's businesses out again by the rules of now.
    mutable std::map<std::string, std::size_t> outwork_;   // Resident -> its ground's index in its community's list.
    mutable std::map<std::string, std::map<std::size_t, int>> outworkTaken_;   // Community -> ground -> how many.
    mutable std::int64_t outworkDay_ = -1;
    std::set<std::string> offered_;                        // Who has put something on the plate this Restday.            // Worker -> the day its employer first couldn't pay (not saved).
    mutable std::unordered_map<std::string, std::vector<std::string>> employers_;   // Work cell -> shops' and houses' positions.
    mutable std::map<std::string, std::string> richest_;    // Home cell -> its household's richest (for the day).
    mutable std::int64_t richestDay_ = -1;
    const std::string* richestAt(const std::string& homeCell, const std::string& besides) const;
    void indexEmployers() const;
    std::vector<Reckoning> reckonings_;                    // Since the world last took them.
    std::vector<Procurement> procurements_;                // Asked for since the world last took them.
    std::map<std::string, double> owed_;                   // "account|item" -> a buyer's use not yet taken from stock.
    // The monitor's side of the society (RatwOrchestrate.cpp): the measures taken at the last snapshot, waiting for the day
    // to turn, the day the last snapshot was for, and what it counts between snapshots (coins between towns, what
    // residents earn).
    std::optional<std::pair<orchestra::Brief, orchestra::Memory>> measured_;
    std::unordered_map<std::string, std::string> tills_;   // Position -> its business's till ("till:<position>"), for tillOf.
    bool keepBriefs_ = false;
    std::int64_t snapshotDay_ = -1, countingFrom_ = -1;   // (countingFrom_: the day counting began.)
    std::int64_t reckonedDay_ = -1;                  // The day of the last weekly reckoning (taxes and tithes).
    std::unordered_map<std::string, std::string> accountTown_;   // Account -> its town, for the coins between towns.
    std::map<std::string, std::pair<std::int64_t, std::int64_t>> townFlow_;   // Town -> coins in, out.
    struct Earned
    {
        std::int64_t day[7] = {-1, -1, -1, -1, -1, -1, -1}, coins[7] = {};
    };
    std::unordered_map<std::string, Earned> earned_;             // Resident -> what it earned on each of the last 7 days.
    std::vector<orchestra::Brief> briefs_;
    bool orchestrating() const;
    void forgetOrchestra();                          // A new or restored society: no snapshot taken, nothing counted.
    bool wantsSnapshot(double absoluteDay) const;
    void orchestrate(double absoluteDay, const std::map<std::string, LifeBody>& bodies);
    orchestra::Snapshot orchestraSnapshot(std::int64_t forDay, const std::map<std::string, LifeBody>& bodies);
    void noteForOrchestra(const std::string& kind, const std::string& from, const std::string& to, std::int64_t coins,
                          const std::string& item = {}, int quantity = 0);
    // Players' help, by town, since the last snapshot (doc 57, 5): coins in, coins out, goods at the town's price.
    struct PlayerFlow
    {
        std::int64_t in = 0, out = 0, goods = 0;
    };
    std::map<std::string, PlayerFlow> playerFlow_;
    std::map<std::string, std::string> projectTowns_;   // A town project's account -> its town (the game's).
    void record(const std::string& kind, const std::string& from, const std::string& to,
                const std::string& item, int quantity, std::int64_t coins);
    bool transfer(const std::string& seller, const std::string& buyer, const std::string& item,
                  int quantity, std::int64_t price, const std::string& kind);
    void decide(double absoluteDay, int season, const std::map<std::string, LifeBody>& bodies);
    // Authored-world routines, defined in RatwResidents.cpp.
    void resetAuthored();
    void decideAuthored(double absoluteDay, const std::map<std::string, LifeBody>& bodies);
};
} // namespace ratw
