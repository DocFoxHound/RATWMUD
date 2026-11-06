#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "RatwAppearance.h"
#include "RatwItems.h"

// The residents' society: their lives (schedules, posts, careers, families), and the money and goods they hold.
//
// The economy's rules are erased (Docs/Design/66-worlds-atlas-and-dm.md, Phase 5, 2026-10-30): nothing here pays
// wages, taxes, rents or tithes, nobody produces, crafts, buys or sells on its own, and no price follows anything but
// the catalogue. What stays is money itself (accounts in whole pennies, conserved: shift, sale, the ledger), the
// catalogue's goods, shops that hold stock (each post's till) at catalogue prices, and players trading with them.
// The residents' needs that their buying met (food) are paused (NeedsPaused): until the user's own economy rules say
// how they are met, nobody grows hungry. Where those rules plug in is named in doc 66's Progress, "Phase 5".

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
    int wagesToday = 0;                             // (The demo roster's: its keeper's service contracts today.)
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
// The accounts of things rather than people: a town's stores ("stores:<town>"), a caravan's load ("caravan:<id>"), a
// bandit camp's loot ("bandits:<id>"), a contract's reward held in trust ("contract:<id>"), a shop's till
// ("till:<position>"), a home's stores ("home:<cell>:<kind>")...
bool facilityAccount(const std::string& id);
// Goods the economy knows: herbs, meals, the sword, and the catalog's goods (doc 35).
bool itemValid(const std::string& item);
// The most kinds of goods one account holds (a wardrobe of clothes, a jeweller's counter).
constexpr std::size_t MaxGoodsKinds = 64;
// The longest task a saved resident may have: its job's title, or "apprenticed: " and its master's (was 40, which a
// long title such as "apprenticed: cooking for the King's household" passed, so the save it made would not load).
constexpr std::size_t MaxTaskText = 160;

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
    // Restday's service: where the townsfolk sit in the church, and where its preacher stands.
    std::vector<Spot> pews;
    Spot pulpit;
};
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
    bool paid = true;     // (As authored: nothing pays a post since doc 66's Phase 5.)
    double level = 0;     // The skill its trade needs (Society::tradeLevel; its business's hardest craft).
};
// Who holds a position now, who is learning it, and since when it has stood empty (-1: it hasn't).
struct PositionState
{
    std::string holder, apprentice, lastHolder;
    double vacantSince = -1;
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
    // Births follow deaths (the user, 2026-11-01: "Birthrates should match death rates for this game"): each resident who
    // dies owes the world one child, born to a couple at home a few days later (Society::tendCareers). The dead -> the
    // day the child is due. Players never count: only residents die into it.
    std::map<std::string, double> birthsOwed;
    std::int64_t day = -1;                            // The last day careers were tended.
};
// A resident the world should gain: a child, in place of one who died, or an unplaced NPC of the world's own list come in
// from the Country for a post nobody in the city can fill (the user, 2026-11-01: "I never said to simulate new NPCs or
// players, so don't do that. If you need newcomers for the economy simulate someone from our preexisting NPC list
// coming in from the country"). No other NPC is ever made. The society decides that one is wanted; the host makes the
// child (a live world adds a row to live.npcs, cloned from its mother's) or places the arrival (World::arriveResident;
// a live world gives its row the home), then calls welcome() with their ID.
struct ResidentRequest
{
    std::string kind;             // "birth" or "arrival".
    std::string templateId;       // Birth: the mother, whose record the child's is cloned from. Arrival: the unplaced NPC.
    std::string name;
    int age = 0;
    Spot home;
    std::string positionId;       // Arrival: the post they come for.
    std::string inPlaceOf;        // Birth: the dead it is born in place of (owed again if it can't be made).
    std::int64_t purse = 0;       // Arrival: the pennies they bring, paid by the Country (WharfFund).
    double skill = 0;             // Arrival: what it knows of the post's work (its trade suiting it: as the trade needs).
    std::vector<std::string> parents;
    // Birth: the child's sex, "male" or "female" (characters are male or female only: the user), decided with the
    // request (birthSex) so a deterministic run repeats; its pronouns follow (Game::pronounsOf). Empty: as the template.
    std::string sex;
    // "farm" (Docs/Design/71-the-country-s-industries.md, Phase C): an unplaced NPC placed by the farms' rule when a farm's
    // area is switched on (Society::staffFarms): a farm's keeper or hand, a community market's keeper or hand (a post of
    // its own: `work`, `workLabel`, `role`, the hours), or a keeper's family (no work: workLabel "-"). Its purse, `purse`,
    // moves from `purseFrom` (a farm crew's founding purse, or the Country's) as far as it holds it: nothing is made.
    // `farm`: its farm, or "market:<community>".
    Spot work, serve;                             // (serve: a market keeper's customers' spot, found by the world.)
    std::string workLabel, role, purseFrom, farm;
    double startHour = 8, endHour = 17;
};
// A child's sex, half and half, from the couple's key and the day (the world's own deterministic mix, no clock or
// global random state): "male" or "female".
std::string birthSex(const std::string& couple, std::int64_t day);
// Skill families (a placeholder until skills are designed): what a job's skill is mostly about, so that some of it
// carries to a similar job. "keeping the inn" and "keeping the stall" are both trade; a sawyer and a quarryman labour.
const char* skillFamily(const std::string& title);
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
    // The communal lodgings within reach of work in a cell (a spot in each), for an arrival with no household of its
    // class to lodge with; null: none.
    std::function<std::vector<Spot>(const std::string& workCell)> lodgings;
    // Whether the world has a place (a cell or an interior): the farms' people are placed only where their places are
    // built (doc 71, Phase C). Null: nowhere is, so nobody is placed.
    std::function<bool(const std::string& cell)> place;
};

// What the self-correcting rules have drifted to (doc 68, Progress, "Self-correcting rules"), saved with the society (its
// save's "loop", only when there is any): each shop's price factors from its stock and its wares' sales a day, smoothed;
// each till's wage step and the days it is being judged on; the treasury's slow level, and the toll and alms it has
// moved. Nothing here is money: the coins stay in the accounts.
struct LoopState
{
    std::map<std::string, std::map<std::string, double>> prices;   // Till -> ware -> its price factor (only those not 1).
    std::map<std::string, std::map<std::string, double>> sales;    // Till -> ware -> pieces sold a day, smoothed.
    struct Wages
    {
        int step = 0;                               // Pennies on each of its posts' wages (the floor aside).
        int days = 0, shortDays = 0;                // The days judged so far, and those it could not pay its posts;
        std::int64_t owed = 0, profit = 0;          // what it owed them, and its keeper's profit, over those days.
    };
    std::map<std::string, Wages> wages;             // Till -> its wage step and its window.
    double level = -1;                              // The treasury's slow mean at 18:00 (-1: not begun),
    int levelDays = 0;                              // over so many nights (its first treasuryDays: their plain mean).
    double levelThen = -1;                          // The mean when the toll and alms were last judged,
    std::int64_t judged = -1;                       // and that day.
    double toll = 0;                                // Points on the world's gate toll.
    int alms = 0;                                   // Pennies a head on almsADay.
    // The Wharf's book with the world beyond the realm (Docs/Design/73-the-wharf.md, 4.6; the save's "loop" "wharf"):
    // the pennies the beyond has paid in (minted: its takes, its export basket, carriage and the fee), those the realm has
    // paid out to it (sunk: its imports, less the fee), the fees the Wharf kept, the moves of the credit by the DM and the
    // standing rules, all since the world began; and the trade credit now (what the realm has paid beyond and not had back,
    // moved by the DM; the beyond buys only within it). Money too is counted in SocietyState::minted and sunk.
    struct Wharf
    {
        std::int64_t in = 0, out = 0, fee = 0, moved = 0, credit = 0;
        bool empty() const { return in == 0 && out == 0 && fee == 0 && moved == 0 && credit == 0; }
    };
    Wharf wharf;
    bool empty() const
    {
        return prices.empty() && sales.empty() && wages.empty() && level < 0 && levelDays == 0 && judged < 0 && toll == 0 &&
               alms == 0 && wharf.empty();
    }
};
struct SocietyState
{
    bool enabled = false;
    std::map<std::string, EconomyAccount> accounts;
    std::map<std::string, ResidentLife> residents;
    std::vector<EconomyEntry> ledger;
    std::int64_t minted = 0, sunk = 0, nextEntry = 1, budgetDay = 0;
    int exportsRemaining = 8, importsRemaining = 4, herbPatch = 40;   // (The demo roster's.)
    double decisionRemainder = 0;
    CareerState careers;
    LoopState loop;
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
    // Its business may draw from the store room (Docs/Design/67-ridgemere2-city.md, 6: Atlas's per-business switch, a
    // "nostore" record for off). Read for the position it founded: the flag follows the business, not who keeps it.
    bool internalStore = true;
};
struct PatrolRoute
{
    std::string id;
    std::vector<Spot> posts;
};
// What a world's treasury starts with (the capital's account, "treasury").
struct EconomySpec
{
    std::int64_t treasury = 1000;
    int storeHerbs = 100, storeMeals = 50;
    // The store room (Docs/Design/67-ridgemere2-city.md, 6; the world's "store" record, live.economy): makers drawing
    // their crafts' inputs (internalStore, at storeMarkup percent of the catalogue's price), vendors drawing what they
    // sell (storeResale, at resaleMarkup), and the restocking rule that has keepers draw at work (storeRule: off until
    // the user confirms it, doc 67 Q2). Defaults: on, 100, on, 75, off.
    bool internalStore = true;
    int storeMarkup = 100;
    bool storeResale = true;
    int resaleMarkup = 75;
    bool storeRule = false;
    // The city's loop (Docs/Design/68-the-country-and-the-city-loop.md; the world's "loop" and "countryoff" records,
    // live.economy, which keep the Country's words: doc 73 renamed the fields, not the records or columns): residents'
    // needs (needsPaused: doc 66 Phase 5's pause, off by default), the estates spending back what they take in (wharf,
    // keeping wharfReserve), the gate toll (gateToll percent of each draw's cost
    // to the treasury), carriage (pennies a wagon load of CarriageLoad weight, in and out), the day's wages by kind of post,
    // and the goods the Country no longer supplies (wharfOff: their real producers' now).
    bool needsPaused = false;
    bool wharf = true;
    std::int64_t wharfReserve = 200;
    int gateToll = 30;
    int carriage = 6;
    int wageKeeper = 14, wageHand = 8, wagePublic = 8, wageWharf = 8;
    std::set<std::string> wharfOff;
    // Services the treasury pays a share of (doc 68, 5: the city's taxes helping its businesses), by service, in percent:
    // "carriage" (a share of every delivery's carriage, in and out, paid to the carters). Only from what the treasury
    // holds over its float and tonight's public wages, and at most a twentieth of its money a day (SubsidyDay).
    std::map<std::string, int> subsidies{{"carriage", 25}};
    // The residents' comforts (doc 68, 3; the world's "comforts" record): pennies a head a household holds before it buys
    // them, and the percent of what is over that it spends a day. -1: city_loop.json's (40, a fifth).
    int comfortsAbove = -1, comfortsShare = -1;
    // The loop's rules as this world has changed them (doc 68, Progress, "First fixes"; "rule" records, migration 0058),
    // by name (items::Rules); a rule not listed is as city_loop.json says.
    std::map<std::string, int> rules;
    // The city's services as this world has changed them ("service" records): on or off, the price, about once every
    // `every` days a household. A service not listed is as city_loop.json and items.json say; one they no longer have is
    // kept and does nothing.
    struct Service
    {
        bool on = true;
        int price = 1, every = 30;
    };
    std::map<std::string, Service> services;
    // The seasons in the Country's supply (city_loop.json's `seasons`; doc 68, Progress, "Seasons and the DM's rules"), a
    // group at a time as this world has changed them ("season" records): supply, price and shelf by season in percent,
    // and the group's normal pieces a day. A group not listed is as city_loop.json says.
    struct Season
    {
        std::array<int, 4> supply{100, 100, 100, 100}, price{100, 100, 100, 100}, shelf{100, 100, 100, 100};
        int normal = 0;
    };
    std::map<std::string, Season> seasons;
    // The Country's food farms as this world has changed them (Docs/Design/71-the-country-s-industries.md, 4; migration
    // 0060, live.economy.farms; the "farms", "farmarea" and "farm" records). -1 (an empty order) is country_farms.json's.
    // The switch: its first area on day `start` (the calendar's day; -1: none; -2: country_farms.json's), the next each
    // `every` days after it, in `order`; the farms' wages. An area's: on (-1: as the schedule says; 0 off; 1 on), the
    // percents on its farms' yields and hands, its wages, its price a year in percent. A farm's: the percents on its yields
    // and hands, its wages.
    struct Farms
    {
        int start = -2, every = -1, keeper = -1, hand = -1;
        std::vector<std::string> order;
        struct Area
        {
            int on = -1, yieldScale = -1, crewScale = -1, keeper = -1, hand = -1, priceFactor = -1;
        };
        struct Farm
        {
            int yieldScale = -1, crewScale = -1, keeper = -1, hand = -1;
        };
        std::map<std::string, Area> areas;
        std::map<std::string, Farm> farms;
    };
    Farms farms;
};
// A family as authored (doc 62, decision 8): `a` is `b`'s spouse, or `a` is `b`'s parent.
struct Kin
{
    std::string a, b, kind;
};
struct AuthoredRoster
{
    std::vector<ResidentSpec> residents;
    // NPCs without a home (Docs/Design/66-worlds-atlas-and-dm.md, 2): in the roster, with their job if they have one,
    // their talk and their families, but with no body, no needs, no work and no money: residents who are away. Never
    // in `residents`, the society's state or its ledger; a build or a push that gives one a home makes them a resident.
    std::vector<ResidentSpec> unplaced;
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
    // An unplaced NPC as authored (doc 66, 2), or null: not a resident (spec() never finds one).
    const ResidentSpec* unplaced(const std::string& id) const;
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
    // gift). Nothing is made or lost; what the receiver can't hold stays put. False if nothing moved.
    bool shift(const std::string& from, const std::string& to, const std::string& item, int quantity, std::int64_t coins,
               const std::string& kind);
    // A Gift lent to a maker (doc 43: a Gifted wolf's Forge Heat at a smithy, Clay Hand at a potter's...): what it would
    // add to the maker's next batch made by `untilDay`, and a paw lent (doc 53, 3: a player handing at a resident's
    // workshop), its batches begun by `untilDay` quicker by `rate`. Kept for the economy rules to come (nobody makes a
    // batch since doc 66's Phase 5): giftLiftOf and pawRateOf read them. Not saved.
    void lendGift(const std::string& maker, double lift, double untilDay);
    double giftLiftOf(const std::string& maker) const;
    void lendPaw(const std::string& maker, double rate, double untilDay);
    double pawRateOf(const std::string& maker) const;
    // A sale: goods from `seller` to `buyer` at `price` each, paid at once.
    bool sale(const std::string& seller, const std::string& buyer, const std::string& item, int quantity, std::int64_t price,
              const std::string& kind)
    {
        return transfer(seller, buyer, item, quantity, price, kind);
    }
    // Goods used up (eaten by bandits, say): gone from the world, recorded as `kind`.
    int consume(const std::string& account, const std::string& item, int quantity, const std::string& kind);
    // A facility account (see facilityAccount), empty to begin with; closing one needs it empty.
    bool openAccount(const std::string& id);
    bool closeAccount(const std::string& id);
    // Home storage (Docs/Design/36-home-storage.md): each household's larder (food), chest (goods), wardrobe (wear)
    // and woodpile (fuel), an account each ("home:<home cell>:<kind>"), owned by everyone who lives there.
    static constexpr const char* StoreKinds[] = {"larder", "chest", "wardrobe", "woodpile"};
    static std::string homeStore(const std::string& homeCell, const std::string& kind) { return "home:" + homeCell + ":" + kind; }
    // Opens the stores of these homes where they don't exist yet (empty: what goes in them is the households' own).
    void furnishHomes(const std::set<std::string>& homeCells);
    // Where each home's stores stand, by home cell and kind (the world places them); a store not placed yet is
    // reached at its household's home spot.
    void setHomeStores(std::map<std::string, std::map<std::string, Spot>> spots) { homeStores_ = std::move(spots); }
    const std::map<std::string, std::map<std::string, Spot>>& homeStores() const { return homeStores_; }
    // The world's towns changed (World::setupTowns): what the residents' records keep of their communities is forgotten.
    void forgetPlaces() { ++rosterRevision_; }
    // Where each resident sleeps (the world gives each a place on a bed in their home, up to four to a bed); without
    // one, their home spot.
    void setBeds(std::map<std::string, Spot> beds)
    {
        beds_ = std::move(beds);
        ++rosterRevision_;                          // (The residents' records keep their beds.)
    }
    // Marriage: both must be unmarried; from now on they are family.
    bool marry(const std::string& a, const std::string& b);
    const std::string* spouse(const std::string& resident) const;
    // Residents wanted since the last call (see ResidentRequest), and a made one taking their place in the society.
    std::vector<ResidentRequest> takeRequests();
    CareerNote welcome(const ResidentRequest& request, const std::string& id);
    // A child the host could not make: owed again, due now.
    void birthFailed(const ResidentRequest& request);
    // An arrival from the Country (ResidentRequest "arrival"): the unplaced NPC becomes a resident at the home chosen for
    // it, placed without work (it holds the post it came for once welcomed), with the purse the request names paid from
    // the Country's account as far as it holds it (an unplaced NPC's purse is outside the economy: no money is made).
    // False if it is no longer unplaced.
    bool arrive(const ResidentRequest& request);
    std::size_t unplacedCount() const { return authored_.unplaced.size(); }
    // Jobs by class (Docs/Design/67-ridgemere-2-the-city.md, Progress, "Jobs by class"): a post's class, "upper" (a House's
    // own, a larger independent business, high office), "middle" (a shop, a stall, a counter, a Great House's works run for
    // it, lower office), "lower" (hands, waged and service work, the Country's labour), "any" (the Watch, the clergy: their
    // own cases), or "" (no post that pays: lessons, play, leisure); and a wolf's class, resident or unplaced ("upper",
    // "middle" or "lower"): its own work's as written, else its family's, else its household's, else middle.
    std::string postClass(const Position& p) const;
    std::string classOf(const std::string& id) const
    {
        auto cls = classOf(id, 0);
        return cls.empty() ? std::string("middle") : cls;
    }
    // How the empty posts were filled since the start (not saved): "the apprentice steps up", "from the city", "from the
    // Country", ... -> how many.
    const std::map<std::string, int>& fills() const { return fills_; }
    // The best a resident is at any job of this family (see skillFamily).
    double familySkill(const std::string& resident, const std::string& family) const;
    // A player asks to learn a position's trade from its holder.
    CareerNote apprentice(const std::string& player, const std::string& positionId, const CareerWorld& world, double day);
    // A player's help with a resident's trouble (Docs/Design/57-changing-the-world.md, 3): a vacant post given to a resident
    // now, as succession would give it (leaving any other post it held); a youth apprenticed now to a position's holder,
    // under the weekly chance's own rules (12 to 25, no master yet, no work elsewhere; a master of 35 or skilled 70, not
    // the watch). "refused" notes, or what changed.
    CareerNote appoint(const std::string& positionId, const std::string& resident, const CareerWorld& world, double day);
    CareerNote apprenticeTo(const std::string& positionId, const std::string& youth, const CareerWorld& world, double day);
    void addPlayer(const std::string& id);
    const EconomyAccount* account(const std::string& id) const;
    std::int64_t cash(const std::string& id) const     // An account's money (0 for none).
    {
        const auto* a = account(id);
        return a ? a->cash : 0;
    }
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
    // A harness may lengthen it (setUnseenStep: world_check's --unseen-step and --fast, an economy run's fewer, longer
    // decisions; doc 69). The game keeps UnseenStep.
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
        ++dayRevision_;                             // (The records' towns.)
    }
    const LifeDay& day() const { return day_; }
    // How the residents' deciding is spread over threads (the world's runner, World::setParallel): run(count, job) calls
    // job(i) for every i below count and returns when all are done. Unset, the game thread decides alone, the same way.
    using Parallel = std::function<void(std::size_t, const std::function<void(std::size_t)>&)>;
    void setParallel(Parallel run) { parallel_ = std::move(run); }
    bool atStall(const std::string& merchant) const { return atStall_.count(merchant) > 0; }
    // A player's trade with a shop (its keeper `merchant`, keeping it): its goods (wares) at the shop's price now (the
    // catalogue's times shopFactor: the season's and the shop's from its stock; doc 68, Progress, "Players in the shifting
    // market"), in whole pennies a piece, a tenth off at a market stall; and what it buys from a player at SellBackShare
    // of that price. What the shop's list, the keeper's word and the trade itself say is this one price.
    EconomyResult quote(const std::string& player, const std::string& merchant, const std::string& item,
                        int quantity, bool buy) const;
    EconomyResult trade(const std::string& player, const std::string& merchant, const std::string& item,
                        int quantity, bool buy);
    // A good's catalogue price a piece, in pennies (its quality's: items.json, doc 35): every shop's.
    static std::int64_t catalogPrice(const std::string& item);
    static constexpr double SellBackShare = .55;   // What a shop pays a player for a good (doc 15: a little over half).
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
    // A resident made in the running world (a birth, a newcomer), with its purse (new money).
    bool addResident(const ResidentSpec& resident, std::int64_t purse);
    // Takes patrol routes and every resident's wander area from `from` (a validated candidate society).
    void adoptLayers(const Society& from);
    static int stock(const EconomyAccount& account, const std::string& item);
    // A good in every quality held (doc 35, Part 4): a maker's hides, common, crude, fine and masterwork together.
    static int stockAll(const EconomyAccount& account, const std::string& base);
    static std::vector<std::string> kindsHeld(const EconomyAccount& account, const std::string& base);
    // Food (doc 35, Part 7): anything of the catalog that feeds (drinks aside), how much, and the best an account holds
    // to eat; what to eat first (the food that spoils soonest, Item::keeps, the most nourishing of those).
    static bool edible(const std::string& item);
    static int nourishment(const std::string& item);
    static std::string bestFood(const EconomyAccount& account);
    static std::string eatFirst(const EconomyAccount& account);
    static bool hasFood(const EconomyAccount& account);   // !bestFood(account).empty(), without building it.
    // Needs paused (Docs/Design/66-worlds-atlas-and-dm.md, Phase 5): a resident's hunger neither grows nor sends it
    // anywhere, nobody buys, nobody is paid and the Country doesn't spend; a hungry one (a save's) still eats what it
    // carries. A switch since doc 68 (EconomySpec::needsPaused), off by default: the city's loop runs. Always paused
    // outside an authored world.
    bool needsPaused() const { return roster_ != Roster::Authored || authored_.economy.needsPaused; }
    // Goods made (not bought: a grant, a player's work): only goods, never money.
    bool create(const std::string& account, const std::string& item, int quantity, const std::string& reason);
    bool merchant(const std::string& id) const;
    // A smith: a merchant whose trade is the forge (its work says "smith" or "forge"; the demo's Ash). A smith deals in
    // swords.
    bool smith(const std::string& id) const;
    // What a merchant sells (Docs/Design/39). A shop of a kind in Data/Items/businesses.json (found by its keeper's work
    // label: "baker at The Amber Loaf") sells what its crafts make and what it supplies (crafts.json), else a handful of
    // its kind's cheap goods, the same handful every day but not every shop of the kind the same; and what it buys from
    // players (doc 41). A smith adds swords; a shop of food adds meals, an herbalist herbs. Anyone else (an innkeeper, a
    // market trader, the demo's keeper) deals in herbs and meals.
    std::vector<std::string> wares(const std::string& merchant) const;
    // A shop of food (a general store, a provisioner, a stall, a bakery, a butcher, a fishmonger, a brewery, an inn).
    bool foodShop(const std::string& merchant) const;
    // A shop's account (its shelves and its money): its post's till ("till:<position>"), or its keeper's own purse for
    // one that has none (the demo's, a merchant without a post).
    std::string tillOf(const std::string& keeper) const;
    // The store room (Docs/Design/67-ridgemere2-city.md, 6): a business's craft inputs (making goods: the inputs of its
    // own crafts, intermediates too) and the goods it resells (its supply list and the kinds of wares its founder started
    // with that its crafts don't make), without end, at the catalogue's price times a markup, Common quality. Not a place:
    // its keeper draws into the position's till, and the coins go to StoreFund, kept in the world (conserved). Who may
    // draw: the keeper of a merchant position with a till, whose business has goods to draw, while the world's switch
    // for that kind of good is on and the business's own (its founder's ResidentSpec::internalStore; a founder whose
    // spec is gone counts as on).
    // Since doc 68 the coins go to the Country, the Wharf since doc 73 (WharfFund), less the gate toll (to the treasury);
    // carriage on the goods goes to the carters. StoreFund is the old account, read only to move an older save's coins.
    static constexpr const char* StoreFund = "fund:store";
    // The Wharf's purse (doc 73: the Country's, "outside:country", renamed; a save's old purse moved into it at load).
    static constexpr const char* WharfFund = "outside:wharf";
    static constexpr const char* OldCountryFund = "outside:country";
    static constexpr int StoreDrawMost = 1000;        // The most of one good a draw takes.
    enum class StoreUse
    {
        None,
        Make,
        Resell
    };
    bool storeOpen(const std::string& keeper) const { return storeClosed(keeper).empty(); }
    // Why the store is closed to this keeper ("" when it is open).
    std::string storeClosed(const std::string& keeper) const;
    // Its goods for the modes that are on (making goods sorted first, then resale goods), kept per keeper like wares.
    const std::vector<std::string>& storeGoods(const std::string& keeper) const;
    StoreUse storeUse(const std::string& keeper, const std::string& item) const;   // Make wins when an item is both.
    // ceil(catalogPrice × quantity × markup / 100): 0 at a markup of 0 (free: goods move, no money), else at least 1.
    std::int64_t storeCost(const std::string& keeper, const std::string& item, int quantity) const;
    EconomyResult drawFromStore(const std::string& keeper, const std::string& item, int quantity,
                                const std::string& why = "from the store room");
    // The restocking rule (doc 67, 6; EconomySpec::storeRule, off by default): a keeper at work, at most once a game
    // hour, brings each ware below its starting stock back: one batch made from drawn inputs where its crafts make it,
    // else drawn up to its starting stock. What was done (empty for nothing).
    std::vector<std::string> restockFromStore(const std::string& keeper, double absoluteDay);
    // For tests: the next draw's goods fail to arrive after it is paid for (its money must come back).
    void failNextStoreCreate() { failStoreCreate_ = true; }
    // The Wharf (Docs/Design/73-the-wharf.md): the realm's valve to the world beyond it. With the rule wharfValve on it
    // lands what the realm's farms can't fill of a good on its imports list (the last lot of a store draw, within the
    // good's most a day) and every good no area grows (the Country's, as before, within the season's cap), paying the
    // beyond for them (the realm's money out, less its fee); it takes what piles on the farms and works over
    // wharfKeepDays of their yield of a good on its takes list (at 16:00); and it buys the city's exports with what credit
    // is left. Every price is the stable price: the store room's at markup 100 and the season's (or the area's year)
    // factor, never a shop's drifted one.
    bool wharfValveOn() const { return rule("wharfValve") > 0; }
    std::int64_t stablePrice(const std::string& item, int quantity) const;
    // Whether the Wharf lands a good now (its imports list, on, not refused, not off for this world, the valve on), and
    // how many more pieces of it it may land today (its most × wharfImportScale, less what it landed today); a good no
    // area grows, the season's cap as before (supplyLeft).
    bool wharfImports(const std::string& item) const;
    int wharfRoom(const std::string& item) const;
    // Whether a landing of the good is a true import, paid beyond the realm: on the imports list (on) after its farms, or
    // a good no area names. An area's good whose farms aren't on yet is landed as the Country did, its takings kept.
    bool wharfPaysBeyond(const std::string& item) const;
    // Whether it takes a good (its takes list, on, not refused, the valve on), and what a farm keeps of it: the larger
    // of wharfKeepLeast and wharfKeepDays of its yield a day today.
    bool wharfTakes(const std::string& item) const;
    int wharfKeep(std::size_t farm, const std::string& item) const;   // (`farm`: its index in farms().)
    // What the beyond may still pay in today: the credit, never past wharfExportMost a day, never under 0.
    std::int64_t wharfExportRoom() const;
    // An order from the Wharf (doc 73, 5): `n` pieces of a good the Wharf lands, at the stable price, into `buyer`'s
    // account, carriage by weight to the carters (the treasury's share as any draw's), the gate toll to the treasury;
    // whole or nothing. The towns to come, the farms' baskets' fallback; a store draw's Wharf lot is booked the same way.
    EconomyResult orderFromWharf(const std::string& buyer, const std::string& item, int n, const std::string& why);
    // The DM's lever (doc 73, 4.6, the action wharf.credit): move the trade credit by `amount` (−: the beyond buys less,
    // money drains as the realm imports; +: it buys more, money comes in as goods bought), at most wharfActionMost
    // either way (0: refused). Recorded; no coin moves now.
    EconomyResult moveWharfCredit(std::int64_t amount, const std::string& who, const std::string& note);

    // The city's loop (Docs/Design/68-the-country-and-the-city-loop.md, RatwCityLoop.cpp). Each game hour: households
    // buy food, needs and comforts (8:00-18:00), the Country buys its basket (8:00-18:00), wages are paid (18:00), shelves
    // are reckoned from the day's sales (0:00). Runs from decide() while !needsPaused().
    static constexpr int ShopOpen = 8, ShopShut = 18, PayHour = 18;
    // A resident needs about this much nourishment a day (hunger grows .0035 a second; food counts 1.1 times when eaten).
    static constexpr double NourishADay = .0035 * 14400 / 1.1;
    static constexpr std::int64_t TillFloat = 20, TreasuryFloat = 200;
    // (The city's alms, doc 68, 4: the rule "almsADay" now, a day's food money a head and some over; it was 7.)
    static constexpr int CarriageLoad = 1000;         // A wagon load, in items.json's weight.
    static constexpr const char* CarriageFund = "fund:carriage";   // Carriage where a world has no carters' yard.
    // A market's purse ("outside:<id>"), and which market supplies a good (the first in city_loop.json: one for now).
    static std::string marketFund(const std::string& market) { return "outside:" + market; }
    std::string marketFor(const std::string& item) const;
    // Who pays a post: its kind ("keeper", "hand", "watch", "public", "estate"; "" for none), the paying account and the
    // wage. A shop's keeper and its hands (a post whose work room holds exactly one shop): its till; the watch: the
    // treasury; otherwise the first city_loop.json wage rule with a word in its title; otherwise nobody.
    struct Pay
    {
        std::string kind, payer;
        int wage = 0;
        int rule = -1;                              // The city_loop.json wage rule that pays it (-1: none of them).
    };
    Pay payOf(const Position& post) const;
    // The Houses' estates (doc 72, Phase 6; items::Country::estates, the rule "estates"): each House's own purse, its
    // lands' rents in, its people's wages out, the Crown's covering the others' short nights. With the rule off (or no
    // estates in city_loop.json) every one is the Country's purse, as before. An estate's purse ("estate:<id>", or the
    // Country's for Grayrock's, which runs the wharf and the sea trade), and the estate of a House ("the Crown",
    // "Greatly"...), of a Country post (its label's words, then its workplace's), and of a purse (nullptr: none).
    bool estatesOn() const;
    static std::string estateFund(const items::Estate& estate);
    static const items::Estate* estateOfHouse(const std::string& house);
    const items::Estate* estateOfPost(const Position& post) const;
    static const items::Estate* estateOfFund(const std::string& fund);
    // Where a House's money goes tonight: its estate's purse while the estates are split and it is open, else the Country's.
    std::string houseFund(const std::string& house) const;
    // The Country's posts held tonight (the posts payWages pays), their wages by paying purse: the Country's, or an estate's.
    std::map<std::string, std::int64_t> estatePostsOwed() const;
    // What a Country or estate purse keeps before it buys (its basket, the Houses' tables, the patrons): its reserve (the
    // Country's wharfReserve; split, each estate its share by its posts' wages) and estatePostsDays nights of its
    // posts; the Crown's also what the others lack of their own nights (its grants to come). `owed`: estatePostsOwed().
    std::int64_t estateKeep(const std::string& fund, const std::map<std::string, std::int64_t>& owed) const;
    // What a Country or estate purse may spend now (its basket, the Houses' tables, the patrons): what it holds over its
    // keep; split (doc 72, after estates6), never more than all the estates together hold over the one purse's keep (the
    // Country's reserve and estatePostsDays nights of all their posts), so that together they buy no more than the one
    // purse would have, one estate's spare never spent while a sister estate lacks its posts.
    std::int64_t estateSpare(const std::string& fund, const std::map<std::string, std::int64_t>& owed) const;
    // The estates' night, before the wages (doc 72, Phase 6): open their purses (the first time, the Country's money split
    // between them by their posts' wages); the Crown's levy on the others' spare (over their keep, up to what the Crown
    // lacks of its own; and, the Crown short of tonight's posts and grants, on what they hold over tonight's posts: after
    // estates6); and the Crown's grants to those that can't pay tonight's posts; with the rule off, any estate's money
    // back in the Country's purse.
    void settleEstates();
    // The households that buy together (doc 68, 3): by home, joined by kin (a child under 16, a spouse), by a key each.
    // (Public for world_check's --economy-detail, which reads it.)
    std::map<std::string, std::vector<std::string>> households() const;
    // A shop's crew (its keeper and hands): the hours an hour it can give to making.
    int crewOf(const std::string& till) const;
    // The carters' till that takes carriage now (the one holding least), or CarriageFund where there is none.
    std::string carters() const;
    // Carriage on so much of a good, in whole pennies, owed by `payer` (fractions kept against it until they make a penny).
    std::int64_t carriageOn(const std::string& payer, const std::string& item, int quantity, bool commit, double share = 1);
    // The subsidised services (doc 68, 5), and what the treasury may still give to them today: at most SubsidyDay of its
    // money at the day's turn, and never into its float or tonight's public wages (the subsidy shrinks first).
    static constexpr const char* Subsidised[] = {"carriage"};
    static constexpr double SubsidyDay = 1. / 20;
    int subsidyShare(const std::string& service) const;
    std::int64_t subsidyRoom();
    // Carriage on a delivery, split: what `payer` pays and what the treasury's subsidy pays (both whole pennies).
    std::pair<std::int64_t, std::int64_t> carriageSplit(const std::string& payer, const std::string& item, int quantity, bool commit);
    // A ware's shelf (doc 68, 7): its starting stock, or a quarter more than sold of it yesterday, at most StoreDrawMost.
    int shelfOf(const std::string& till, const std::string& ware) const;
    // The seasons in the Country's supply (doc 68, Progress, "Seasons and the DM's rules"): city_loop.json's groups as this
    // world has them (EconomySpec::seasons), and where in the year the society is (0 at Spring's first day, up to 365;
    // -1 until it first decides, when no season applies: every factor 1, no cap). A factor is the group's percent for
    // the season whose middle day is nearest on either side, blended in a straight line, as a fraction (1: normal).
    const std::vector<items::SeasonGroup>& seasonTable() const { return seasons_; }
    const items::SeasonGroup* seasonGroup(const std::string& item) const;
    double yearDay() const { return yearDay_; }
    void setYearDay(double absoluteDay);           // (decide() does it; for tests and harnesses.)
    static double seasonal(const std::array<int, 4>& percentBySeason, double yearDay);
    // The same blend of any four values (the farms' yields, pieces a day; before the date is known, their mean).
    static double seasonalValue(const std::array<double, 4>& bySeason, double yearDay);
    double priceFactor(const std::string& item) const;
    double shelfFactor(const std::string& item) const;
    double supplyFactor(const items::SeasonGroup& group) const;
    // What the Country can still supply of a good's group today (its normal × the supply factor, less what was drawn of
    // the group today); a large number for a good outside every capped group.
    int supplyLeft(const std::string& item) const;
    int groupCap(const items::SeasonGroup& group) const;
    int groupDrawn(const std::string& group) const;
    // The most `quantity` of a good costs a resident in a shop now (for whether it can pay): the catalogue's price, or the
    // season's (times its price factor) rounded up if dearer. What is paid: the catalogue's, then the difference.
    std::int64_t seasonalCost(const std::string& item, int quantity) const;
    // The self-correcting rules (doc 68, Progress, "Self-correcting rules"), as they stand now: a shop's price factor for
    // a ware from its stock (1 with the rule off), times the season's for what `quantity` of it costs a resident there;
    // the gate toll in percent and the alms a head, with what the treasury's level has moved them by; a till's wage with
    // its wage step (the floor kept), within the keeper's bound (wageDriftMost) or a hand's (wageDriftHandMost).
    double stockFactor(const std::string& till, const std::string& ware) const;
    std::int64_t shopCost(const std::string& till, const std::string& item, int quantity) const;
    double gateTollNow() const;
    std::int64_t almsNow() const;
    int driftedWage(const std::string& till, int wage, bool hand = false) const;
    // A shop's price for a ware now, as a factor of the catalogue's: the season's times the shop's own from its stock (a
    // quality of a ware at the ware's). What residents pay there, and what players pay and are offered (quote).
    double shopFactor(const std::string& till, const std::string& item) const;
    // Where a till's wage step stands against its bounds: 1 at the top (no post can rise further), -1 at the foot (none
    // can fall further: the floor or the bound), 0 between.
    int wageAtBound(const std::string& till) const;
    // The Country's food farms (Docs/Design/71-the-country-s-industries.md, 4, Phases A and B; RatwFarms.cpp): modelled
    // producers with virtual crews (no NPC holds their posts yet). Each farm is a till (FarmTill: "farm:<id>") and its
    // crew a household (FarmCrew: "crew:<id>") that shops in the city as a household of its size does. An area switched on
    // (its own switch, else the schedule: one area every `every` days from `start`) takes its goods from the Country: the
    // shops' store draws of them come from its farms' stock (spread by stock; paid to the farms' tills, the toll and
    // carriage as before), limited by that stock, not the season's cap; the Country no longer sends them. Its farms are
    // founded the first time it goes on: their floats and their crews' settled purses made, as the world's founding money
    // was (recorded as minted, so the money stays conserved), and a week of their yields grown. Each day an on farm grows
    // its yields (blended by season, times its yield scale), the Country's own people eat at its table, and what is past
    // keeping spoils; at 18:00 it pays its crew their fixed wages (short: the treasury's wage support, as a till's); each
    // shop hour it spends what it holds over its float and tonight's wages on its basket of the city's goods, as the
    // Country does. Off again, its farms stand still (they keep their money and stock) and the Country supplies the goods.
    struct FarmNow
    {
        const items::FarmDef* def = nullptr;
        std::size_t area = 0;                       // Its area in items::farms().areas.
        std::string till, crew;
        int keepers = 0, hands = 0, keeper = 0, hand = 0;
        double yieldScale = 1;                      // Its area's percent times its own.
        std::int64_t reserve = 0, purse = 0;        // Its float (what its till keeps before its basket), its crew's settled purse.
        std::map<std::string, double> grown, eaten;   // The fractions of a piece carried to tomorrow (not saved).
        std::map<std::string, double> making;       // Its makes' own milk (`aDay`) carried, by the good made (not saved).
        double hunger = 0;                          // Its crew's nourishment owed (a day's at most; not saved).
        double takingsCarry = 0;                    // The fraction of a penny its store draws owe it, or it them (not saved).
        // A works (doc 72, Phase 0: its area's `works`): it sends its rent each night to the House whose land it is on
        // (what its till holds over its float after the wages: the estates' rent, into the Country's purse, "the Houses'
        // estates and the sea trade", booked by House: `house`, its own or its area's). A food farm spends what is over
        // on its basket, less its land rent (doc 72, open question 2, Phase 3's follow-up): its rule's percent of its
        // takings since the last night (`rentTakings`, not saved), to its House's estate (`house`; "the Crown" where no
        // House holds the land) after its wages, from what its basket would have spent, never its wages or its float.
        std::string house;
        bool works = false;
        bool rents() const { return works; }
        std::int64_t rentTakings = 0;
        // Its work out of doors (a works, doc 72: its centre's cell, which it may share with others' posts): a post there
        // is its own only by its labels.
        bool outdoors = false;
        // Its real posts (doc 71, Phase C): the positions at its workplaces (its keeper's by its label), and how many of
        // them are held now by the living (countFarmPosts, each day and as people come). Each real post replaces one
        // virtual post; a real post standing empty is a hand short, and the farm yields in proportion.
        int postKeepers = 0, postHands = 0, heldKeepers = 0, heldHands = 0;
        int crewSize() const { return keepers + hands; }                  // Its crew as set (real and virtual).
        int virtualKeepers() const { return std::max(0, keepers - postKeepers); }
        int virtualHands() const { return std::max(0, hands - postHands); }
        int heads() const { return virtualKeepers() + virtualHands(); }   // Its virtual crew (the household crew:<id>).
        // Its wages a day: all its posts', real and virtual (its float is floatDays of them), and its virtual crew's.
        std::int64_t wages() const { return std::int64_t(keepers) * keeper + std::int64_t(hands) * hand; }
        std::int64_t virtualWages() const { return std::int64_t(virtualKeepers()) * keeper + std::int64_t(virtualHands()) * hand; }
        // Its yield's share: the crew at work (the virtual and the real posts held) of its crew as set, at most 1.
        double working() const
        {
            const int all = crewSize();
            return all > 0 ? std::min(1., double(heads() + std::min(heldKeepers, keepers) + std::min(heldHands, hands)) / all) : 1.;
        }
    };
    static std::string farmTill(const std::string& farm) { return "farm:" + farm; }
    static std::string farmCrew(const std::string& farm) { return "crew:" + farm; }
    const std::vector<FarmNow>& farms() const { return farms_; }
    // Whether an area (of items::farms().areas) is on today, and the area on that supplies a good (-1: the Country does).
    bool farmAreaOn(std::size_t area) const { return area < areaOn_.size() && areaOn_[area]; }
    int farmAreaOf(const std::string& item) const;
    // The pieces of a good the on farms hold, all together; their price for it a year (0: the season's, as the store room).
    int farmsHold(const std::string& item) const;
    // Whether a farm sells a good of `area` from its store: a farm of that area, or of another area on whose yields grow it
    // (doc 72, the iron: the Frosthollow hearth's charcoal, the iron area's, beside the woods' own).
    bool farmSells(const FarmNow& farm, std::size_t area, const std::string& item) const;
    // A food farm's basket a day at most (the rule farmSurplus, after wharf7b): its design's basketADay; -1, no bound.
    std::int64_t farmBasketMost(const FarmNow& f) const;
    double farmPriceFactor(const std::string& item) const;
    // How far the Country's basket has moved to its re-weighted one (0 to 1): the share of the food areas' takings on.
    double wharfBasketShift() const;
    // A food farm's land rent (doc 72, open question 2): the percent of its takings it pays its House's estate each night
    // (the rule landRent<Area>, e.g. landRentFruit, where there is one, else landRent; a works none),
    // and what it owes tonight of its takings so far (its basket keeps it back through the day).
    int landRentPercent(const FarmNow& farm) const;
    // A farm's `makes` (country_farms.json, doc 72: the dairies' spare milk into cheese and butter) of `spare` pieces of
    // `from` past keeping this morning: what of it it used.
    int makeOfSpare(FarmNow& farm, const std::string& from, int spare);
    // Its makes' own yields (`aDay`, doc 72, Phase 4's follow-up: the cows milked for the cheese room) this morning, at
    // `working` of its crew: made at once, while each store has room; never in the till as milk. What it made.
    int makeOfOwn(FarmNow& farm, double working);
    std::int64_t landRentDue(const FarmNow& farm) const;
    // The share of the Country's own people (country_farms.json `estatePeople` and `otherWorkers`) still eating at the
    // farms' table: less the crews of the works whose areas are on, who buy their food in the shops (doc 72, Phase 3).
    double tableShare() const;
    // The day the farms were last tended (-1: not yet), and the money the farms' founding has made since the start.
    std::int64_t farmsDay() const { return farmsDay_; }
    std::int64_t farmsMinted() const { return farmsMinted_; }
    // The farms' people (doc 71, Phase C; RatwFarmPeople.cpp): the farm a post is at (its work room one of the farm's
    // workplaces, items::farmPeople()), -1 for none, and whether it is the farm's keeper's (its title the keeper's label);
    // the labels a farm's posts have; and how many have been placed since the start (not saved), by "keeper", "family",
    // "hand <community>", "market <community>" (a market's keepers and hands).
    int farmOfPost(const Position& p, bool* keeper = nullptr) const;
    std::string farmLabel(const FarmNow& farm, int post) const;   // 0: its keeper's; 1: a hand's; 2: its second's.
    // The cell a post's holders are judged near from (CareerWorld::near, the lodgings): its work's, but for a works' post
    // out of doors (doc 72) its crew's home ground: its community's (a home there), or the city's for a works whose crew
    // lives in Ridgemere (a city farm's work room), as staffFarms places them. (A works' cell is the realm's own region,
    // near no home: its posts, once empty, were never filled again.)
    std::string reachOf(const Position& p) const;
    const std::map<std::string, int>& farmPlaced() const { return farmPlaced_; }
    // The world's economy settings replaced while it runs (the DM's economy.sync: doc 68, Progress): every rule the loop
    // and the store room read, at once; the treasury's and stores' starting amounts are kept (they were spent long ago).
    void adoptEconomy(const EconomySpec& economy);
    // The residents' comforts as this world has them (EconomySpec, else city_loop.json).
    int comfortsAbove() const;
    double comfortsShare() const;
    // The loop's rules as this world has them (doc 68, Progress, "First fixes"): EconomySpec::rules, else city_loop.json's,
    // else items::Rules' own; -1 for a name that isn't a rule.
    int rule(const std::string& name) const;
    // The city's services as this world has them (city_loop.json's list, with this world's changes; the price items.json's
    // unless changed), in city_loop.json's order.
    struct ServiceNow
    {
        std::string id, business;
        bool on = true;
        std::int64_t price = 1;
        int every = 30;
        bool patron = false;                        // A patron's, for each of its patrons' units (the armoury's mending).
    };
    const std::vector<ServiceNow>& services() const { return services_; }
    // Trades (doc 68, Progress, "Trades: medicine, the armoury's patrons, office heads"). The city sellers of a good a
    // business uses (city_loop.json `suppliers`: the cooper for the wine makers' casks), and how many armed wolves a
    // patron's group has (its posts, or its rule).
    std::vector<std::string> citySellers(const std::string& buyerBusiness, const std::string& item) const;
    int unitsOf(const items::PatronGroup& group) const;
    // Buys up to `want` of an input for `till` from the city's sellers of it (till to till, at their price: the
    // catalogue's, the season's difference after), the seller holding most first; how many came. Nothing when the rule
    // "suppliers" is off. `resale` (doc 72, Phase 5: the resale goods, `!w.craft`): a good the till sells on, bought at
    // the store room's price for it (resaleMarkup of the season's; at the seller's price it would leave the reseller
    // nothing), or the supplier's own `price`. The sellers are the city's makers and the country workshops alike.
    int buyFromCity(const std::string& till, const items::Business* business, const std::string& item, int want, bool resale = false);
    // The percent of the catalogue's (the season's) price a seller of a business is paid for a good (city_loop.json
    // `suppliers`, its `price`; else 100 for a craft input, resaleMarkup for a good sold on).
    int supplierPrice(const std::string& buyerBusiness, const std::string& sellerBusiness, const std::string& item, bool resale) const;
    // A luxury (city_loop.json `luxuries`, doc 72, Phase 5: cheese): never bought as food, only from the luxuries' budget.
    static bool luxury(const std::string& item);
    // The crew's hours a till may hold in hand (workBankHours: that many hours of the crew; 0: the business's longest
    // craft and an hour more, at least two hours' worth).
    double workBankMost(const std::string& till, const items::Business* business) const;
    // What the loop did, for reports (world_check --economy): since the day began, and the day before's.
    struct LoopDay
    {
        std::int64_t day = -1;
        std::int64_t residentsSpent = 0, basketSpent = 0, basketUnspent = 0, drawn = 0, toll = 0, carriage = 0;
        std::int64_t wagesOwed = 0, wagesPaid = 0, profit = 0, subsidy = 0, alms = 0;
        // Of the alms, what went to households with no earner over the day's a head any household is made up to (the rule
        // almsNoEarner, doc 73, round 4: of what the treasury held over its cap), and how many households were given it.
        std::int64_t almsNoEarner = 0;
        int almsNoEarnerHomes = 0;
        // And to households whose earners' wages together are under the day's alms a head (doc 73, round 6: a lone hand
        // on a floor wage, a family of four on two hands' wages), made up the same way, of the same money.
        std::int64_t almsLowWage = 0;
        int almsLowWageHomes = 0;
        // The treasury's wage support (paid to the posts of tills short of their wages; counted in wagesPaid too), and
        // the city's services: done, what households paid, and the treasury's share of it (doc 68, "First fixes").
        std::int64_t wageSupport = 0, servicesSpent = 0, servicesCity = 0;
        int servicesSold = 0;
        // Trades (doc 68, "Trades"): what keepers bought from the city's makers for their crafts (pieces, pennies); the
        // patrons' work done and new kit, what the treasury and the Country paid, and by business (in serviceTrades'
        // kind).
        int cityTradePieces = 0, patronJobs = 0, patronPieces = 0;
        std::int64_t cityTrade = 0, patronsTreasury = 0, patronsEstates = 0;
        // Of the city trade, the goods bought to sell on (doc 72, Phase 5: the resale path), pieces and pennies, by good.
        int cityResalePieces = 0;
        std::int64_t cityResale = 0;
        std::map<std::string, int> cityResaleGoods;
        // The luxuries (doc 72, Phase 5: cheese): pieces households bought, what they paid, and the households that did.
        int luxuriesBought = 0, luxuryHouseholds = 0;
        std::int64_t luxuriesSpent = 0;
        // By the households' class (doc 72, after country5: "upper", "middle", "lower", "crew" a farm's), and the Houses'
        // own tables (the Country's purse: housesLuxuries), pieces and pennies.
        struct Luxuries
        {
            int pieces = 0;
            std::int64_t spent = 0;
        };
        std::map<std::string, Luxuries> luxuryClasses;
        int luxuriesHouses = 0;
        std::int64_t luxuriesHousesSpent = 0;
        std::set<std::string> housesLuxuriesDone;   // (The purses whose tables have bought today: the Country's, the estates'.)
        // The country workshops (doc 72, Phase 5), by building: tonight's wages owed and paid, and what went to its House.
        struct Workshop
        {
            std::int64_t owed = 0, paid = 0, toHouse = 0;
        };
        std::map<std::string, Workshop> workshops;
        int boughtAhead = 0;                        // 1 on a day households bought ahead for a Restday or festival.
        int householdsShort = 0, foodBought = 0, needsBought = 0, comfortsBought = 0, tillsShort = 0;
        // The treasury's surplus (doc 68, "The treasury's surplus: works and alms"): what it held over its cap and
        // surplusKeep tonight, what of it went to the public works' orders (pieces, pennies; by business in worksTrades)
        // and to the poorest households, and the households given it.
        std::int64_t surplus = 0, worksSpent = 0, surplusAlms = 0;
        int worksPieces = 0, surplusHouseholds = 0;
        // By business (the services' trades): services done and what they took in, the city's share included.
        struct Trade
        {
            int sold = 0;
            std::int64_t spent = 0;
        };
        std::map<std::string, Trade> serviceTrades, patronTrades, worksTrades;
        // By season group (doc 68, Progress, "Seasons"): pieces drawn from the Country, pieces sold to residents and
        // what they paid for them.
        struct Group
        {
            int drawn = 0, sold = 0;
            std::int64_t spent = 0;
        };
        std::map<std::string, Group> groups;
        // The Country's food farms (doc 71, 4), by farm: pieces grown, sold to the shops (store draws), eaten at the
        // Country's table and spoiled (and made into another good at the farm: its `makes`, the dairies' spare milk into
        // butter and cheese, doc 72; what is made counts as grown); the pennies its till took in (the draws' cost less the toll), owed its crew and paid
        // them (the treasury's wage support apart), and spent on its basket (its carriage out apart); and its crew's
        // household: its spending in the city's shops (food, needs, comforts and services) and the pieces it bought, a day
        // it could not buy a day's food, and its nourishment not eaten for want of food.
        struct Farm
        {
            int grown = 0, sold = 0, eaten = 0, spoiled = 0, used = 0;
            std::int64_t takings = 0, wagesOwed = 0, wagesPaid = 0, support = 0, basket = 0, basketCarriage = 0;
            std::int64_t crewSpent = 0;
            std::int64_t peoplePaid = 0;            // Of wagesPaid, what its real posts' holders were paid (doc 71, Phase C).
            std::int64_t rent = 0;                  // A works' rent tonight, or a food farm's land rent, to its House's estate (doc 72).
            std::int64_t surplus = 0;               // Of `rent`, a food farm's surplus over its design's basket (farmSurplus).
            int crewFood = 0, crewNeeds = 0, crewComforts = 0, crewServices = 0, crewShort = 0, tableShort = 0;
            double crewHungry = 0;
            // By good (world_check's full recording, doc 69, 10): the same pieces, and the table's short of it.
            struct Good
            {
                int grown = 0, sold = 0, eaten = 0, spoiled = 0, tableShort = 0, used = 0;
            };
            std::map<std::string, Good> goods;
        };
        std::map<std::string, Farm> farms;
        std::map<std::string, int> areasOut;         // By area: draws the farms' stock could not fill in full today.
        std::map<std::string, int> goodsOut;         // The same by good (a good no farm grows among them).
        std::map<std::string, int> goodsSold, goodsSpoiled;   // By good: the pieces the farms sold to the shops, and spoiled.
        // The treasury's day (doc 71, 4, Phase B's gate): the least it held at an hour's end, what it held after the night's
        // wages, support, alms and surplus; the wage support the short tills wanted and what of it went unpaid, the
        // switch's (the farms' tills and switchTrades') apart from the city's ordinary short tills'; the money made today
        // (a farm's founding: its float and its crew's purse).
        std::int64_t treasuryLow = -1, treasuryNight = -1;
        std::int64_t supportWanted = 0, supportUnpaidSwitch = 0, supportUnpaidOrdinary = 0, minted = 0;
        std::map<std::string, std::int64_t> supportByTrade;   // The wage support paid tonight by trade (a farm's: "farm").
        // The Country's day by its parts (doc 72, Phase 0: "the Houses' estates and the sea trade", one purse): the works'
        // rents by House; its sales to the shops' store draws, its takings after the toll (the sea trade and the goods it
        // still sends); its posts' wages owed and paid tonight, all and by city_loop.json wage rule (its name).
        std::map<std::string, std::int64_t> rents;
        std::map<std::string, std::int64_t> landRents;         // The food farms' land rents by House ("the Crown" too).
        std::int64_t wharfSales = 0, estatePostsOwed = 0, estatePostsPaid = 0;
        std::map<std::string, std::int64_t> estatePostsByRule;
        std::map<std::string, int> estatePostsHeld;           // The posts held tonight, by the same rule.
        // The Houses' estates (doc 72, Phase 6), by estate id: what came in (its House's rents, works' and farms'; the
        // Country's own sales are the wharf's), the Crown's grants in and out, tonight's posts owed and paid and how many,
        // what it bought (its basket, its tables, its patrons), and the money moved when the purse was split or folded.
        struct Estate
        {
            std::int64_t rents = 0, grantsIn = 0, grantsOut = 0, postsOwed = 0, postsPaid = 0, spent = 0, moved = 0;
            std::int64_t levyIn = 0, levyOut = 0;   // (The Crown's levy on the others' spare: in, the Crown's; out, each's.)
            // Of `rents`, by source (after wharf7b, to trace an estate's income): the food farms' land rents, the works'
            // rents, the country workshops' and the Houses' city businesses' takings; and the Wharf's fees (the wharf's).
            std::int64_t land = 0, works = 0, shops = 0, fees = 0;
            std::int64_t surplus = 0;               // (Of `land`, the food farms' surplus over their design's basket.)
            std::int64_t keep = -1;                 // (Its keep tonight, before its buying tomorrow: estateKeep; -1 not known.)
            int posts = 0;
        };
        std::map<std::string, Estate> estates;
        // The Wharf's day (doc 73, 7): by good, the pieces it landed and what the realm paid for them (its lot's share of
        // the draws, the toll apart), and the pieces it took from the farms and what the beyond paid them (the toll apart);
        // what its export basket spent; the beyond's pennies in and out; its fees; the credit's moves; and (-1 until the first
        // shop hour) what the day's takes want of the beyond, kept back for them from the export basket before their hour.
        struct WharfGood
        {
            int pieces = 0;
            std::int64_t pennies = 0;
        };
        std::map<std::string, WharfGood> wharfImported, wharfTaken;
        std::map<std::string, WharfGood> wharfTakenShops;      // (Of wharfTaken, what came from the city's shops.)
        std::int64_t wharfIn = 0, wharfOut = 0, wharfFee = 0, wharfMoved = 0, wharfBasket = 0, wharfTakeWant = -1;
        std::int64_t wharfTakeUnfit = 0;            // What the takes wanted that the export room could not fit (pennies).
        std::map<std::string, std::int64_t> wharfTakeUnfitBy; // (Round 9, doc 73: wharfTakeUnfit by good.)
        // Round 2 of stabilizing (doc 73): the pieces of each good rotted tonight (rotStock); what each trade's short tills
        // could not pay their posts tonight, before the treasury's wage support (supportWanted by trade: the till's own
        // shortfall, not what the treasury had room for).
        std::map<std::string, int> rotted;
        std::map<std::string, std::int64_t> shortByTrade;
        std::map<std::string, std::int64_t> wharfLandedBeyond; // Of the landings' takings, what was paid beyond, by good.
        // Round 3 (doc 73): each maker's crew-hours today (restockFromStore's work bank): the hours its crew put in, what
        // its crafts used of them, and what the bank could not hold (its cap, workBankMost: hours no craft wanted). A
        // till that uses all its hours is bound by its crew; one that loses most of them, by its sales.
        struct Work
        {
            double hours = 0, used = 0, lost = 0;
        };
        std::map<std::string, Work> work;
    };
    const LoopDay& loopToday() const { return loopToday_; }
    // Wants the loop could not meet (world_check's full recording, Docs/Design/69-simulation-speed.md, 10): `buyer` (a
    // resident, a crew or a till) went without `pieces` of `item` at `shop` (a till; "" where no open shop had it), and
    // took `instead` in its place ("" for nothing). A household's food short at the shops' last hour is item "food" (its
    // pieces the meals' worth it lacks); a need no open shop holds, each of its goods; a store draw the Country's cap or
    // the farms' stock cut short, the ware. Only read: nothing in the loop depends on it.
    struct Shortage
    {
        std::string buyer, shop, item, instead;
        int pieces = 0;
    };
    void setShortageWatcher(std::function<void(const Shortage&)> watcher) { shortageWatcher_ = std::move(watcher); }
    const LoopDay& loopYesterday() const { return loopYesterday_; }
    // A ware's starting stock on a new shop's shelves (resetAuthored): GoodsKept, or SmithSwords of swords.
    static int startingStock(const std::string& ware);
    static constexpr int SmithSwords = 3;
    static constexpr int CheapPrice = 6;              // A shop's handful of goods cost at most this (pennies), for now.
    static constexpr int GoodsKept = 4;               // How many of each good a new shop starts with, and buys up to.
    static constexpr int SuppliesKept = 40;           // ...of a good it supplies or buys (crafts.json), a store's worth.
    // A new player's own meals to start with (doc 36), and its welcome grant from the treasury, in pennies.
    static constexpr int StartingMeals = 3;
    static constexpr std::int64_t PlayerWelcome = 20;
    static const char* itemName(const std::string& id);
    // The capital is the community whose treasury is "treasury" (the world sets it; empty in a world of one town).
    void setCapital(std::string community) { capital_ = std::move(community); }
    const std::string& capital() const { return capital_; }
    // A community's treasury: its own store's account ("stores:<it>"), or else the capital's ("treasury").
    std::string treasuryOf(const std::string& community) const;
    // A resident's town, where it works or else where it lives (by the world's communities; its store's without them).
    std::string townOfResident(const std::string& id) const { return communityOfResident(id); }
    // Great houses (doc 42, Phase 5b, as people, not owners): each a head's post ("ruling House Fell", "keeping Grayrock
    // Hall", "holding court"), its town, built from the posts on first asking. (What they owned and spent went with doc
    // 66's Phase 5: a House's family and its servants still speak of it.)
    struct House
    {
        std::string id, name, head, community;      // id: "house:fell"; head: the head's position.
    };
    const std::vector<House>& houses() const;
    static bool houseHead(const std::string& title);
    // A barracks, a bunkhouse, quarters or lodgings: many under one roof who aren't a family.
    static bool communalHome(const std::string& home);
    // Restday's service: from ServiceStart to ServiceEnd a third of the town (a steady per-wolf share, different week to
    // week) sits in its church while its clergy preach.
    static constexpr double ServiceStart = 9, ServiceEnd = 11;
    static bool goesToChurch(const std::string& resident, std::int64_t day);
    static bool clergy(const std::string& title);     // A priest's, a chapel keeper's, an acolyte's work.
    std::int64_t moneySupply() const;
    bool conserved() const;
    // Every ledger entry recorded since the last call, oldest first (the saved ledger keeps only the latest 128).
    // For the world's event log; never saved. Keeps at most JournalKept if nobody collects it.
    static constexpr std::size_t JournalKept = 50000;
    std::vector<EconomyEntry> takeJournal();
    const std::vector<EconomyEntry>& pendingJournal() const { return journal_; }

    // Lives (doc 64, Phase 8; RatwCareers.cpp, RatwWorld.cpp's tendOldAge): deaths by age (a yearly chance of DeathA *
    // e^(DeathB * years), InfantDeath under five); each wolf retires at its own age, RetireLeast..RetireMost (a lot by
    // its id), leaving its post. A post's trade has a level (tradeLevel: its business's hardest craft's skill). Learning
    // at work, a year of it: LearnBeside beside a better hand (up to five short of it, or the level where it's there),
    // LearnHead for a shop's head short of its level, LearnAlone otherwise; never past SkillTop. Apprentices learn beside
    // a qualified master until they reach the trade's level (TradeSkilled for a plain trade); a masters' trade
    // (TradeMasters) takes the young from plain work too. Succession: the apprentice; for a masters' trade a master idle
    // or working below its level; family or a local as skilled as the trade needs; else (the user's rule, 2026-10-14)
    // the best-trained wolf about who isn't in a master's place or an important post. A household of more than
    // HouseholdMost sends a young grown member to a free home in its town.
    static constexpr double DeathA = .0004, DeathB = .085, InfantDeath = .03;
    static constexpr int RetireLeast = 58, RetireMost = 66, HouseholdMost = 6;
    static constexpr double LearnBeside = 8, LearnHead = 4, LearnAlone = .3, SkillTop = 85;
    static constexpr double TradeSkilled = 30, TradeMasters = 50;
    // A day at a post, in spells of an hour (a year of learning is DaysPerYear of them).
    static constexpr int PaidSpells = 8;
    static int retireAge(const std::string& resident);
    double tradeLevel(const Position& p) const;
    bool important(const std::string& resident) const;     // In a master's place, keeping its own shop, or the clergy.
    // Children's friend groups (the user, 2026-10-05): in each town its children, by age, in fours; they play together.
    // A child's group ("" for none); made again each day.
    const std::string& friendGroup(const std::string& child) const;
    // A child's first pennies at its birth, from the better-off parent (given, never made); and what an arrival from the
    // Country brings: its own purse up to this, from the Country's account (moved, never made).
    static constexpr std::int64_t ChildPurse = 5, ArrivalPurse = 30;
    // A post nobody has taken in PostWaitDays is filled from the city (a wolf with no post that pays, grown pups first, of
    // its class), else by an unplaced NPC of its class from the Country. A death's child is due 1 to BirthDelayMost days
    // after it.
    static constexpr int PostWaitDays = 8, BirthDelayMost = 7;

  private:
    void splitHouseholds(std::int64_t day, const CareerWorld& world, std::vector<CareerNote>& notes);
    std::string communityOfResident(const std::string& id) const;
    LifeDay day_;
    Parallel parallel_;
    // Each resident by number (its place in ID order), and what its decisions every second reach through it, kept while
    // nothing they rest on changes (refreshRecords): the roster, the careers, the specs, the positions, the accounts.
    struct ResidentRecord
    {
        std::pair<const std::string, ResidentLife>* entry = nullptr;
        const ResidentSpec* spec = nullptr;
        EconomyAccount* wallet = nullptr;           // (Null: none.)
        const Position* job = nullptr;              // jobOf, apprenticedTo.
        const Position* learning = nullptr;
        std::string home, community;                // Its home as last seen, and that home's community.
        double* skill = nullptr;                    // Its skill at `job`, once it has one.
        const Spot* bed = nullptr;                  // Its place on a bed (doc 36), or null.
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
    std::uint64_t dayRevision_ = 1;
    // A resident's skill at a position, where it is kept (made, at 0, with `make`; else null for none yet).
    double* skillSlot(const std::string& resident, const std::string& position, bool make);
    std::int64_t secondsDecided_ = 0;               // Seconds decided since this society began (for UnseenStep; not saved).
    bool unseenDecided_ = false;
    static inline int unseenStep_ = UnseenStep;     // (setUnseenStep.)
    std::set<std::string> atStall_;                 // Merchants trading from a market stall right now.
    std::map<std::string, std::string> friendGroups_;   // Child -> its group (made each day: groupChildren).
    std::int64_t groupsDay_ = -1;
    void groupChildren(const std::map<std::string, LifeBody>& bodies);
    SocietyState state_;
    Roster roster_ = Roster::Demo;
    AuthoredRoster authored_;
    std::vector<EconomyEntry> journal_;
    // spec() by resident ID, built on first use: every decision looks up every resident's spec. Anything that
    // adds, removes or replaces authored_.residents must call forgetSpecs().
    mutable std::unordered_map<std::string, std::size_t> specIndex_;
    mutable std::unordered_map<std::string, std::pair<std::string, std::vector<std::string>>> waresCache_;   // id -> (label, wares)
    // The store room's goods by keeper (storeGoods), kept while its business, founder and the world's modes stay.
    struct StoreList
    {
        std::string signature;
        std::vector<std::string> making, resale, goods;
        bool anything = false;                      // Goods of either kind, the world's switches aside.
    };
    bool failStoreCreate_ = false;                  // (failNextStoreCreate.)
    mutable std::unordered_map<std::string, StoreList> storeCache_;
    const StoreList& storeList(const std::string& keeper) const;
    std::unordered_map<std::string, std::int64_t> restockedAt_;   // Keeper -> the game hour it last restocked (not saved).
    // The city's loop (doc 68; none of it saved: after a restart the shelves start again from the starting stock).
    std::int64_t loopHour_ = -1;                    // The last game hour the loop ran (-1: not yet; the first is not run).
    void loopHourly(std::int64_t hour, int season);
    void householdsShop(int hourOfDay, std::int64_t day, int season);
    void giveAlms();
    void estatesBuy(int hourOfDay);
    void patronsBuy(int hourOfDay, std::int64_t day);   // Each trade's patrons, once a day while it is open.
    std::map<std::string, std::int64_t> patronsDay_;    // Business -> the day its patrons last bought (not saved).
    std::unordered_map<std::string, std::vector<std::int64_t>> drawnDays_;   // Till -> its draws a day, the last 28 days.
    std::int64_t tillFloat(const std::string& till) const;   // A till's working float (floatDrawDays' mean draws).
    void payWages();
    void supportWages();                            // The treasury over its cap pays the short tills' posts (18:00).
    // Then what it holds over its cap and surplusKeep goes to the public works and the poorest households (18:00, last).
    void spendSurplus();
    std::int64_t topUpPoorest(std::int64_t budget);     // The poorest households topped up, per head, from `budget`.
    std::int64_t worksOrders(std::int64_t budget);      // The public works' orders from the city's trades, from `budget`.
    void turnShelves();
    // The self-correcting rules (doc 68, Progress, "Self-correcting rules"): at the day's turn the shops' wares' sales a
    // day and price factors follow the day's sales and stock (the day ended a Restday or festival: sales never lowered,
    // prices kept); at 18:00 a till's day is counted toward its wage step, and the treasury's level moves the toll and
    // the alms (after the surplus).
    void followStock(bool restDay);
    const std::map<std::string, double>* priceFactors(const std::string& till) const;   // (Null: none but 1.)
    void judgeWages(const std::string& till, std::int64_t owed, std::int64_t paid, std::int64_t profit);
    void followTreasury(std::int64_t day);
    // A maker's perishable by-products (the rule shopRot, doc 73, round 2): each night, a till's stock of a good its crafts
    // make and none of them uses, not food, that keeps only so many days (items.json `keeps`: the butchers' hides), over
    // its keep (the Wharf's for a shop: wharfKeepLeast, wharfKeepDays of its sales, its shelf) loses a `keeps`th of what
    // is over: what no buyer took rots, so it never piles without end.
    void rotStock();
    std::unordered_map<std::string, std::set<std::string>> soldOut_;   // Till -> wares its shelves ran out of today.
    std::map<std::string, std::int64_t> tillsShort_;   // Till -> what it could not pay its posts tonight (payWages).
    std::vector<ServiceNow> services_;              // (buildSeasons, with the rules.)
    std::map<std::string, int> rules_;              // Every rule by name as this world has it (buildSeasons).
    std::unordered_map<std::string, double> serviceOwed_;   // Till -> the city's fraction of a penny of its services.
    std::unordered_map<std::string, std::unordered_map<std::string, int>> soldToday_, shelf_, soldBefore_;   // Till -> ware -> pieces.
    std::unordered_map<std::string, std::int64_t> drawnToday_, drawnYesterday_;              // Till -> pennies drawn.
    std::unordered_map<std::string, double> workBank_;                                         // Till -> crew hours in hand.
    std::unordered_map<std::string, double> carriageOwed_, tollOwed_;                          // Payer -> pennies owed.
    std::int64_t subsidyLeft_ = -1, treasuryOwed_ = -1;  // Today's subsidy still to give, and tonight's public wages.
    std::int64_t gateToll(const std::string& till, std::int64_t cost, bool commit);
    mutable std::unordered_map<std::string, int> crews_;                                       // Till -> crew (with the records).
    mutable std::uint64_t crewsRevision_ = 0;
    LoopDay loopToday_, loopYesterday_;
    std::function<void(const Shortage&)> shortageWatcher_;   // (setShortageWatcher; empty: nobody records.)
    // The seasons (seasonTable): the groups as this world has them, a good's group by its index, where in the year it is
    // (yearDay_) and on which day (seasonDay_), and the pieces drawn of each group that day (not saved: after a restart
    // the day's supply starts again).
    std::vector<items::SeasonGroup> seasons_;
    std::unordered_map<std::string, std::size_t> seasonOf_;
    double yearDay_ = -1;
    std::int64_t seasonDay_ = -1;
    std::unordered_map<std::string, int> seasonDrawn_;
    std::unordered_map<std::string, double> priceCarry_;   // A payer's fraction of a penny of the season's prices.
    void buildSeasons();
    // The farms (doc 71; RatwFarms.cpp): built from items::farms() and the world's own settings (buildFarms, with the
    // seasons), which areas are on today (refreshAreas: founding the farms of one that goes on), the day's growing, table
    // and spoiling (tendFarms, the first loop hour of each day), the wages (payFarms, 18:00, after payWages), the baskets
    // (farmsBuy, each shop hour), and what is spent on a basket of the city's goods from its makers (basketBuys: the
    // Country's and the farms').
    std::vector<FarmNow> farms_;
    std::vector<char> areaOn_;
    std::unordered_map<std::string, std::size_t> farmOf_;   // Farm till -> its farm.
    std::unordered_map<std::string, std::size_t> areaOfGood_;   // A good -> the area whose farms grow it.
    std::int64_t farmsDay_ = -1, farmsMinted_ = 0;
    void buildFarms();
    void refreshAreas(std::int64_t day, bool found = true);
    void foundFarm(FarmNow& farm);
    // The farms' people (RatwFarmPeople.cpp): each farm's real posts counted (FarmNow::postKeepers...), from the
    // positions; and the posts of the farms of an area on filled from the unplaced NPCs (once a day, tendCareers), with
    // their community's market when its first farm is: "farm" requests the host carries out (arrive).
    std::unordered_map<std::string, std::size_t> farmWorkplace_;   // A farm's work room -> its farm.
    std::map<std::string, int> farmPlaced_;
    void countFarmPosts();
    void staffFarms(const CareerWorld& world, std::vector<CareerNote>& notes);
    void tendFarms(std::int64_t day);
    void payFarms();
    void payRents();                                // (A works' rent, a food farm's land rent, after the wages: doc 72.)
    void farmsBuy(int hourOfDay);
    bool farmSwitchTill(const std::string& till) const;
    // `wharfFallback` (doc 73, 5: a farm's basket): a good on the Wharf's imports that no open maker holds is ordered from
    // the Wharf (orderFromWharf) for its share. `subsidised` false (the Wharf's export basket, doc 73, 4.5: the beyond pays
    // its carriage out): no treasury share of the carriage.
    std::int64_t basketBuys(const std::string& fund, const std::vector<std::pair<const std::string*, double>>& basket, double budget,
                            std::int64_t keep, int hourOfDay, const std::vector<std::pair<std::string, const items::Business*>>& open,
                            const char* saleKind, const char* usedKind, std::int64_t* carriageOut, bool workshopsFirst = false,
                            bool wharfFallback = false, bool subsidised = true);
    // The Wharf (RatwWharf.cpp, Docs/Design/73-the-wharf.md). The basket of the city's exports as the farms have moved it
    // (the first market's, blended toward country_farms.json's wharfBasket: the estates' and the Wharf's own).
    std::vector<std::pair<const std::string*, double>> wharfBasketNow() const;
    // What the Wharf's lot of a draw or an order paid it (`coins`, the toll apart) for `n` landed pieces: the beyond's
    // share sunk (with the valve on; its fee kept), the credit raised, the day's books.
    void bookWharfLanding(const std::string& item, int n, std::int64_t coins);
    // Money from beyond the realm into the Wharf's purse (minted), and from it back beyond (sunk); recorded with "beyond".
    void wharfMint(std::int64_t coins, const std::string& why);
    void wharfSink(std::int64_t coins, const std::string& why);
    // The takes at WharfTakeHour (doc 73, 4.4): with `commit` false, only what they would want of the beyond today
    // (pennies: prices, carriage and fees), within the lists' caps but not the export room.
    std::int64_t wharfTakesRun(bool commit);
    void bookWharfFee(std::int64_t coins);      // (Its fee as its estate's income, for the reports.)
    void wharfExports(int hourOfDay);               // The export basket (4.5), each shop hour after the estates'.
    void settleWharf();                             // The standing levers, at 18:00 before the estates (4.6).
    static constexpr int WharfTakeHour = 16;
    // The Houses' luxuries (doc 72, after country5): what the Country's purse spends today on luxuries for the Houses'
    // tables, out of `spare` (luxuryHouses percent of it), from the open shops holding them at their own price.
    std::int64_t housesLuxuries(const std::string& fund, std::int64_t spare,
                                const std::vector<std::pair<std::string, const items::Business*>>& open);
    // A household's class (classOf, doc 67): "upper" if any member is the Houses', else the most members', "middle"
    // while unknown; kept for the day (homeClass_).
    const std::string& homeClass(const std::string& home, const std::vector<std::string>& members);
    std::unordered_map<std::string, std::string> homeClass_;
    std::int64_t homeClassDay_ = -1;
    // A country workshop's till (doc 72, Phase 5: its post's work at a workshops.json building).
    bool workshopTill(const std::string& till) const;
    std::vector<std::pair<std::string, const items::Business*>> openShops() const;
    std::set<std::string> dead_;                    // Residents who have died (died, revived): nobody feeds or pays them.
    std::vector<Position> positions_;               // From authored_; rebuilt with it (buildPositions).
    std::unordered_map<std::string, std::size_t> positionIndex_;
    mutable std::unordered_map<std::string, std::string> heldBy_, learning_;   // resident -> position (cache).
    mutable bool careersIndexed_ = false;
    std::vector<ResidentRequest> requests_;
    std::map<std::string, int> fills_;
    std::string classOf(const std::string& id, int depth) const;
    void tendBirths(double now, const CareerWorld& world);
    // A wolf of the city for an empty post (PostWaitDays), of its class (classStep 0) or the next (1), or "".
    std::string cityWolfFor(const Position& p, const CareerWorld& world, int classStep) const;
    // An arrival from the Country for an empty post, of its class (classStep 0) or the next (1), with a home found for
    // it; false if none fits or there is nowhere for it to stay.
    bool arrivalFor(const Position& p, const PositionState& ps, const CareerWorld& world, int classStep, ResidentRequest& out) const;
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
    std::map<std::string, std::map<std::string, Spot>> homeStores_;
    std::map<std::string, Spot> beds_;
    static constexpr std::int64_t MoneyCap = 1000000000;
    static constexpr int StockCap = 10000;
    mutable bool specsIndexed_ = false;
    void forgetSpecs() { specsIndexed_ = false; }
    std::unordered_map<std::string, std::pair<double, double>> giftLift_;   // Maker -> a lent Gift's lift, and until when (doc 43).
    std::unordered_map<std::string, std::pair<double, double>> pawLent_;    // Maker -> a lent paw's rate, and until when (doc 53).
    double decidingAt_ = 0;                         // (The absolute day of the second being decided: decide().)
    // Every shop's till (its post's, "till:<position>"), by its position, for tillOf.
    std::unordered_map<std::string, std::string> tills_;
    void indexTills();                              // tills_ from the accounts (a reset, a restore).
    // Every shop post without a till of its own gets one, once (a new world, or a save from before tills): its keeper's
    // wares move into it, the shop's shelves from then on, and it is given its founder's authored purse's worth (made,
    // as every authored purse is), its money to buy from players with.
    void foundShops();
    std::string capital_;
    mutable std::vector<House> houses_;             // From the heads' positions (houses(), built on first use).
    mutable bool housesKnown_ = false, housesTold_ = false;
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
