#pragma once

#include <cmath>
#include <cstddef>
#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "RatwAppearance.h"
#include "RatwOrchestrator.h"

namespace ratw
{
namespace items
{
struct Business;
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
    // Where a spot is on the map, in tiles: its cell's place there (an outdoor cell's corner, an interior's footprint) and
    // its own position on it; false where the cell isn't known. How far a wolf lives from a field's hire, straight
    // across (doc 64, Phase 13, round 9): no path is searched. (Null: a society alone.)
    std::function<bool(const Spot& at, double& x, double& y)> onMap;
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
// A lone owner's business short of its wages (doc 64, Phase 13, round 9: Society::tendOwners): its owner's weekly looks
// in a row that found it holding less than a week's wages, the day its owner stopped topping it up (-1: it hasn't), and
// the last day its till couldn't pay its keeper's wage.
struct ShortTill
{
    int weeks = 0;
    std::int64_t stopped = -1, unpaid = -1;
    std::int64_t idle = -1;                          // The day it went idle (round 11, IdleTills), -1 while it trades on.
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
    // "till|good" -> the days a seller's good has stood over its shelf target, its markdown (Society::reviewMarkdowns;
    // round 11, lane 3). Kept daily with the prices.
    std::map<std::string, int> markdowns;
    // Businesses and owners (doc 64, Phase 5): what each business's owner has put into its till lately (till -> pennies,
    // a quarter's worth, wearing away by a thirteenth a week and paid back by its profits), and trade credit ("buyer's
    // till|seller's till" -> what it owes and the day it first owed it).
    std::map<std::string, double> putIn;
    std::map<std::string, std::pair<std::int64_t, std::int64_t>> credit;
    // A lone owner's business that can't pay its wages (doc 64, Phase 13, round 9; with work by the hours): its till ->
    // its owner's weekly looks running that found it short of a week's wages, the day its owner stopped topping it up
    // (-1 while it still does), and the last day it couldn't pay its keeper its wage.
    std::map<std::string, ShortTill> shortTills;
    std::map<std::string, EmployerBook> employers;   // Employer (Society::employerKey) -> its book (doc 64, Phase 6).
    // A business's trade lately (round 12, lane A, WageBySales; with work by the hours): its till -> what its sales took
    // less what it paid for goods and materials (on credit too), a day, a running mean over its first SalesDays days and
    // then a SalesDays-day running average; and the days counted (to SalesDays). What its weekly review sees its wage
    // bill paid from.
    std::map<std::string, std::pair<double, int>> tradeNet;
    std::map<std::string, double> going;             // Town ("" the realm) -> its going rate, as last measured (Phase 13).
    // The realm's labourer's day when its money was founded (Society::foundWage: ladder.json's foundingDay); 0 for a
    // realm founded before Phase 13 (at the catalogue's 12p) or a village. The amounts' day at the seconds' pace.
    double foundedDay = 0;
    std::set<std::string> closed;                   // Posts a business has let go and keeps empty (doc 64, Phase 6).
    double pace = 0;                                 // The realm's way to the calibrated hours, 0 to 1 (Society::PaceStep).
    // The realm's food in hand at the day passes of the last week, days a head, each with its day (a day it missed has
    // none: the week is still a week).
    std::vector<std::pair<std::int64_t, double>> paceCover;
    // The land's hours worked (its producers' but the seasonal fields', their hands' with their own) at the day passes of
    // the last fortnight, each with its day: the pace's brake (Society::PaceLandSlack; doc 64, Phase 13).
    std::vector<std::pair<std::int64_t, double>> paceLand;
    // The land's hours credited since the last day pass, as they are credited (Society::produce, a hand's hire): the
    // brake's next reading. -1 until the first pass counts them (a new world, a save from before round 6 of Phase 13).
    double landToday = -1;
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
    double holdingsTax = .01;                        // The holdings tax, a week's (Society::HoldingsTaxRate to start): the
                                                     // Dungeon Master's lever (round 11, Society::setHoldingsTax).
    // Households and institutions (doc 64, Phase 7): each account's earned income this week (taxed at the reckoning)
    // and over the last month (a running sum, a 28th off a day); each home's want fund and help budget; each
    // institution's relief set aside for the week, its relief given this week, and the wages it paid this week.
    std::map<std::string, double> weekIncome, monthIncome, wantFund, helpBudget, reliefLeft, reliefWeek, wagesWeek;
    // The capital's buyers' funds inside its week's budget (doc 64, Phase 13, round 6; with work by the hours): what is
    // left of the week's allowance for them, and what they drew this week (Society::institutionsWeek, townBuyers).
    std::map<std::string, double> fundsLeft, fundsWeek;
    // A town's caravans' net takings since its last week's budget (round 9, the user, 2026-10-27; with work by the hours):
    // spent in its next week's budget on top of its tax's, as they come. Below nothing (road money out, the takings not
    // yet home), it stays to be netted against the takings when they come.
    std::map<std::string, double> caravanWeek;
    // What a town's own posts and relief came to over its week's tax budget, not yet made good by its caravans' takings
    // (round 9, the user, 2026-10-27: the caravans pay its posts first, its works only with what's left; with work by the
    // hours). Its works see its caravans' money only once this is paid.
    std::map<std::string, double> postsOwed;
    // A town's tax eased while its stores hold over their target (round 10, the user, 2026-10-27; with work by the hours;
    // Society::easeTownTax): its rate as a share of the realm's (1 unless kept here, 0 at the least), its stores' mean
    // holding (a week's, day by day), and a month of what its tax would bring at the full rate.
    std::map<std::string, double> townTax, heldMean, taxBase;
    // A town's caravans' road money inside its week's budget (round 10, with TownPosts): what is left of the week's
    // allowance for it, and what they drew this week (Society::institutionsWeek, Society::roadMoneyFor).
    std::map<std::string, double> roadLeft, roadWeek;
    // The small towns' Town Works' day labour the capital paid since its last week's budget (CapitalLabour; the user,
    // 2026-10-28): counted in its next week's budget before its posts ("treasury").
    std::map<std::string, double> labourWeek;
    // Round 11, lane 4 (the user, 2026-10-29; with work by the hours): what is left of the week's allowance for day labour,
    // the capital's for the small towns' it carries ("treasury", CarriedPosts: inside its posts' share, after its watch)
    // and each small town's for its own ("stores:x", TownLabourFirst: its tax's budget after its relief and its other
    // posts). And in labourWeek, since: what the carried labour asked this week, paid or not ("treasury|asked"), and the
    // day labour a town paid itself ("stores:x"), each its next week's measure.
    std::map<std::string, double> labourLeft;
    // Round 12, lane B (HoldingsBite; the user, 2026-10-29): each treasury's holdings tax not yet spent back in its town
    // (its relief, its day labour, at its week its works), by its account.
    std::map<std::string, double> biteLeft;
    // Round 10, lane A (the user, 2026-10-27; with work by the hours): a House's week's budget in parts, what is left of
    // each ("house:x|posts", "|road", "|tops") and the floor it never pays out below ("house:x|floor"; and the church's,
    // "town:all:church|floor"), set at its week's reckoning (spendLeft); and what each part paid out this week, the next
    // week's measure, and what its caravans brought home since its last week's budget ("house:x|takings", spent on top
    // of its next: spendWeek).
    std::map<std::string, double> spendLeft, spendWeek;
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
    void setStores(std::map<std::string, std::string> byCell)
    {
        storeForCell_ = std::move(byCell);
        ++dayRevision_;
    }
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
    // What a farm spends at its town's food shops on its bunkhouse's food (stockBunkhouses): a penny for each 15 of the
    // nourishment it still wants, 2p at least, at a 12p day, at the town's day (doc 64, Phase 13).
    std::int64_t farmhandsFoodMoney(int wanted, const std::string& town) const
    {
        return pennies(double(std::max(2, wanted / 15)) / 12., town);
    }
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
    // A harness may lengthen it (setUnseenStep: econ_watch's --unseen-step, an economy run's fewer, longer decisions; the
    // game keeps UnseenStep).
    static constexpr int UnseenStep = 5;
    static int unseenStep() { return unseenStep_; }
    static void setUnseenStep(int seconds) { unseenStep_ = std::clamp(seconds, 1, 600); }
    // Whether tick(seconds) at absoluteDay needs the bodies at all: anyone seen (`anySeen`), the unseen's step, a new day,
    // or the society's first decisions. Otherwise it is given none, and nothing happens to anyone that second.
    bool needsBodies(double seconds, double absoluteDay, bool anySeen) const;
    // Whether the last decisions included the unseen residents' (their errands follow: World::continueSchedules).
    bool unseenDecided() const { return unseenDecided_; }
    // The day's plans, for the decisions that follow (the world sets them before each tick; without, every day is
    // an ordinary working day under a fair sky).
    void setDay(LifeDay day)
    {
        day_ = std::move(day);
        ++dayRevision_;                             // (The records' towns: placeRecords.)
    }
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
    static bool producesForSale(const items::Producer* producer, const std::string& item);   // (Its work's producer's.)
    static bool hasFood(const EconomyAccount& account);   // !bestFood(account).empty(), without building it.
    // Kept hands' meals (doc 64, Phase 13; doc 32): a wolf off its own day (a Hold's hand, a party's companion) eats what
    // it carries, and when it carries nothing takes a piece of food from its mess at the eat mark and eats it then: the
    // Chapter's stores, or the pack of whoever asked it along. The game sets the messes as its camps and parties stand
    // (not saved: those are), each its own account's ID by the kept hand's, and what of a mess isn't to be eaten (a
    // player's goods for an open contract or mission: `keeps`, by the mess's account, as plain goods' ids); one without a
    // mess eats only what it carries.
    void setMesses(std::map<std::string, std::string> messes, std::map<std::string, std::set<std::string>> keeps = {})
    {
        messes_ = std::move(messes);
        messKeeps_ = std::move(keeps);
    }
    const std::map<std::string, std::string>& messes() const { return messes_; }
    const std::map<std::string, std::set<std::string>>& messKeeps() const { return messKeeps_; }
    // What a kept hand takes from that mess (round 6, the companions' review): prepared food first, what no craft works
    // with (items::ingredient: not a keep workshop's raw meat, nor a hunter's rabbit), raw goods last; of those, what
    // spoils soonest, the most nourishing of those; never one kept back (setMesses). Empty: nothing it may eat.
    std::string messFood(const std::string& mess, const EconomyAccount& stores) const;
    // A kept hand that can't eat: as hungry as `hunger` or more, carrying nothing that feeds, its mess (if any) with
    // nothing it may eat, or a piece it couldn't take (its purse holding MaxGoodsKinds kinds already: a refused draw is a
    // bare mess, never a hand starving unsaid). The game tells the Chapter or the hirer at KeptToldHunger, and at
    // KeptLeaveHunger lets it go back to its own life, where it eats as anyone does: nobody starves in a Chapter's service.
    bool unfed(const std::string& id, double hunger) const;
    static constexpr double KeptToldHunger = 70, KeptLeaveHunger = 90;
    // The meals eaten from a mess since the last call (who, from which account, what), so the game can tell a player
    // what went from its pack, when it was eaten. At most MaxMessMeals kept between calls.
    struct MessMeal
    {
        std::string who, mess, item;
    };
    static constexpr std::size_t MaxMessMeals = 256;
    std::vector<MessMeal> takeMessMeals() { return std::exchange(messMeals_, {}); }
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
    // An amount in labourer's days (LD), in whole pennies at the town's day for amounts ("" the realm's: amountDay), a
    // penny at least (doc 64, Phase 13: the realm is founded at ladder.json's `foundingDay`, 60p). The amounts here that
    // were written as pennies when a day was the catalogue's 12p are kept so, in LD (5 / 12. for 5p): in a realm founded
    // at 12p each is what it was, at either pace.
    std::int64_t pennies(double days, const std::string& town) const { return pennies(days * amountDay(town)); }
    // A labourer's day for the amounts in LD and the catalogue's prices (pennies(days, town), atGoingRate; doc 64, Phase
    // 13). With work by the hours, the town's going rate (its measure a labourer's day, the skill's premium off). At the
    // seconds' pace, the realm's day at its founding (memory.foundedDay; the catalogue's, for a realm founded before Phase
    // 13 or a village): that pace's measure keeps the premium in, as it did (a town of skill-10 shops at 12.8p), so the
    // amounts don't follow it, and stay as they were.
    double amountDay(const std::string& town) const;
    // A good's catalogue price (in the catalogue's pennies: a day of `catalogueDay`), at the town's day for amounts.
    // Meals, herbs and swords are still priced so (storePriced).
    double atGoingRate(double catalogue, const std::string& town) const;
    // The dearest piece the ledger moves, in LD (1,000p at a 12p day), at the realm's day (amountDay("")): one rate for it
    // and every cap under it (a stall's price, a story's prize), so none takes a price the ledger refuses.
    static constexpr double PieceMost = 1000 / 12.;
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
    // And the holdings tax (round 11, lane 2; the user, 2026-10-29): a share of what a home holds over HoldingsMonths of
    // its needs, a week's (HoldingsTaxRate to start, 0 to MostHoldingsTax; 0 is off), taken a seventh of it a day
    // (Society::holdingsTax) and paid to its town. Saved (EconomyMemory::holdingsTax).
    static constexpr double HoldingsTaxRate = .01, MostHoldingsTax = .1;
    static constexpr int HoldingsMonths = 6;
    double holdingsTaxRate() const { return state_.memory.holdingsTax; }
    EconomyResult setHoldingsTax(double rate, const std::string& by = "");
    // The holdings tax that bites (round 12, lane B; HoldingsBite; the user, 2026-10-29): of what a home holds over its
    // target (HoldingsMonths of its needs), the lever's rate on the first target's worth over it (holding up to
    // BiteFrom targets), BiteMiddle times it up to BiteTop targets, BiteHigh times it above that; no band over
    // MostHoldingsTax a week, and never below its target. What it brings is the town's to spend back in the week
    // (EconomyMemory::biteLeft): its relief past its week's allowance, its day labour past its own, and what's left on
    // its works' crew at its week. Not its income (its budget doesn't count it twice), nor its holding for the capital's
    // share, TaxEase, the rebate or its budget (biteHeld).
    static constexpr double BiteFrom = 2, BiteTop = 4, BiteMiddle = 3, BiteHigh = 6;
    double biteHeld(const std::string& treasury) const
    {
        const auto found = state_.memory.biteLeft.find(treasury);
        return found == state_.memory.biteLeft.end() || !workByHours() || !wageRule(HoldingsBite) ? 0. : std::max(0., found->second);
    }
    // A week's holdings tax on `pot` over `target` (the lever's rate, `rate`), graduated with HoldingsBite or flat.
    static double holdingsOwed(double pot, double target, double rate, bool graduated);
    // A town's own rate (round 10, TaxEase): the realm's, eased while its stores hold over their target, TaxEaseMonths
    // (the model's RULE["treasury"] k) of what its tax brings at the full rate. A resident's is its town's
    // (treasuryOfResident). The capital's is the realm's.
    double townTaxShare(const std::string& treasury) const
    {
        const auto found = state_.memory.townTax.find(treasury);
        return found == state_.memory.townTax.end() ? 1. : found->second;
    }
    double townTaxRate(const std::string& treasury) const { return taxRate() * townTaxShare(treasury); }
    double residentTaxRate(const std::string& id) const
    {
        return state_.memory.townTax.empty() || !workByHours() || !wageRule(TaxEase) ? taxRate()
                                                                                     : taxRate() * townTaxShare(treasuryOfResident(id));
    }
    double townTaxTarget(const std::string& treasury) const
    {
        const auto found = state_.memory.taxBase.find(treasury);
        return found == state_.memory.taxBase.end() ? 0. : TaxEaseMonths * found->second;
    }
    double townHeldMean(const std::string& treasury) const
    {
        const auto found = state_.memory.heldMean.find(treasury);
        return found == state_.memory.heldMean.end() ? 0. : found->second;
    }
    static constexpr double TaxEaseMonths = 2;      // The model's RULE["treasury"]["k"]: a treasury holds two months.
    static constexpr double TaxEaseWeeks = 4;       // The rate moves a quarter of the way to where it's wanted a week.
    static constexpr double HeldMeanDays = 7;       // The stores' holding a week's mean: the caravans come and go.
    // What a town's caravans may take on the road now (round 10, RoadBudget): what's left of its week's allowance, or -1
    // where none is kept (the capital's, a House's; before its first week's budget).
    std::int64_t roadMoneyFor(const std::string& trader) const;
    static bool roadBudgeted()
    {
        return workByHours() && wageRule(CaravanIncome) && wageRule(CaravansAsTheyCome) && wageRule(TownPosts) && wageRule(RoadBudget);
    }
    EconomyResult setTaxRate(double rate, const std::string& by);
    EconomyResult setMint(bool on, const std::string& by);
    EconomyResult reliefGrant(const std::string& town, std::int64_t coins);
    std::int64_t moneySupply() const;
    bool conserved() const;
    // Every ledger entry recorded since the last call, oldest first (the saved ledger keeps only the latest 128).
    // For the world's event log; never saved. Keeps at most JournalKept if nobody collects it.
    static constexpr std::size_t JournalKept = 50000;
    std::vector<EconomyEntry> takeJournal();
    // The entries waiting there, not taken (econ_watch's accounts: what the founding paid before its first tick).
    const std::vector<EconomyEntry>& pendingJournal() const { return journal_; }

  private:
    LifeDay day_;
    Parallel parallel_;
    std::map<std::string, double> mealsSold_;       // A shop's till -> the meals it sells a day (a running average).
    // Doc 64, Phase 4 (RatwPrices.cpp): account -> item -> what its stock cost a piece, on average; seller's account ->
    // good -> its margin; and each seller's normal cost of a good it makes (by keeper or account), worked out once a day.
    std::unordered_map<std::string, std::unordered_map<std::string, double>> cost_, margins_;
    mutable std::unordered_map<std::string, std::unordered_map<std::string, double>> unitCost_;
    std::unordered_map<std::string, std::unordered_map<std::string, int>> daysOver_;   // Till -> good -> days over its target.
    // Goods coming into a costed account: its average cost a piece, `had` held before, `n` more at `each`.
    void addCost(const std::string& account, const std::string& item, int had, int n, double each);
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
    // Round 14, lane 1: of those, the buyers it turned away (till -> good -> a day, a running average as sellRate_'s,
    // kept by Society::spoil from missedToday_ whatever the rules: it moves nothing by itself; MarginsByShortage's
    // margin reads it). Only the buyers there were: a maker or a caravan's order that found no seller in its town with
    // any left (missedAt, at the town's producers), a hungry wolf's meal at a bare shelf (missedMeal), a household's
    // fund that found its piece on no shelf; not the sold-out half again, a guess at buyers that a shelf selling its
    // last at dusk earns every day. Entries under .01 dropped. Not saved.
    std::unordered_map<std::string, std::unordered_map<std::string, double>> missedRate_;
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
        // Its own work's producer (its spec's workLabel), and what its job's title says: idlePost, outworkTitled, the
        // Town Works', the title's producer.
        const items::Producer* producer = nullptr;
        const items::Producer* jobProducer = nullptr;
        bool idleJob = false, outworkJob = false, labourJob = false;
        // Its town and treasury (communityOfResident, treasuryOfResident), while placesHold().
        std::string town, treasury;
        // The day's plan at its home (its community's, or its nearest market's: decideAuthored), as of the day's plans
        // `planned` (dayRevision_; 0: not since its home changed) and its home's point.
        const DayPlan* plan = nullptr;
        std::uint64_t planned = 0;
        double planX = 0, planY = 0;
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
    // Whether the records still stand: nothing they rest on has changed since they were made.
    bool recordsHold() const { return recordsOf_ == this && recordsRevision_ == rosterRevision_ && careersIndexed_ && specsIndexed_; }
    // The records' towns and treasuries, and the merchants by town (payerOf), rest on the day's communities too (setDay,
    // setStores: dayRevision_) and on which accounts there are (the stores; counted, as one closed moves the roster on).
    std::uint64_t dayRevision_ = 1;
    struct PlacesStamp
    {
        const Society* of = nullptr;
        std::uint64_t roster = 0, day = 0;
        std::size_t accounts = 0;
        bool operator==(const PlacesStamp& o) const { return of == o.of && roster == o.roster && day == o.day && accounts == o.accounts; }
    };
    PlacesStamp placesAt_;
    PlacesStamp placesNow() const { return {this, rosterRevision_, dayRevision_, state_.accounts.size()}; }
    bool placesHold() const { return recordsHold() && placesAt_ == placesNow(); }
    void placeRecords();
    // Each town's merchant positions, in positions_ order, with their business (placeRecords).
    std::unordered_map<std::string, std::vector<std::pair<const Position*, const items::Business*>>> merchantsByTown_;
    // communityOfResident and treasuryOfResident by its record: what the record keeps while placesHold(), else `spare`.
    const std::string& communityOfResident(const ResidentRecord& r, std::string& spare) const
    {
        return placesHold() ? r.town : (spare = communityOfResident(r.entry->first));
    }
    const std::string& treasuryOfResident(const ResidentRecord& r, std::string& spare) const
    {
        return placesHold() ? r.treasury : (spare = treasuryOfResident(r.entry->first));
    }
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
    std::map<std::string, std::string> messes_;     // Kept hand -> the account it eats from (setMesses).
    std::vector<MessMeal> messMeals_;               // Meals eaten from a mess since takeMessMeals.
    std::map<std::string, std::set<std::string>> messKeeps_;   // A mess's goods not to be eaten (setMesses).
    // The piece a kept hand took from its mess, being eaten (keptHandDoes): kept hand -> mess, good. Not saved: a restart
    // finds it in the hand's purse, and it is eaten as its own.
    struct Drawn
    {
        std::string mess, item;
    };
    std::map<std::string, Drawn> drawn_;
    // A kept hand's deciding (both rosters): at the eat mark, carrying nothing, a piece taken from its mess (in its turn:
    // false at once, `atOnce`, when it would take one), and whether it eats now.
    bool keptHandDecides(const std::string& id, const EconomyAccount& wallet, double hunger, bool atOnce, bool& eats);
    // And its doing: eight seconds at its meal, as anyone's, then the piece it took eaten (else what it carries that
    // spoils soonest), and the meal told (takeMessMeals).
    void keptHandDoes(const std::string& id, ResidentLife& life, const EconomyAccount& wallet, bool eats, int step);
    // One let go (no longer kept) with a piece it took and hasn't eaten gives it back to the mess it came from, in its
    // turn (false at once, `atOnce`, when it would): no meal walks off with a dismissed companion.
    bool giveBackDrawn(const std::string& id, bool atOnce);
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
    static inline bool workByHours_ = false, costAtPace_ = false, foundAnyLand_ = false;
    std::map<std::int64_t, double> paceHeld_;       // (holdPace: a test's.)
    double workCredited_ = 0, workLost_ = 0;
    std::unordered_map<std::string, std::pair<double, double>> workBy_;   // Account -> hours credited, lost (econ_watch).
    // The land's work that counts toward the pace's brake (doc 64, Phase 13): a producer's but a seasonal one's (a
    // field's, whose work is the calendar's; an apiary's, idle all winter: round 6, the round-5 review).
    static bool brakesOn(const items::Producer& p);

  public:
    // The realm's pace (doc 64, Phase 6): with work by the hours, a craft's or yield's hours go from the old pace to the
    // calibrated over some eight weeks (PaceStep a day) while the realm is fed, and back a little faster (PaceBack a day)
    // while more than PaceHungry of it goes hungry: the labour market needs weeks to move hands to the land, and the
    // founding's stores last days. Where it settles is what the realm's hands can carry.
    // Steadied (Phase 13): it goes on only while the realm's food in hand (homes and shelves, days a head, a town's at
    // most PaceCoverCap) hasn't fallen over the week (today's reading against the one exactly a week ago: PaceWeek
    // days, ends included; within PaceNoise days a head, a reading wandering by a few hundredths, it didn't fall); it
    // steps back while the week's food falls by over PaceSlide days a head or averages under PaceFloor days; and back
    // faster while the realm goes hungry, as before. (2026-10-26: no longer held while the land's hands are short of
    // what its sales want. That hold stopped the pace at the first shortage of hands, and with it any wage bid; the
    // land's shortage is its posts open now, which its weekly review answers by its wage, as the model's market does.)
    // A loose brake in its place (2026-10-26): it holds while the land's hours worked (its producers' but the seasonal
    // fields', with their hands') over the last seven day passes fell by over PaceLandSlack against the seven before
    // (PaceLandDays of readings, each with its day; fewer, or a gap, and the food alone decides). The pace stands
    // while the realm stops following it to the land, not at the first shortage of hands.
    static constexpr double PaceStep = 1. / 56, PaceBack = 1. / 14, PaceHungry = .02;
    static constexpr int PaceWeek = 8, PaceLandDays = 14;
    static constexpr double PaceSlide = .15, PaceNoise = PaceSlide / 3, PaceFloor = 1, PaceCoverCap = 4, PaceLandSlack = .05;
    static constexpr double PaceLandMost = 1e9;    // (A day's land hours read back from a save: at most this.)
    // Whether residents' work takes the calibrated hours (econ_watch --hours; on in play from doc 64's Phase 6).
    static void setWorkByHours(bool on) { workByHours_ = on; }
    static bool workByHours() { return workByHours_; }
    static void setCostAtPace(bool on) { costAtPace_ = on; }   // (A test's: ladder.json's costAtPace, whatever the data says.)
    // A test's: a land of any size founds its wage at ladder.json's foundingDay as a realm's is (foundWage), so a test's
    // village can be a realm founded at 60p; its money is still a village's.
    static void setFoundAnyLand(bool on) { foundAnyLand_ = on; }
    // A test's (econ_watch --hold-pace, doc 64, Phase 13): the realm's pace on each day given, as another run had it,
    // whatever this one's hunger, food and hands would make it (so a run with players differs from one without by what
    // they did, not by the pace they moved). Not saved; the game never sets it.
    void holdPace(std::map<std::int64_t, double> byDay) { paceHeld_ = std::move(byDay); }
    // Hours of work credited to businesses and producers so far, and those lost to a full bank (econ_watch's measures).
    double workCredited() const { return workCredited_; }
    double workLost() const { return workLost_; }
    const std::unordered_map<std::string, std::pair<double, double>>& workBy() const { return workBy_; }
    // And the labour market's (doc 64, Phase 13): each town's going rate ("" the realm's), an employer's town, and an
    // account's hours of work in hand.
    const std::map<std::string, double>& goingRates() const { return state_.memory.going; }
    // Each time an employer is marked short of pay (econ_watch's shortpay.csv; doc 64, Phase 13, round 7): its key, what
    // it was paying ("wage", a hand's spell; "hire", a hire's day), what it owed and what its purse held. Kept only when
    // asked (noteShorts), until taken.
    struct ShortNote
    {
        std::string key, what;
        std::int64_t owed = 0, cash = 0;
    };
    static void noteShorts(bool on) { noteShorts_ = on; }
    std::vector<ShortNote> takeShorts() const { return std::exchange(shorts_, {}); }   // (A measure: taken, not moved.)
    // What the capital paid of each town's Town Works' day labour (CapitalLabour), and each town's rebates (TaxRebate),
    // since the last call, by its stores' account (econ_watch's town_tax.csv; a measure, kept only while asked).
    static void noteTowns(bool on) { noteTowns_ = on; }
    std::map<std::string, std::int64_t> takeLabourCarried() const { return std::exchange(labourCarried_, {}); }
    std::map<std::string, std::int64_t> takeRebates() const { return std::exchange(rebated_, {}); }
    // And each treasury's holdings tax (HoldingsTax) since the last call, by its account ("treasury" the capital's).
    std::map<std::string, std::int64_t> takeHoldingsTaxed() const { return std::exchange(holdingsTaxed_, {}); }
    // And (round 11, lane 4) the capital's share each town sent, and the day labour each paid itself (TownLabourFirst).
    std::map<std::string, std::int64_t> takeShares() const { return std::exchange(sharesSent_, {}); }
    std::map<std::string, std::int64_t> takeLabourOwn() const { return std::exchange(labourOwn_, {}); }
    // And (round 12, lane B) the holdings tax's money each town spent back (HoldingsBite: relief, day labour, its works).
    std::map<std::string, std::int64_t> takeBiteSpent() const { return std::exchange(biteSpent_, {}); }
    // A trial's (econ_watch --without, rounds 7 and 8): the wage book's rules of round 7, and round 8's, left out, one at
    // a time, to see what each does. Not saved; the game never sets it.
    enum WageRule : std::uint64_t
    {
        ChildrenOpen = 1,                            // A hire's place a child holds stands open in the wage book.
        ByEmployees = 2,                             // The going rate weighed by employees, not the keeper.
        StallsShed = 4,                              // A stall lets its surplus help go at its weekly review.
        SellersOnly = 8,                             // Only the stalls and the moneychangers let go (round 8).
        KeepApprentices = 16,                        // No apprentice is let go, nor counted in the excess (round 8).
        UnpaidFirst = 32,                            // Only the unpaid, not the out of work, look for crews early (round 8).
        CaravanIncome = 64,                          // A town's caravans' net takings count in its budget (round 8).
        CaravansAsTheyCome = 128,                    // ...in its week's budget on top of its tax's, not its month (round 9).
        FarGiveWay = 256,                            // A field's far taker, still on its way, gives way to a nearer (round 9).
        KeepersApprentice = 512,                     // Only the keeper's own apprentice is kept over and above (round 9).
        PostsFirst = 1024,                           // A town's caravans' week pays its own posts' shortfall first (round 9).
        CrewsAbroad = 2048,                          // A town's works crew may be taken by wolves of other towns (round 9).
        OnePlace = 4096,                             // A wolf holding a hire's place takes no other hire (round 9).
        OwnersStop = 8192,                           // A lone owner stops topping up a till short of its wages (round 9).
        TaxEase = 1048576,                           // A town's stores over their target lower its tax (round 10, towns).
        TownPosts = 2097152,                         // A town's own posts are kept to its tax, as the capital's (off unless asked).
        RoadBudget = 4194304,                        // A town's caravans' road money from its week's budget (off unless asked).
        TaxRebate = 8388608,                         // Over twice its target, a rebate to its taxpayers (round 10).
        FoundAtTarget = 16777216,                    // A town is founded at its target, the rest to the realm's others.
        CapitalLabour = 33554432,                    // The capital pays the small towns' Town Works' day labour (round 10).
        HouseBudget = 16384,                         // Every House outflow draws on its week's budget (round 10, lane A).
        HouseTills = 32768,                          // ...a House's till it can't top up trades on as a lone owner's (round 10).
        ChurchBudget = 65536,                        // The church's alms with its relief, inside its budget, over a floor (round 10).
        CheapAlms = 131072,                          // Alms buy the cheapest food that feeds the wolf, not the shop's best (round 10).
        CarryParts = 262144,                         // A House's budget unpaid last week goes to this week's works and crews (round 10).
        FoodAtItsPrice = 67108864,                   // A hungry wolf buys where it can pay for food, not by the bite (round 10, C).
        FeastLaidIn = 134217728,                     // A town's stores buy its festival's feast from its shops (round 10, C).
        NearestShop = 268435456,                     // With no shop of its town open, the nearest it can pay at (round 10, C).
        IdleTills = 1ull << 29,                      // A stopped business goes idle: no wages, its hands let go (round 11).
        OwnerReturns = 1ull << 30,                   // ...and its owner returns to it when its sales would pay (round 11).
        MoreHands = 1ull << 31,                      // A business holding well over its working money takes on hands (round 11).
        RichHomes = 1ull << 32,                      // A home's help, servants and wants scale with its budget (round 11, lane 2).
        HoldingsTax = 1ull << 33,                    // A home holding over HoldingsMonths of its needs pays the holdings tax (round 11).
        CarriedPosts = 1ull << 36,                   // The day labour the capital carries is kept to its posts' share (round 11).
        TownLabourFirst = 1ull << 37,                // A small town pays its own day labour from its tax's budget first (round 11).
        ShareByWealth = 1ull << 38,                  // The capital's share of a town's tax by its wealth and the capital's (round 11).
        WageBySales = 1ull << 39,                    // A business raises its wage only if its sales pay it (round 12, lane A).
        WagelessFirst = 1ull << 40,                  // A business's hire goes to those without a wage of their own first (round 12).
        ShelfOrders = 1ull << 43,                    // A town's food orders by its shelves and its hungry, from the nearest (round 12, C).
        HoldingsBite = 1ull << 41,                   // The holdings tax graduated over its target, spent back in its town (round 12, B).
        CapitalByIncome = 1ull << 42,                // The capital's week kept to its income, over a reserve floor (round 12, B).
        LivingSlack = 1ull << 44,                    // The realm's idle and grown are the living, a week's hire work (round 13, 1).
        WageBookMended = 1ull << 45,                 // The weekly review one chain again: a short week no bid nor calm (round 13, 1).
        // Round 13, lane 2 (the user, 2026-10-29: inequality): a trading town's money that has no use among its own poor
        // goes on to the city's poor, by how much poorer the city's homes are than the town's; and no baby works or hoards.
        SurplusByNeed = 1ull << 46,                  // A town's surplus over two targets to the city's crews, by need (round 13, 2).
        ChildrenKept = 1ull << 47,                   // Under ChildWorkAge no odd job; a child's spare to its home (round 13, 2).
        KitchenKept = 1ull << 48,                    // A food maker's kitchen runs with its keeper hungry or away (round 13, 3).
        FeastAhead = 1ull << 49,                     // A festival's feast bought in the week ahead, from what shops hold over (round 13, 3).
        // Round 14, lane 1 (the user, 2026-10-30: Ridgemere's food dearer every quarter, its margins at their caps
        // along the chain): a seller's margin follows the buyers it turns away, and a good only sold on is priced once.
        MarginsByShortage = 1ull << 50,              // A margin aims by the buyers turned away, not a shelf bare at the turn (round 14, 1).
        ChainPricedOnce = 1ull << 51,                // A maker buys at the cheapest seller; a good sold on carries the ladder's margin (round 14, 1).
        // Round 14, lane 3 (the user, 2026-10-30): a hand's day on the land brings in at once, and the market follows the
        // post and whoever holds it, not its founder's written record.
        HandsBringIn = 1ull << 54,                   // A hand's day on a producer's land made into yields when it's done (round 14, 3).
        TradeByPost = 1ull << 55,                    // A seller's trade, lists and town are its post's, held by its holder (round 14, 3).
        // Round 14, lane 2 (the user, 2026-10-30: shopping): the wolves compare the food shops near them, and the food
        // shops keep their hours through a festival.
        ShopsCompared = 1ull << 52,                  // The hungry and the larders go to the cheapest food near, not the nearest (round 14, 2).
        FestivalShopsOpen = 1ull << 53,              // A food shop's keeper keeps its hours on a festival day (round 14, 2).
        Markdowns = 1ull << 34,                      // What a seller holds over its shelf target a day marked down (round 11, 3).
        FoodByNeed = 1ull << 35                      // A town short of food sends for its people's week, not a dozen a shop (round 11, 3).
    };
    // The rules that are off unless asked (econ_watch --with): a town's crew abroad (round 9, off since round 10: the user,
    // 2026-10-27), its caravans' road money from its week's budget (round 10, dropped by the user, 2026-10-27) and its own
    // posts kept to its tax (round 10, dropped by the user, 2026-10-28).
    // `wageRulesOff(off, on)`: `off` the rest left out, `on` these put in; wageRulesOff(0) is the game's.
    static constexpr std::uint64_t OptInRules = CrewsAbroad | RoadBudget | TownPosts;
    static void wageRulesOff(std::uint64_t off, std::uint64_t on = 0)
    {
        wageRulesOff_ = off & ~OptInRules;
        wageRulesOn_ = on & OptInRules;
    }
    static bool wageRule(WageRule rule) { return (rule & OptInRules) ? (wageRulesOn_ & rule) != 0 : (wageRulesOff_ & rule) == 0; }
    std::string employerTownOf(const std::string& key) const
    {
        const auto found = employerTown_.find(key);
        return found == employerTown_.end() ? std::string() : found->second;
    }
    // The realm's idle share of its grown wolves of working age at the last labour pass (the calm cut's, the mint's): with
    // LivingSlack the living only, a wolf on a week's hire working (round 13, lane 1). A measure.
    double slack() const { return slack_; }
    double workBankedOf(const std::string& account) const
    {
        const auto found = workBanked_.find(account);
        return found == workBanked_.end() ? 0. : found->second;
    }

    // (Tests': a till's hours in hand set as its hands' spells would leave them; when a keeper's own time at work was last
    // counted, -1 for never; whether a batch is at work for it.)
    void setWorkBanked(const std::string& account, double hours) { workBanked_[account] = std::clamp(hours, 0., WorkBankedMost); }
    double lastAtWorkOf(const std::string& keeper) const
    {
        const auto found = lastAtWork_.find(keeper);
        return found == lastAtWork_.end() ? -1. : found->second;
    }
    bool batchAtWork(const std::string& keeper) const { return craftAtWork_.count(keeper) > 0; }
    // Round 13, lane 3 (the user, 2026-10-29: KitchenKept, with work by the hours): a food maker's kitchen kept running
    // when its keeper is hungry or away. Ridgemere's hunger spikes (days 469-473 of the two-year run, 119 hungry, 27
    // starving) came with flour and firewood in town, the church holding 166-231k and 97% of the hungry holding the price
    // of the cheapest food: three of its four bakeries baked nothing for five days, their work in hand full, because a
    // batch starts only in craft(), craft() only in its keeper's trade, and a hungry keeper (it can't buy at its own shop)
    // went off to eat, or slept at hunger 92-95 holding 300-400p. A kitchen: a food shop (foodShop) that makes or supplies
    // a food (foodMade) and has a till of its own (tillOf: not a keeper whose purse is the till, which eats its own stock
    // as before). Worked out once a day a keeper (kitchens_), on the main thread.
    //  - It eats at its own counter: on duty, hunger 55 or more and nothing on it, its till holding a food it sells that
    //    its purse pays for, it stays at its trade and buys one piece (counterFood: the most nourishment for the money),
    //    purse to till at the shop's price, a sale like any other ("bought at its own counter").
    //  - It bakes first: hunger from 55 to under BakeFirstBelow, nothing it sells on its shelf, but a food batch it could
    //    start now (foodBatchReady), it stays at its trade; the batch is on the shelf a minute on, and it eats at its
    //    counter. From BakeFirstBelow, or with no batch ready, it goes for food as before: it can't wait itself to starve.
    //  - The hands bake while it is away: each second, after the residents' turns, a kitchen whose keeper isn't keeping
    //    it crafts from its work in hand (craft's byHands: no keeper's time counted or credited, no yield, no supplier's
    //    restock); the shop opens only with its keeper, the food waiting on its shelf till then.
    static constexpr double BakeFirstBelow = 80;
    bool kitchen(const std::string& keeper);
    std::string counterFood(const std::string& keeper, std::int64_t purse, std::int64_t* price = nullptr);
    // Whether a kitchen's next batch, as craft() would choose it (in its order, the first whose good is under its shelf
    // target and whose materials its till holds), is a food its work in hand covers now; or the batch at work is a food,
    // on the shelf at its next look. Only with that look due within a quarter hour. Only reads: asked from the
    // residents' decisions.
    bool foodBatchReady(const std::string& keeper, double absoluteDay) const;
    // Round 13, lane 3 (FeastAhead, with FeastLaidIn): a festival's feast bought over the FeastAheadDays before it (k days
    // ahead, (FeastAheadDays + 1 - k) / (FeastAheadDays + 1) of the feast's want, less what the stores hold), from what
    // each food shop holds over its shelf target (what Markdowns would mark down), never meals (a meal shop restocks from
    // the stores at wholesale: they'd go back out); at the turn, as before. Every transfer 99 pieces at most.
    static constexpr int FeastAheadDays = 7;
    // Round 14, lane 3 (the user, 2026-10-30), with work by the hours.
    // HandsBringIn: a hand's day on a producer (a fishery's, a warren's, a field's) was credited at once as HireSpells
    // hours to a bank capped at WorkBankedMost, but only the keeper's own paid spell made banked hours into yields, so a
    // producer hiring four or more lost 13-34% of what its hands worked (Ridgemere's fisheries, days 91-729: 6.6-11.6k
    // hours a quarter, 15-23k fish, while its makers went 21-28k hours a quarter short of fish; its catch levelled off at
    // about 100 a day however many it hired). Under the rule, when a hand's day on a producer is done, and only while
    // the producer is alive and holds its post with a till of its own, the yield loop runs on what its bank holds, the
    // day is credited, and the loop runs again (produce with no hours of the keeper's): the hours become yields at once
    // while its store is under its kept target, and wait in the bank (lost past its cap) only while the store is full.
    // TradeByPost: the market's lists (World::marketOf, sellersOf, the shops of each town...) were built from the
    // authored residents by their written role and work label, so a dead founder's post, held by a successor written
    // with another label (a smelter's 'works at The Frosthollow Smelter', a civilian) or by one born or come later
    // (civilian, '-'), sold nothing wholesale, made nothing and was bought from nowhere: the Frosthollow smelter held 148
    // bars unsold from day 364, and stopped. Under the rule a seller's trade is its post's (tradeLabel, keepsShop), its
    // town its post's (sellerTown), and the lists are of the posts with a till and a living holder, in their founders'
    // written order, the holder standing for the seller (traders): with no succession, the same lists in the same order.
    static bool handsBringIn() { return workByHours() && wageRule(HandsBringIn); }
    // A hired hand's day (`spells` hours) on the land done for its producer `keeper` (advanceOddJob; a test's too): its
    // hours to the producer's work in hand, and with HandsBringIn its yields brought in then.
    void handsDayOnTheLand(const std::string& keeper, int spells, double absoluteDay);
    static bool tradeByPost() { return workByHours() && wageRule(TradeByPost); }
    // A seller's trade: under TradeByPost, the title of the post it holds when that post has a till and names a business
    // or a producer; otherwise its written work label ("" with no record). Whether it keeps a shop: under the rule, its
    // post's role is a merchant's; otherwise its written role is.
    const std::string& tradeLabel(const std::string& id) const;
    bool keepsShop(const std::string& id) const;
    // A post's town (where its work is; with no work cell, its founder's, as communityOfResident finds it), and a seller's
    // (under TradeByPost, the post's it holds with a till; otherwise, or with none, communityOfResident).
    std::string postTown(const Position& p) const;
    std::string sellerTown(const std::string& id) const;
    // The economy's sellers, in the written order: each written resident; under TradeByPost, for a founder whose post has
    // a till, that post's holder instead (none while it stands vacant: there is nobody to sell), and nobody for a
    // resident holding another founder's post with a till (it stands at that founder's place). The callers' filters
    // (alive, role, trade) apply to whoever is listed.
    std::vector<std::string> traders() const;
    // (A test's: who sells a good as buyMaterials reads them, built afresh: by id and kind, 0 supplier, 1 workshop, 2
    // producer.)
    std::vector<std::pair<std::string, int>> sellersListed(const std::string& item) const;

  private:
    // Round 14, lane 2 (the user, 2026-10-30: ShopsCompared, with work by the hours): the wolves compare the shops. A
    // hungry wolf went to the nearest open food shop of its cell, else the first of its town, and a household laid in
    // starting at a shop fixed by its home, so a dear seller lost no buyers: Ridgemere's smoked fish went from 9% to 52% of
    // its residents' food spending over the two-year run (35p to 104p for 50 of nourishment, against bread's 15-20p), and
    // its three smokehouses took 47% of the city's year-2 food bill. A food shop's value: the fewest pennies for a unit of
    // nourishment among the foods it sells now (in stock, sellsFood, nourishment over 0: shopPrice / nourishment, a
    // stall's at its .9, as cheapestFood), and its cheapest food, as cheapestFood (shopOffer):
    //  - The hungry wolf, whatever its purse, looks at the open food shops of its cell and the ShopsLooked nearest of
    //    its town beyond it (by the map; off it, the first ShopsLooked of its town), and goes to the one of least value
    //    whose cheapest food its purse pays for; of values within ValueTie of the least, the nearest. None it can pay:
    //    alms, as before. (No shop of its cell or town open: NearestShop's, as before.)
    //  - A household lays in at its town's food shops least value first; those within ValueTie of each other in its own
    //    order (from its home's start), so the households still spread over shops alike.
    // Each food's price is the game hour's, asked at its first sight on the shelf in the hour (valueShop, on the main
    // thread: prices are worked out on first asking, and that isn't for the decision threads), and what's on the shelf
    // is now (shopOffer reads the till's stock): a bakery whose bread sold out at ten past is no cheap shop for the rest
    // of the hour, and one that bakes at twenty past is one again.
    // ShopsLooked: a city cell's walk, not across Ridgemere for a penny. ValueTie: within a tenth is the same price to a
    // wolf, so its walk doesn't grow for nothing. A game hour: prices move at most a day (markdowns), a week (margins)
    // or on a purchase (costs). Nothing is made and no money moves: a dear shop loses its buyers, and its markdowns and
    // its margin bring it down; a cheap one's sales raise its shelf target and its batches.
    static constexpr int ShopsLooked = 3;
    static constexpr double ValueTie = .10;
    struct ShopValue
    {
        struct Food
        {
            std::string item;                       // (Its stock's own name: its quality too.)
            std::int64_t asked = 0;                 // Its price (shopPrice), as first seen in the hour.
            int feeds = 0;                          // Its nourishment.
        };
        std::string till;
        std::vector<Food> foods;                    // Each food it sells seen on its shelf this hour.
    };
    struct ShopOffer
    {
        double value = 0;                           // Pennies a unit of nourishment, at its best (0: nothing that feeds).
        std::int64_t cheapest = 0;                  // Its cheapest food, as cheapestFood (0: none).
    };
    // A shop's prices this game hour (ShopsCompared), null if it wasn't valued this hour; and its offer now: the hour's
    // prices of what its shelf holds now, at a stall at its .9. Both only read, so the decision threads may ask.
    // valueShop: each food on its shelf not yet seen this hour priced (the hour begun afresh as it turns), on the main
    // thread only.
    const ShopValue* shopValue(const std::string& keeper) const
    {
        const auto found = shopValues_.find(keeper);
        return found == shopValues_.end() ? nullptr : &found->second;
    }
    ShopOffer shopOffer(const std::string& keeper) const;
    void valueShop(const std::string& keeper, double absoluteDay);
    // (Tests': how many hours the prices were begun afresh, and the thread that last priced a shop's food.)
    std::int64_t shopValueHours() const { return shopValueHours_; }
    std::thread::id shopValuedBy() const { return shopValuedBy_; }

  private:
    // The prices' hour (floor of the absolute day x 24, -1 for none) and its shops; the thread that priced last.
    std::unordered_map<std::string, ShopValue> shopValues_;
    std::int64_t shopValuesHour_ = -1, shopValueHours_ = 0;
    std::thread::id shopValuedBy_;
    double decidingAt_ = 0;                         // (The absolute day of the second being decided: decide().)
    int season_ = 0;                                // The season of the latest decision (0 spring .. 3 winter).
    // A shop's batch (RatwCrafting.cpp). `byHands` (KitchenKept): its hands' work in hand alone, its keeper away: no time
    // of the keeper's counted, credited or brought in, no supplier's restock; a batch finished, the day's materials laid
    // in and the next batch begun from the work in hand, as with the keeper.
    void craft(const std::string& id, const std::string& workCell, double absoluteDay, bool byHands = false);
    // craft()'s parts, read by foodBatchReady too: what a shop keeps fresh of a material it also sells (preserving), whether
    // its till holds a batch's materials, its crafts in the order it looks at them (what sells fastest first), and the
    // store of a craft's first good it keeps on its shelf.
    static int freshKept(const items::Business* business, const items::Craft& k, const std::string& item);
    bool batchHas(const std::string& till, const items::Business* business, const items::Craft& k) const;
    std::vector<const items::Craft*> craftOrder(const std::string& till, const items::Business* business) const;
    int craftKept(const std::string& id, const std::string& till, const items::Craft& k, const std::vector<std::string>& supplies) const;
    std::unordered_map<std::string, bool> kitchens_;   // Keeper -> a kitchen (kitchen()), on kitchensDay_. Not saved.
    std::int64_t kitchensDay_ = -1;

  public:
    // Buys up to `wanted` of an item for `id`: in its own community at the price, then from anywhere, carted in. A
    // supplier restocking (`forSupplier`) buys from workshops and producers, never from another supplier. Without
    // `credit`, only with what its till can spare. In its town by the kind of seller, suppliers, then workshops, then
    // producers; with ChainPricedOnce (round 14, lane 1, with work by the hours), every seller there that has one to
    // spare, the cheapest first (by the whole penny it would pay; level, the producer, the workshop, the supplier).
    // (Public for the tests.)
    int buyMaterials(const std::string& id, const std::string& workCell, const std::string& item, int wanted,
                     bool forSupplier = false, bool credit = true);
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
    // `resold`, if given, is set when the seller makes none of the good and sells it at what it paid (the -1 path).
    double normalCost(const std::string& seller, const std::string& item, bool* resold = nullptr) const;
    double marginOf(const std::string& seller, const std::string& item) const;
    // Round 14, lane 1 (ChainPricedOnce, with work by the hours): a good the seller only sells on (normalCost's resold)
    // is priced at what it paid times one and the least of its margin and the ladder's (a House's till asked by itself,
    // whose businesses tell what it makes: resells); its markdowns as before. A good it makes keeps its own margin.
    double priceAt(const std::string& seller, const std::string& item) const;   // Pennies a piece, not rounded.
    // Whether the account selling from `till` makes none of `good` (normalCost's -1 by its trade): a business's till by
    // its post's trade, a House's by any of its businesses', anything else (a caravan's load, a town's stores) sells
    // on.
    bool resells(const std::string& till, const std::string& good) const;
    // Each seller's weekly review of its margins, on its own day of the week (RatwPrices.cpp; public for the tests).
    void reviewMargins(std::int64_t day);
    // The buyers a seller turned away of a good, a day (a running average: missedRate_). 0 for none.
    double missedRate(const std::string& till, const std::string& good) const;
    const std::unordered_map<std::string, double>* missedRates(const std::string& till) const
    {
        const auto found = missedRate_.find(till);
        return found == missedRate_.end() ? nullptr : &found->second;
    }
    // Markdowns (round 11, lane 3; the user, 2026-10-29: "Shops should mark down what sits over their shelf target for
    // more than a day."): with work by the hours, once a day, each shop's and House's good held over its shelf target
    // (shelfTarget, at least ProducerKept: no seller is marked down on the store every maker keeps) counts a day over,
    // and one at or under it a day back; from its second day over, its price comes down MarkdownStep of itself a day,
    // never below MarkdownFloor of its normal cost (the weekly review's own least margin: a glut is sold at a loss,
    // as a seller's margin may sink to over weeks, but never given away). As the stock falls back under its target the
    // price climbs back a step a day. Days over are kept to MarkdownDaysMost (past the floor, more mean nothing).
    void reviewMarkdowns();
    int daysOver(const std::string& seller, const std::string& item) const;
    static constexpr double MarkdownStep = .05, MarkdownFloor = .7;   // (1 + MarginLeast.)
    static constexpr int MarkdownDaysMost = 30;
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
    // A rich home spends with its budget (round 11, lane 2, RichHomes; the user, 2026-10-29): as many hands about the
    // home as its help budget pays a week of, a post a day (hireForWeek's most), none while one of its places stands
    // untaken; servants as a tenth of its excess pays a month (the model's), topped up any day; and, past its day's
    // piece or works, up to RichWants more things it fancies while the day's spending is under half its want fund.
    static constexpr int RichWants = 4;
    // A House's crews (doc 64, Phase 13, round 6; doc 63's model, house_week), with work by the hours: its estate crew as
    // many as what's left of its week's budget pays at the going rate, up to a tenth of the realm's grown wolves of
    // working age (EstateShare), posted in gangs of CrewGang about its town; their places weighed with the businesses'
    // hires by pay, and those untaken its posts open (vacanciesOf(house + "|crew")). A crew hand's day of building work
    // mends its town by CrewMend while it isn't in full repair. A House that can't pay a day's share ends that gang.
    // (CrewMend is 0 until the user picks a value: a mend made of no materials lowered the Town Works' buying once the
    // capital paid for it, kept Ridgemere's decay off its condition, and stopped the disrepair project; round 6's review.)
    static constexpr int EstateShare = 10, CrewGang = 12;
    static constexpr double CrewMend = 0;
    static constexpr int PieceWaitDays = 3;         // Days a household waits for a piece no shelf has (Society::wants).
    double monthIncomeOf(const std::string& account) const;
    static bool unearnedKind(const std::string& kind);
    // Carting a piece `legs` steps (ladder.json carryPerWeight: labourer's hours a unit of weight a step), in pennies.
    double carriage(const std::string& item, double legs, const std::string& town = {}) const;
    static constexpr double CarriageLegs = 10;       // Steps between towns, for what's carted from one to another.
    static constexpr double MarginLeast = -.3, MarginMost = .6, StapleMarginMost = 1.;
    static constexpr double MarginStep = .04;
    // Round 14, lane 1 (MarginsByShortage, with work by the hours; the user, 2026-10-30): at its weekly review a
    // seller's margin is pulled MarginPull of the way (times its review's step lot) toward an aim set by the share of
    // its buyers it turned away (missedRate over its rate, `unmet`): the ladder's at UnmetFrom or under (a twentieth
    // turned away is a sale missed at closing, not scarcity), its most (StapleMarginMost for a staple, else MarginMost)
    // from UnmetFull (half its buyers turned away: Ridgemere's boats ran at about .55-.6), straight between. A glut
    // (over 1.7 times its keeping days and more than GoodsKept, or over its own shelf target, a seasonal field's store
    // aside) still comes down MarginStep a step. So the margin is a function of how short the seller is now: it can't
    // wind up past what its shortage earns, and it unwinds as the shortage closes (from the cap back to the ladder's in
    // about three months: a fifth of the gap a week, its step's mean .5 and three weeks in four reviewed). It never
    // rises with sales or a wage, only with buyers turned away.
    static constexpr double UnmetFrom = .05, UnmetFull = .5, MarginPull = .5;
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
    // A House's own hires for the week (its retainers, its estate crew: doc 64, Phase 13, round 6).
    static bool houseCrew(const OddJob& j)
    {
        return j.until >= 0 && j.payer.rfind("house:", 0) == 0 && (j.kind == "building work" || j.kind == "about the home");
    }
    // An institution's crew for the week: a House's, a town's works (its treasury's or stores'), the church's masons. With
    // work by the hours all are posted as a House's are (houseCrewForWeek) and taken by its rules (doc 64, Phase 13, round
    // 7; the model's staff_to: grown hands first).
    // A town's works crew for the week (its stores'), which with work by the hours any town's grown wolves may take (doc
    // 64, Phase 13, round 9; the user, 2026-10-27).
    // A business's hire for the week, not a field's (round 12, WagelessFirst: the jobless' first).
    static bool businessHire(const OddJob& j)
    {
        return j.until >= 0 && !j.field && j.payer.rfind("till:", 0) == 0 && (j.kind == "a hand" || j.kind == "a hand at the shop");
    }
    static bool townCrew(const OddJob& j) { return j.until >= 0 && j.kind == "building work" && j.payer.rfind("stores:", 0) == 0; }
    static bool crewPlace(const OddJob& j)
    {
        return houseCrew(j) || (j.until >= 0 && j.kind == "building work" &&
                                (j.payer == "treasury" || j.payer.rfind("stores:", 0) == 0 || j.payer == SharedChurch));
    }
    int estateMost() const;                          // A House's estate crew at most: EstateShare of the realm's workers.
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
    // (By its record: what it keeps, for its town, its treasury, its own work and, when `job` is its own, its title.)
    Payer payerOf(const ResidentRecord& resident, const Position& job, int age) const;
  private:
    Payer payerFor(const std::string& resident, const ResidentRecord* record, const Position& job, int age) const;
  public:
    // A resident's town as the society reckons it, where it works or else where it lives: a hire's town (OddJob::community)
    // is its payer's so (econ_watch's hires.csv, doc 64, Phase 13, round 6).
    std::string townOfResident(const std::string& id) const { return communityOfResident(id); }
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
    // A post's employer as the labour market keys it (its holder's, or for a post empty its founder's at 30), "" if none.
    std::string postEmployer(const Position& p, const std::string& holder, int age) const;
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
    // Round 12, lane A (the user, 2026-10-29): a business's wage bid only while its trade lately (memory.tradeNet) pays its
    // hands' and its open posts' wage at the higher wage, its posts open past its sales' hands (MoreHands' extra) left
    // out of the bid (WageBySales); filled, its wage drifts WageBack of the way back to the rate a week. A keeper drawing
    // its own till's wage takes a business's hire (not a field's) only from WagedFrom, and its place is the jobless'
    // again at dawn (WagelessFirst).
    static constexpr double WageBack = .25, WagedFrom = 14;
    static constexpr int SalesDays = 28;
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
    // (Round 11, lane 1: an idle business wants its keeper only (IdleTills); one holding over its working money takes on a
    // hand more for each month of a hand's wage it holds over, up to doubling (MoreHands, the user, 2026-10-29: "trades
    // with buyers take on more hands", so its buyers' money goes in wages, not its owner's profit). hoursBySales is
    // the hours its sales want, as before.)
    // (`more`: with MoreHands' extra; without, the hands its sales want alone: round 12's wage bid, WageBySales.)
    int handsWanted(const std::string& till, bool bare = true, bool more = true) const;
    double hoursBySales(const std::string& till, bool bare = true) const;   // The hours a day its sales want (handsWanted's).
    bool idleHand(const std::string& resident, const Payer& payer) const;   // Its business has no work for it.
    bool idleHand(const ResidentRecord& resident, const Payer& payer) const;   // (By its record, as payerOf.)
  private:
    bool idleHandOf(const std::string& resident, const ResidentRecord* record, const Payer& payer) const;
  public:
    // A business whose hands have more work in hand than it can use (its bank full at this turn of the day and the last:
    // its shelves full, its materials short; a producer's store full), or a field with none in its window (Phase 13).
    bool idleBusiness(const std::string& till) const;
    bool fieldIdle(const std::string& till, const FieldYear& f, const items::Producer& producer) const;   // (Its field's.)
    // A business whose hands make something: one with crafts (and so its mending), or a producer; not a stall, whose
    // help sells and whose hours are no work in hand (Phase 13).
    bool makesThings(const std::string& till) const;
    // A market stall or a moneychanger, by its business in the catalogue: a business whose help only sells, and the only
    // ones that let surplus help go with nothing made (round 8, the user, 2026-10-27).
    bool sellsOnly(const std::string& till) const;
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
    // With work by the hours (doc 64, Phase 13, round 8), goods bought to sell on are counted for ResaleDays of its
    // sales, not a month, nearer the model's firms, which carry only their inputs (a month of the shops' goods was
    // three-quarters of their working money, and kept their margin in their tills). And a lone owner tops its
    // business up only from what it holds above a week of its household's food, the model's 7 x need_cost; a House
    // gives what it has.
    // And (round 9, the user, 2026-10-27) a lone owner whose business can't pay its wages, found at StopWeeks of its
    // weekly looks in a row holding less than a week of them (staffing), stops topping it up. The business trades on
    // with what it has; on a day its till can't pay its keeper's wage the keeper is free for hire work as an unpaid hand
    // is (ownerUnpaid). Its owner tops it up again once a look finds it holding a week's wages of its own.
    static constexpr double RentShare = .15;
    // And (round 11, lane 1, the user, 2026-10-29: "if there is no work, for them to just go idle and the owners to return
    // to them when there's demand") a stopped business still short IdleWeeks after its owner stopped goes idle
    // (IdleTills): it pays no keeper's wage (its keeper free for hire work all day, ownerUnpaid) and wants no hands but its
    // keeper, so its help is let go at its weekly review and its hires run out; it keeps its stock and sells what it has.
    // It is not closed: its owner returns at a weekly look that finds what its sales would bring a day (its running sales
    // with missed sales counting, at its own prices) at least ReturnCover of the wages of the hands those sales want
    // (OwnerReturns), or, as in round 9, its till holding a week's wages again. Returned, its owner tops it up again; it
    // can fail again, and idle again. Failing is idling, not vanishing. (ReturnCover 1: its prices are its hours' cost
    // and a margin, so its sales cover its hands' wages but for the rounding of its hands up to whole ones, its keeper at
    // least: what it comes back for is buyers enough to keep its keeper's day, at prices over its hours' cost. A fishery
    // at a 14% margin didn't come back at 1.25.)
    static constexpr double ReturnCover = 1;
    static constexpr int CreditDays = 7, CreditWrittenOff = 56, ResaleDays = 14, StopWeeks = 4, IdleWeeks = 4;
    bool tillIdle(const std::string& till) const;   // Gone idle (IdleTills) and not yet returned to.
    bool ownerReturns(const Position& p, const std::string& till) const;   // Its sales would pay its hands again.
    std::int64_t workingMoney(const std::string& positionId) const;
    double staffing(const std::string& positionId) const;   // Its keeper's, its help's and two open posts' wages a day.
    // A keeper whose owner has stopped topping its till up, on a day the till couldn't pay its wage (round 9). (By its
    // post: the till is "till:" and the post's id.)
    bool ownerUnpaid(const std::string& resident, const Position& job) const;
    // A grown wolf keeping a business whose till pays it its keeper's wage today: not idle, not unpaid (round 12, lane A,
    // WagelessFirst: a business's hire goes to those without a wage of their own first).
    bool keeperWaged(const std::string& resident) const;
    // The day's net trade of each till into its running average (WageBySales); and what that average is a day.
    void keepTradeNet();
    double tradeNetOf(const std::string& till) const;
    // A seasonal field's farm (its till's year in memory.fields), judged over its season: it stops only at a look after a
    // harvest's end (harvestEndedSince: a HarvestEnd day of the year in (from, day]). (The round-9 review, the user.)
    bool seasonalField(const Position& p, const std::string& till) const;
    static bool harvestEndedSince(std::int64_t from, std::int64_t day);
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
    std::size_t householdOf(const std::string& resident) const;         // Those whose food its purse keeps (at least 1).
    std::string landlordOf(const std::string& positionId) const;        // The House whose land it works, or "".
    // What the materials (or, bought to sell on, the goods themselves) of one piece cost a seller, without its hours;
    // `resold`, if given, is set when it is bought to sell on.
    double materialsCost(const std::string& seller, const std::string& item, bool* resold = nullptr) const;
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
    // Work for hire ("carries loads for hire", "washes linen for hire", "runs messages about the city"): with work by the
    // hours, nobody's post to pay; the household keeps them, and they take hires as the jobless do (doc 64, Phase 13,
    // round 6: the capital's posts of last resort).
    static bool forHire(const std::string& title);
    // Work at home that nobody pays (keeps the house, sits by the well, sells from a tray, begs): free to leave for an
    // institution's crew, with work by the hours (doc 64, Phase 13, round 7). Not an apprentice's: it learns its trade.
    static bool unpaidAtHome(const std::string& title);
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
    // The least a hungry wolf pays at a shop for a piece of anything to eat it sells (as buyFood prices it: a stall's
    // tenth off), or 0 for nothing to eat. Not const: the sellers' costs are worked out on first asking (round 10, C).
    std::int64_t cheapestFood(const std::string& seller);
    // A festival's feast (round 10, lane C, the user's): at the turn of a festival's day each town's stores buy from its
    // food shops a meal's worth for each wolf living in it, less the food they hold: meals first, then what feeds most
    // for the money; no more than half of what a shop holds of a food, and as far as the stores can pay. With FeastAhead
    // (round 13, lane 3), bought ahead too over the week before, from what the shops hold over their shelf targets
    // (FeastAheadDays), and every transfer in 99s.
    void layInTheFeast(std::int64_t day);
    // What a House may pay out now for `part` of its week's budget (round 10, lane A: HousePosts its posts' wages,
    // HouseRoad its caravans' money and guards, HouseTops its owner's investments in its businesses): what is left of
    // that part, and never below its floor. Not a House's, the rule off, or before its first week's reckoning: spendable.
    enum HousePart
    {
        HousePosts,
        HouseRoad,
        HouseTops
    };
    std::int64_t houseMay(const std::string& house, HousePart part) const;
    // What the church may spend now on alms bought for the hungry (round 10, lane A: ChurchBudget): what is left of its
    // week's relief, over its floor. The rule off: what it holds.
    std::int64_t almsMay() const;
    // The food alms buy at a shop (round 10, lane A, the user, 2026-10-27: CheapAlms): the cheapest on sale there that
    // feeds a wolf (AlmsFeeds of nourishment, a loaf's: a wolf asking at 55 is brought under 40), else the cheapest it
    // has at all; "" for none. Its price in `price`. (Before, the most nourishing it had: a 36p meal where a 7p loaf does.)
    std::string almsFood(const std::string& shop, std::int64_t& price) const;
    static constexpr int AlmsFeeds = 15;
    // The floor (round 10): a week's wages for HouseFloorHands at its town's going rate (the model's house_week keeps
    // twelve retainers at most, staffed first), set at each week's reckoning.
    static constexpr int HouseFloorHands = 12;
    static constexpr double PartGrowth = 1.33, PartLeast = .1;   // (The model's church_week relief: last week's and a third, a tenth at least.)

  private:
    std::vector<OddJob> oddJobs_;
    std::int64_t nextOddJob_ = 0;
    // An index of the odd jobs: who holds one (resident -> its place in
    // oddJobs_), each town's (places, in order), and the farms' hires (anyone from a city may take one). Remade when the
    // list changes (oddVersion_, moved on wherever a job is posted or the day's are cleared), before residents decide.
    std::unordered_map<std::string, std::size_t> oddHeld_;
    std::unordered_map<std::string, std::vector<std::size_t>> oddByTown_, oddHiresByTown_;   // (And its businesses' hires.)
    std::vector<std::size_t> oddFarmHires_;
    std::vector<std::size_t> oddTownCrews_;          // The towns' works crews, open to any town's wolves (round 9).
    std::unordered_set<std::string> oddHiresHeld_;   // Who holds a place on a hire (any stage): one place each (round 9).
    std::uint64_t oddVersion_ = 1, oddIndexed_ = 0;
    void indexOddJobs();
    // Each odd job's farmHire, by its place in oddJobs_: made before the residents decide (markFarmHires), kept while the
    // list, the roster and the pace stand (else farmHireOf asks afresh).
    std::vector<char> farmHires_;
    std::uint64_t farmHiresVersion_ = 0, farmHiresRoster_ = 0;
    bool farmHiresHours_ = false;
    void markFarmHires();
    bool farmHireOf(const OddJob& j) const;
    // What each shop's till took in today (wages follow takings: RatwSurplus.cpp). Not saved.
    std::map<std::string, std::int64_t> takings_;
    std::unordered_map<std::string, std::int64_t> weekTakings_;   // A till's takings since its rent was last paid (doc 64).
    std::unordered_map<std::string, std::int64_t> netToday_;      // A till's sales less its buying today (WageBySales).
    void noteTrade(const std::string& seller, const std::string& buyer, std::int64_t total);
    // The labour market's day (doc 64, Phase 6; RatwLabour.cpp): each employer's posts open and filled, and its town;
    // each town's going rate; the idle share of grown wolves; each wolf's pay today and lately, and whose it is.
    std::unordered_map<std::string, int> vacancies_, hands_;
    static inline bool noteShorts_ = false;
    static inline int unseenStep_ = UnseenStep;
    static inline std::uint64_t wageRulesOff_ = 0, wageRulesOn_ = 0;
    mutable std::vector<ShortNote> shorts_;
    static inline bool noteTowns_ = false;
    mutable std::map<std::string, std::int64_t> labourCarried_, rebated_, holdingsTaxed_, sharesSent_, labourOwn_, biteSpent_;
    // Round 13, lane 2 (SurplusByNeed): what each town sent on to the capital's pot ("a town's surplus sent on" and
    // "holdings tax sent on"), a measure kept while noteTowns; each town's share of homes under PoorDays (tendHouseholds,
    // daily, not saved); and each town's pull as its share was sent this week (reckon: its rebate is cut by it).
    mutable std::map<std::string, std::int64_t> sentOn_;
    std::map<std::string, double> poorShare_, pullSent_;
  public:
    // (Round 13, lane 2: what each town sent on to the capital's pot since the last call, by its stores' account.)
    std::map<std::string, std::int64_t> takeSentOn() const { return std::exchange(sentOn_, {}); }
    // At the founding (FoundAtTarget, while noteTowns): what the towns' targets left of their share, and what each kind
    // then got in all (households, businesses, Houses, the church). A measure.
    struct FoundingMoved
    {
        std::int64_t excess = 0, homes = 0, businesses = 0, houses = 0, church = 0;
    };
    FoundingMoved foundingMoved() const { return foundingMoved_; }
  private:
    FoundingMoved foundingMoved_;
    std::unordered_map<std::string, std::string> employerTown_, payKey_, postKey_;   // (postKey_: a post -> its employer.)
    std::unordered_map<std::string, double> paidToday_, owedToday_, pay7_;   // (The going rates: memory.going.)
    double slack_ = 0;
    int grown_ = 0;                                  // (Its grown wolves of working age, at the last pass.)
    // The living's hungry share at the last labour pass, the pace's (LivingSlack: round 13, lane 1), for the mint's harm
    // to read the same; not saved (the next pass sets it before the mint reads it). Unset, the mint counts its own.
    double hungryShare_ = 0;
    bool hungryShareSet_ = false;
    void tendLabour(std::int64_t day, const std::map<std::string, LifeBody>& bodies);
    // Households and institutions (doc 64, Phase 7; RatwInstitutions.cpp).
    std::unordered_map<std::string, double> earnedToday_;   // What each account earned today (into memory.weekIncome).
    void tendIncome();                                      // Daily, first: today's earnings into the week's and month's.
    void flushEarnings();                                   // (Those so far, without the month's daily decay.)
    void institutionsWeek(std::int64_t day);                // At the reckoning: each institution's week's budget, spent.
    // At the reckoning (round 10, TaxEase): each town's rate for next week, from its stores' mean holding against
    // TaxEaseMonths of `fullTax` (this week's tax at the full rate, by town), a quarter of the way a week.
    void easeTownTax(const std::map<std::string, double>& fullTax);
    // And the rebate (TaxRebate, since round 10): over twice its target, half of the rest back to its
    // taxpayers by their week's incomes (`taxpayers`: each town's, with its income).
    void rebateTownTax(const std::map<std::string, std::vector<std::pair<std::string, double>>>& taxpayers);
    // A town's posts kept to `share` of its week's budget (doc 64, Phase 13, round 6; the model's staff_to): the watch's
    // whatever, then its own (the city's clerks, its court, its lamplighters) in an order by lot that is the same week to
    // week, as many as the rest pays a week, PostWeekDays of a post's wage (the watch's seven). Past that, a post's holder
    // is let go and the post kept empty (memory.closed, as a business's help); within it again, a post it let go is open
    // again, for succession to fill. Returns the week's cost of the posts kept.
    // With `carried` (the capital's, CarriedPosts: round 11), the small towns' day labour it carries is counted among its
    // posts after its watch and before its own: `carried` is what it asks, set to what it is given of what's left.
    std::int64_t keepPostsTo(const std::string& treasury, double share, std::int64_t day, double* carried = nullptr, double over = -1);
    // The capital's share of a town's tax by the towns' wealth (round 11, lane 4; ShareByWealth): of the tax, a tenth
    // times the town's holding over its target (0 to ShareTownMost) times the capital's want (2 less its holding over its
    // target, ShareCapitalLeast to ShareCapitalMost); and ShareSurplus a week of what the town holds over its target,
    // while the capital holds under twice its own (CapitalMonths of its income), all of it under its target.
    static constexpr double ShareTownMost = 2, ShareCapitalLeast = .5, ShareCapitalMost = 1.5, ShareSurplus = 1. / 8, CapitalMonths = 1;
    static constexpr double CarriedGrowth = 1.1;    // The carried labour's allowance: what it asked last week and a tenth.
    // The capital kept to its income (round 12, lane B; CapitalByIncome; the user, 2026-10-29): its week's budget a
    // quarter of its month's income (its tax, the towns' share: a running month), and CapitalDraw a week of what it holds
    // over CapitalReserveMonths of it, never its savings under that floor; its outgoings outside the week's parts (its
    // caravans' unsold goods bought back, rewards set aside) counted off it the next week. Relief and its watch first,
    // then the day labour (the small towns' it carries and its own, CapitalLabourRoom over what they asked at most),
    // then its own posts, its buyers' funds last: the first cut.
    static constexpr double CapitalReserveMonths = 2, CapitalDraw = 1. / 8, CapitalLabourRoom = .5;
    bool carriedKept() const { return wageRule(CarriedPosts) || wageRule(CapitalByIncome); }
  public:
    // A town's surplus where the need is (round 13, lane 2; SurplusByNeed; the user, 2026-10-29), with work by the hours.
    // A town's need: the share of its homes under PoorDays of food money (poorShare, each day's tendHouseholds), from
    // NeedFrom (nothing) to NeedFull (all of it): in round 12's run Ridgemere's was .07 on day 90, .20 on day 181, .57 on
    // day 727, the small towns' .04-.18. A town's pull toward the city (surplusPull): the city's need less its own, never
    // below nothing. At the week's reckoning (after the capital's share) a town sends NeedShare of what it holds over
    // two targets (where TaxEase has eased its tax to nothing and the rebate begins) times its pull to the capital's
    // holdings-tax pot ("a town's surplus sent on"), not the capital's income; and at its week, its pull's part of its
    // holdings tax not spent on its own relief and day labour ("holdings tax sent on"). Both inside the capital's room:
    // CapitalBiteRoom weeks of the most its crew can be paid (estateMost at the city's going rate) less what the pot holds,
    // so the pot can't pile up idle. The capital's pot then pays its crew as far as its places go, its relief past its
    // allowance and its day labour past its own; the rest is kept. Its rebate (rebateTownTax) is cut by the part sent on.
    static constexpr double NeedFrom = .05, NeedFull = .25, NeedShare = 1. / 4, CapitalBiteRoom = 2;
    bool surplusByNeed() const { return workByHours() && wageRule(SurplusByNeed); }
    double townNeed(const std::string& community) const
    {
        const auto found = poorShare_.find(community);
        return found == poorShare_.end() ? 0. : std::clamp((found->second - NeedFrom) / (NeedFull - NeedFrom), 0., 1.);
    }
    double surplusPull(const std::string& community) const { return std::max(0., townNeed(capital()) - townNeed(community)); }
    // Each town's share of homes under PoorDays of food money, by community (tendHouseholds, each day; not saved: empty
    // after a restart, no need until the next day's count). A measure too (econ_watch's model.need).
    const std::map<std::string, double>& poorShares() const { return poorShare_; }
    // What room the capital's holdings-tax pot has for the towns' surplus this week (SurplusByNeed).
    double capitalPotRoom() const;
    // Children kept (round 13, lane 2; ChildrenKept; the user, 2026-10-29), with work by the hours: none under
    // ChildWorkAge takes an odd job (a business's hire, a field's place, a crew's); and a child of a family home, not
    // apprenticed nor holding a paid post, keeps ChildAllowance (a week of the largest stipend) and passes the rest to its
    // household's purse. In round 12's run the 53 born_ residents earned 1,257,959p on hires from their first day and
    // held 343,908p on day 729, 11% of the residents' money: a newborn stood "growing up", so out of work, and its age
    // under eight wasn't a child's.
    static constexpr int ChildWorkAge = 8;
    std::int64_t childAllowance(const std::string& community) const { return pennies(7 * MostStipend, community); }
  private:
    // A House's week's budget set in its parts (round 10, lane A: HouseBudget): returns what is left of it for its works
    // and crews.
    std::int64_t houseParts(const House& h, std::int64_t day);
    static constexpr double TreasuryStaff = .5;     // The capital's posts' share of its week's budget (the model's watch).
    static constexpr int PostWeekDays = 6;          // A post's working days a week (not Restday; the watch, every day).
    void dailyRelief(std::int64_t day, const std::map<std::string, LifeBody>& bodies);
    // `lodger`: one wolf of a barracks, a bunkhouse, quarters or lodgings, its own household by its own purse (`home` its
    // ID): all its spare to its wants, no help about the home or servants for a bunk.
    std::int64_t holdingsTax(const std::vector<std::string>& members, std::int64_t pot, std::int64_t need);
    int homePlaces(const std::vector<std::string>& members, const char* what, std::int64_t day, bool untaken) const;
    void householdRule(const std::string& home, const std::vector<std::string>& members, std::int64_t pot, std::int64_t day,
                       bool lodger = false);
    void leftovers(std::int64_t day);                       // Bakers' bread that would spoil overnight, to the hungry.
    // Hands hired by the week (an institution's works, masons, retainers; a household's servants): `slots` hands at
    // `wage` a day each, for `days`, at `at`. Returns what a day of them costs.
    std::int64_t hireForWeek(const std::string& payer, const std::string& community, const std::string& kind, const std::string& what,
                             const Spot& at, int slots, std::int64_t wage, int days, std::int64_t day);
    Spot townSpot(const std::string& community, const std::string& salt) const;   // Somewhere public in town.
    // A House's crew for the week, with work by the hours (doc 64, Phase 13, round 6; and a town's works and the church's
    // masons, round 7): `slots` hands at `wage` a day in gangs of CrewGang, each at the next of `spots`. Returns what a
    // day of them costs.
    std::int64_t houseCrewForWeek(const std::string& house, const std::string& community, const std::string& kind, const std::string& what,
                                  const std::vector<Spot>& spots, int slots, std::int64_t wage, std::int64_t day);
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
    // How far a wolf lives from a spot: from its home (a lodging at a farm's bunkhouse while it lasts) straight across the
    // map, in tiles (LifeDay::onMap); -1 where either isn't known.
    double livesFrom(const std::string& id, const Spot& at) const;
    // A field's place full, with work by the hours (doc 64, Phase 13, round 9; the user, 2026-10-27: "The ones travelling
    // from furthest away should be released first in favor of someone closer."): the taker that gives way to `id`, a
    // wolf of another town than the field's and than `id`'s, still on its way to it today (no work done there yet), living
    // farther from it than `id` does, the farthest first. "" for none, or a place not full.
    std::string givesWayTo(const OddJob& j, const std::string& id) const;
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
    // it is set if one would be (deciding at once, RatwResidents.cpp). `community`: its communityOfResident, if known.
    // `crewFree`: a grown wolf free for an institution's crew at any hour of its day (round 7).
    const OddJob* oddJobFor(const std::string& id, const Position& job, bool jobless, int age, bool poor, double hour,
                            std::int64_t ownDayPay = 0, bool* claims = nullptr, const std::string* community = nullptr,
                            bool crewFree = false);
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
    // (A step's seconds of its work, at the second's game day: a hand's day on the land brought in then, HandsBringIn.)
    void advanceOddJob(const std::string& id, int seconds, double absoluteDay);
    // Made into `account` as starting money: counted as made, and kept out of the month's profit.
    void startingMoney(const std::string& account, std::int64_t coins);
    // The realm's labourer's day as it is founded (doc 64, Phase 13): in a land of WorldMoneyResidents or more, whose
    // money is made up to WorldMoney, the going rate starts at ladder.json's `foundingDay` (the realm's and every town's,
    // memory.going), so its tills' floats, its first wages and employers' books and its prices start there. Before its
    // purses are made, once; a village made for a test keeps the catalogue's day. An older save founding its purses now
    // has its employers' books moved with their towns' rates, so its first measure doesn't pull the day back (each post's
    // employer told by its holder's age as the labour market tells it: its body's, if `bodies` has it, else its spec's).
    void foundWage(const std::map<std::string, LifeBody>* bodies = nullptr);
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
    mutable bool housesTold_ = false;                       // (Built with the world's towns: houses().)
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
    // A great house's seat, its rooms (its head's work room, and that room's floors above and below) -> its head's
    // position (doc 64, Phase 13, round 6: the chambers above, the kitchen below; with work by the hours).
    mutable std::unordered_map<std::string, std::string> seats_;
    // Each home cell's household's richest, and its richest not in service (one whose own paid post is worked at home:
    // a live-in servant), for the day. Filled at fixed points of the society's own day (rollRichest: the day's turn,
    // before the labour market, and a day's first decision with none yet), never by whoever asks first: a talk scene or
    // a harness asking payerOf leaves the run as it was (doc 64, Phase 13, round 6). Asked before they're known, they're
    // worked out for that asking alone.
    std::map<std::string, std::string> richest_, richestFree_;
    std::int64_t richestDay_ = -1;
    void rollRichest();
    std::string richestAt(const std::string& homeCell, const std::string& besides, bool free = false) const;
    std::string richestIn(const std::string& homeCell, bool free) const;   // (Worked out afresh: richestAt's fallback.)
    bool inService(const std::string& resident, const std::string& homeCell) const;
    void indexEmployers() const;
    const House* seatOf(const std::string& cell) const;   // The house whose seat a room is (seats_), or none.
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
