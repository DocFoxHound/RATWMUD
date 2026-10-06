#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "RatwAppearance.h"
#include "RatwOrchestrator.h"

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
struct LifeDay
{
    std::map<std::string, DayPlan> plans;           // By community (a region).
    std::shared_ptr<const std::map<std::string, std::vector<WorkGround>>> grounds;   // By community (doc 42, 3b).
    Harvest harvest;
    std::function<std::string(const std::string& cell)> communityOf;
    std::function<int(const std::string& cell)> sky; // -1 indoors, 0 fair, 1 wet (rain), 2 harsh (storm, snow).
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
double workPace(double skill);
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
};
struct House
{
    std::string id, name, head, community;          // id: "house:fell"; head: the head's position.
};

// What the economy remembers from day to day (doc 42): who has gone unpaid since when (Phase 3), and each treasury's,
// church's and house's usual spending a day, for its reserve (the rule against hoarding). Saved with the society.
struct EconomyMemory
{
    std::map<std::string, double> unpaidSince;      // Worker -> the day its employer first couldn't pay.
    std::map<std::string, double> outgoing;         // Collector -> a slow average of its days' spending.
    std::map<std::string, double> condition;        // Community -> its buildings' repair, 0 to 100 (the Town Works).
    int purses = 0;                                 // The grant of starting money the world has had (Society::PursesFounded).
    int tills = 0;                                  // Owner-run businesses have tills of their own (Society::TillsFounded, doc 46).
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
    orchestra::State orchestrator;                  // The economy orchestrator's memory, steers and last brief (doc 46).
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
struct AuthoredRoster
{
    std::vector<ResidentSpec> residents;
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
    bool sellsFood(const std::string& merchant, const std::string& item, const std::vector<std::string>& wares) const;
    // Producers sell their town's food shops what food they bring in beyond ProduceKept, daily, a shop taking up to
    // FoodShelf of a food, at a shop's buying price (doc 15: a little over half), as far as it can pay.
    void producersSell(std::int64_t day);
    // Spoilage (doc 42, the user, 2026-10-06): each account's food in batches by the day it came in; once a day, what has
    // come in since is a batch of today, what has gone went from the oldest, and a batch older than its food keeps
    // (Item::keeps) spoils. Goods bought are fresh to their buyer; on the road, in a caravan, they age. Players' packs
    // don't spoil (not yet). The batches aren't saved: a loaded world's food is all fresh.
    void spoil(std::int64_t day);
    // Prices (Docs/Design/46-economy-orchestrator.md, Phase 3): one price for each good in each town, set by the economy
    // orchestrator (a new good's at once, then once a week at its decision), and the margin: what a shop pays the land
    // (a farm, a gatherer, a player selling) is the town's price times it. Every shop sells at the town's price, a good of
    // a quality at its quality's share of it; shops buy from each other at it. A good the orchestrator hasn't priced (or
    // with the orchestrator in shadow or off) goes at the catalog's price; meals and herbs by the town's stores, as before
    // (World::tendPrices). These replace doc 42's markdowns, flush discounts, town food factors, shelf and supply factors.
    double townPrice(const std::string& town, const std::string& item) const;   // Pennies a piece (quality counted).
    double buyingPrice(const std::string& town, const std::string& item) const { return townPrice(town, item) * margin(); }
    double margin() const { return margin_; }
    double townFactor(const std::string& town, const std::string& item) const;   // townPrice against the catalog's.
    std::string shopTown(const std::string& shop) const;   // The town a shop works in.
    // Meals, herbs and swords: priced by the town's stores (World::tendPrices), not the orchestrator, for now.
    static bool storePriced(const std::string& item) { return item == "meal" || item == "herbs" || item == "sword"; }
    void applyPrices();                             // prices_, wages_ and margin_ from the orchestrator's memory, when on.
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
    static constexpr double CartedIn = 1.5;
    // The current grant (SocietyState::craftingStocked): 1 the starter crafts, 2 the workshops (mills, tanneries...), 3
    // masking oil (apothecaries' and perfumers' wormwood and resin, herbalists' wormwood).
    static constexpr int CraftingStock = 3;
    // Food to start with (doc 36): a new player's own meals; a new household's larder, meals for each who lives there,
    // and its chest, herbs for each. Placeholder amounts for the balance pass.
    static constexpr int StartingMeals = 3, LarderMealsEach = 3, ChestHerbsEach = 2;
    // How many days' food a household's larder holds (the user, 2026-10-06: 2 to 5), its own steady habit, so the
    // world's households don't all shop on the same day; and a new larder holds 2 to 5 days' to begin with (in place of
    // LarderMealsEach for everyone), so their first trips are spread out too.
    static int stockingDays(const std::string& homeCell);
    static int shutAhead(std::int64_t today);       // Days from tomorrow the shops aren't properly open (Restday, a festival).
    static int startingLarderDays(const std::string& homeCell);
    static const char* itemName(const std::string& id);
    // The economy orchestrator (Docs/Design/46-economy-orchestrator.md; RatwOrchestrate.cpp). Once a game day, at
    // SnapshotHour, the society copies what the orchestrator reads (purses, food, work, prices, what each holder spent
    // since yesterday, the Dungeon Master's steers) and hands it to the orchestrator's thread; when the day turns, before
    // the day's pass, it takes the brief (waiting for it if need be), so a run repeats exactly. It measures every day and
    // acts once a week (the user, 2026-10-06): the evening of the weekly reckoning, after the taxes and tithes, the brief
    // also holds the week's decisions. Phase 1 (shadow): the briefs are kept and shown, and nothing in them is applied.
    static constexpr double SnapshotHour = 23;
    // Whether the orchestrator plans on a thread of its own (the world's runner turns it on with its threads); off, it
    // plans on the game thread when the day turns: the same brief either way.
    void setOrchestratorThread(bool threaded);
    const orchestra::State& orchestrator() const { return state_.orchestrator; }
    // The dials (Data/Economy/orchestrator.json, read once; RATW_ORCHESTRATOR=off|shadow overrides the mode), or a test's.
    const orchestra::Dials& orchestratorDials() const;
    void setOrchestratorDials(const orchestra::Dials& dials) { dialsOverride_ = std::make_shared<orchestra::Dials>(dials); }
    // Every brief planned since the last call, in full (econ_watch); kept only while someone takes them.
    std::vector<orchestra::Brief> takeBriefs();
    void keepBriefs(bool keep) { keepBriefs_ = keep; }
    // A Dungeon Master's steer (doc 46, Part 10), in force from tomorrow for `days` days; `id` names it ("steer-<n>" if
    // empty). Steers are saved, and end by themselves.
    EconomyResult steer(orchestra::Steer steer, int days);
    bool unsteer(const std::string& id);
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
    std::map<std::string, int> mealsToday_;         // A shop's till -> the meals it sold today.
    std::map<std::string, std::string> shopTown_;   // A shop's till -> its community (made daily).
    // The orchestrator's prices, applied (applyPrices): town -> good -> pennies; its wage table (town -> kind -> a day's
    // pay); and its margin.
    std::unordered_map<std::string, std::unordered_map<std::string, double>> prices_, wages_;
    double margin_ = .55;
    // What each larder gives its household a day (nourishment taken from it, a running average, Society::spoil): the
    // household lays in by what it eats from home, not by what its members would eat if they never ate out (not saved).
    std::unordered_map<std::string, double> larderUse_;
    std::unordered_map<std::string, int> larderTaken_;      // Today's, so far.
    std::map<std::string, std::map<std::string, std::deque<std::pair<std::int64_t, int>>>> batches_;
    // What each shop sells of each good a day, and the sales it missed for having none (a running average; Society::spoil
    // keeps it daily): a maker short of materials makes first what is wanted (TRIAL make_what_sells, doc 42). Not saved.
    std::unordered_map<std::string, std::unordered_map<std::string, double>> sellRate_;
    std::unordered_map<std::string, std::unordered_map<std::string, int>> soldToday_;   // Account -> food -> (day, n).
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
    std::unordered_map<std::string, double> produceNext_;   // Producer -> the game day of its next yield.
    int season_ = 0;                                 // The season of the latest decision (0 spring .. 3 winter).
    void craft(const std::string& id, const std::string& workCell, double absoluteDay);
    // Buys up to `wanted` of an item for `id`: in its own community at the price, then from anywhere, carted in.
    // A supplier restocking (`forSupplier`) buys from workshops and producers, never from another supplier.
    int buyMaterials(const std::string& id, const std::string& workCell, const std::string& item, int wanted,
                     bool forSupplier = false);
    // A producer's spell of work done: what it brings in from the land, while it has fewer than ProducerKept.
    bool produce(const std::string& id, double absoluteDay);
    std::string communityOfResident(const std::string& id) const;
    // Tops a shopkeeper's stock up to a good store of what they make things from and what they supply; returns how
    // many goods were added.
    int stockMaterials(const std::string& id);
    // What townsfolk buy (RatwDemand.cpp): whether a shop has food for sale; a hungry resident's purchase at it (enough
    // for now, and a couple of days more with a larder), returning how many it bought; and each household's day of
    // errands (firewood, candles, clothes... crafts.json `households`).
    bool shopHasFood(const std::string& merchant) const;
    int buyFood(const std::string& resident, const std::string& seller, bool stocking);
    void householdShopping(std::int64_t day, int season, const std::map<std::string, LifeBody>& bodies);
    // Children's stipends (the user, 2026-10-05; RatwDemand.cpp): once a day each child living with grown family is given
    // a little from the household's purse (its grown members' together), as far as it can spare after a week's food for
    // everyone at home, the tax and tithe it owes on the week so far, and its rent; and children spend freely, up to half
    // what they have each day, on something cheap they like.
    void childrenAndStipends(std::int64_t day, const std::map<std::string, LifeBody>& bodies);
    // Wants (the user, 2026-10-05): a grown wolf with money to spare after a week's food and the tax and tithe it owes
    // spends a tenth of what is above that each day on something it simply wants, by its own taste: a treat or a dish
    // it fancies, a drink, jewellery, a scarf or a hat, soap and scent, a pipe, dice, a broadsheet. Food is kept to
    // eat, drink drunk, finery kept (a few pieces), the rest used.
    void wants(std::int64_t day, const std::map<std::string, LifeBody>& bodies);
    // Households (the user, 2026-10-05; RatwHouseholds.cpp), once a day: the grown members share one purse (a shopkeeper's
    // till aside), and a household that has stayed comfortable a week keeps one of them at home (to mind the children,
    // shop and look about), sending it back to work when money runs low; a poor one sends everyone who can to work.
    void tendHouseholds(std::int64_t day, const std::map<std::string, LifeBody>& bodies);
    std::int64_t householdNeed(std::size_t members) const { return FoodADay * std::int64_t(members); }
  public:
    static constexpr std::int64_t FoodADay = 5;     // A wolf's food a day, as a household reckons it (pennies).
    // A shop's price to the townsfolk: its town's (townPrice, the economy orchestrator's: doc 46, Phase 3).
    std::int64_t shopPrice(const std::string& shop, const std::string& item) const;
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
        // A business's hire runs HireDays (the user, 2026-10-05: semi-permanent work at shops that are hiring): whoever
        // took it comes back each day to it, paid each day, until then. Other odd jobs are the day's.
        std::int64_t until = -1;
    };
    static constexpr int HireDays = 7;
    // A business's hire is a day's work (HireSpells at its premises), paid by the day like wages, at its town's pay for a
    // hand (the wage table, doc 46, Phase 4). A wolf whose own post pays HireRaise less than a hire takes it.
    static constexpr std::int64_t HireRaise = 2;
    static constexpr int HireSpells = 4;
    // Children's friend groups (the user, 2026-10-05): in each town its children, by age, in fours; they play together
    // and take odd jobs together. A child's group ("" for none).
    const std::string& friendGroup(const std::string& child) const;
    static constexpr std::int64_t OddJobPay = 4;
    const std::vector<OddJob>& oddJobs() const { return oddJobs_; }
    // Who covers a wage an employer can't pay, or posts work for the poor (the user, 2026-10-05: towns and churches
    // subsidise industry so things keep turning): the town's treasury if it is well above lean, else its church if it
    // has plenty; "" for neither.
    std::string subsidiser(const std::string& community) const;
    static constexpr std::int64_t MostStipend = 3;  // A child's stipend a day, before a rich household's more (see below).
    // A household is comfortable with ComfortDays of food in its purse for everyone at home, poor under PoorDays'; it keeps
    // a member at home after a week comfortable, and sends it back under KeeperDays' (and everyone to work after a week poor).
    static constexpr int ComfortDays = 28, PoorDays = 7, KeeperDays = 10, StreakDays = 7;
    static bool homemaking(const std::string& title);   // "keeps the house", "keeping the house".
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
        std::int64_t price = 1;                          // A piece, as the catalog has it.
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
    // - the tax on a week's profit is a tenth, and a fifth of the part above TaxBand;
    // - each week a WealthTitheShare-th of what anyone holds above a comfortable line goes to the church (wealthLine);
    // - each week the capital sends the towns half of what it holds above four weeks' spending, by how many of their
    //   folk are short of a week's food;
    // - each day the church gives households short of a week's food DoleADay a member, in coins (the dole);
    // - a great house keeps only its floor and spends a HouseSurplusShare-th of the rest a day; what it holds above that
    //   floor raises the wages it pays (wageFor), rather than being given away.
    static constexpr std::int64_t TaxBand = 70, DoleADay = 2;
    static constexpr int WealthTitheShare = 20, HouseSurplusShare = 10, KeeperSurplusShare = 5;
    // Above what a wealth tithe is paid: a resident's four weeks' food; a keeper whose purse is its shop's till, that, its
    // food money and a float; a great house, its floor (100p and a float for each business: as for its surplus).
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
    // Wages (doc 46, Phase 4): each town's table of a day's pay by kind of post (orchestra::wageKinds), set by the
    // economy orchestrator (the dials' starts until it has), paid a PaidSpells-th a spell of work. Every payer pays the
    // table: a shop or house that can't is covered by its town (or church) if it has plenty, else its hand goes unpaid.
    double dayWage(const std::string& town, const std::string& kind) const;
    std::string wageKind(const Payer& payer, const Position& job) const;   // "help", "guard", "labour", "clergy".
    // A living for every grown wolf (doc 42, Phase 3): one out of work labours for the Town Works; at RetireAge, retired.
    static constexpr const char* LabourTitle = "labouring for the Town Works";
    static constexpr int PayrollDays = 7;          // Days of wages a town's treasury keeps before funding its buyers (a
                                                   // week's: the market dues come in weekly).
    // Spells of paid work a day (a spell: a game hour at the post): paid for the hours worked, up to a full day's
    // (the user, 2026-10-05: wages must cover a day's living; it was 3, a third of the day). A placeholder.
    static constexpr int PaidSpells = 8;
    // A treasury with less than this a head of its town pays its posts 1p a spell, not 2 (doc 42, 2026-10-05).
    static constexpr int LeanTreasury = 20;
    bool treasuryLean(const std::string& treasury) const;
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
    bool carriesForSale(const std::string& resident) const;
    bool sellsTo(const std::string& resident, const std::string& shop) const;   // The shop takes something it carries.
    // Great houses (doc 42, Phase 5b; RatwHouses.cpp): the houses, each business's till (the keeper itself unless a house
    // owns it), and the house that owns a position's business ("" for none).
    const std::vector<House>& houses() const;
    std::string tillOf(const std::string& keeper) const;
    std::string ownerOf(const std::string& positionId) const;
    // A manager's wage a day, the days of running costs a till keeps (its float), and how many of the last 28 days a
    // till may run low (propped up by its house, or not) before the house sells the business on. Placeholders.
    static constexpr int ManagerWage = 8, FloatDays = 3, ProppedDays = 10;
    // A great house's founding fortune, and a month's rent a business pays a house for its ground. Placeholders.
    static constexpr std::int64_t HouseFortune = 3000, MonthlyRent = 20;
    // The month's rents (at the reckoning): each business in a town with great houses pays one that doesn't own it.
    void collectRents();
    std::int64_t floatOf(const std::string& positionId) const;
    // Once a game day: the managers' wages, the takings above each till's float to its house, a struggling till propped
    // up, and a business that keeps losing sold to another house. Gives the houses their businesses the first time.
    void tendHouses(std::int64_t day);
    // Every business its own till (doc 46, Phase 2; RatwTills.cpp): an owner-run shop, workshop, farm or site keeps
    // "till:<position>" apart from its keeper's purse, as a house's business does (tillOf finds either). Its keeper draws
    // OwnerWage a day from it (what it can spare above half its float) and, at the reckoning, an OwnersShare-th of the
    // week's profit after the till's tax and tithe. Founded when a world first runs (an older save: the first time it runs
    // with tills): its keepers move their shop's goods and money (all but a month's living) into it once (TillsFounded).
    // A great house that takes a business on later (foundHouses, a sale) takes its till with it.
    static constexpr std::int64_t OwnerWage = ManagerWage;
    static constexpr int OwnersShare = 3, TillsFounded = 1;
    bool ownsTill(const Position& p) const;         // A business that keeps a till of its own (not a house's).
    void indexTills();                              // tills_ from the accounts (a reset, a restore).
    std::string ownTill(const std::string& positionId) const;   // Its till ("" for none, or a house's business).
    void foundTills();
    void tendTills(std::int64_t day);               // Daily: the keepers' wages, and (until doc 46, Phase 5) surplus spending.
    void ownersShare(const std::map<std::string, std::int64_t>& profits);   // At the reckoning: till -> its week's profit.
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
    // The poverty line (the user, 2026-10-06: tax and tithe the same for all, unless in poverty): a resident holding less
    // than two weeks' food money pays neither tax nor tithe that week.
    static constexpr std::int64_t PovertyLine = 14 * FoodADay;
    // Starting money (the user, 2026-10-05; RatwFounding.cpp): every resident and every body that keeps a purse begins with
    // a reasonable one, made once when the world is founded (an older save: the first time it runs with this). Placeholders:
    // a child ChildPurse and half a penny a year of its age; a beggar PoorPurse; a grown wolf AdultPurse (some five days'
    // food); one retired ElderPurse; a shopkeeper whose shop is its own KeeperPurse, or its shop's float if more. A town's
    // treasury TreasuryHead a resident who pays it, its church ChurchHead (or four weeks of its basket, if more), a town's
    // buyers their working funds, a great house's business its till's float.
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
    // The world's money (the user, 2026-10-06: "increase the world total monetary amount to 500,000p, and spread it
    // evenly everywhere"): once the purses are founded, what the world holds is made up to WorldMoney, WorldGrownShare
    // percent of it shared alike among the grown residents, the rest among the towns' treasuries by their residents.
    // (The second grant: an older save gets it the first time it runs with this.) Only a land of WorldMoneyResidents or
    // more: a village made for a test keeps its own starting money.
    static constexpr std::int64_t WorldMoney = 500000;
    static constexpr std::size_t WorldMoneyResidents = 500;
    static constexpr int WorldGrownShare = 85;
    static constexpr int PursesFounded = 2;          // (1: the purses; 2: the world made up to WorldMoney.)
    std::int64_t startingPurse(const std::string& resident, int age) const;
    // A keeper whose shop is its own (its purse is the till) never spends its last KeeperReserve on the business
    // (materials, restocking, help's wages): it keeps its food money (the user, 2026-10-05). What an account may spend so.
    static constexpr std::int64_t KeeperReserve = 20;
    std::int64_t spendable(const std::string& account) const;

  private:
    std::vector<OddJob> oddJobs_;
    std::int64_t nextOddJob_ = 0;
    // What each shop's till took in today (wages follow takings: RatwSurplus.cpp). Not saved.
    std::map<std::string, std::int64_t> takings_;
    std::map<std::string, std::string> friendGroups_;   // Child -> its group (rebuilt each day: childrenAndStipends).
    // A business with money to spare (RatwOddJobs.cpp): hands hired for its premises, paid from its till, and its premises
    // improved (materials bought from the town's makers, builders hired); returns what it committed.
    std::int64_t businessSpends(const std::string& payer, const std::string& keeper, const Position& job, std::int64_t budget);
    std::map<std::string, std::int64_t> improving_;     // Position -> what has gone into its next improvement (not saved).
    // Farmhands (doc 42, the user, 2026-10-06): every farm, orchard and vineyard hires hands for a few days when it has the
    // money, from its town or from any city; one from elsewhere lodges in the farm's bunkhouse (BunkBeds), fed from its
    // larder, which the farm stocks. A city: a community of CityResidents or more.
    bool farmHire(const OddJob& j) const;          // A hire posted by a farm (its producer works the land).
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
    std::unordered_map<std::string, double> wageCarry_;   // Worker -> the fraction of a penny its wages owe it (not saved).
    // Each Restday, every improved business keeps its premises up (the user, 2026-10-05): UpkeepALevel a level a week in
    // materials bought from its town's makers, from its till (or its house). Two weeks without, and it loses a level.
    void keepUpPremises();
  public:
    static constexpr std::int64_t UpkeepALevel = 5;
  private:
    // Posts odd jobs worth up to `budget` for `payer` in `community`; returns their worth.
    std::int64_t postOddJobs(const std::string& payer, const std::string& community, std::int64_t budget);
    // The odd job this resident holds, claiming one if it is free to (nullptr for none). With `claims`, nothing is claimed:
    // it is set if one would be (deciding at once, RatwResidents.cpp).
    const OddJob* oddJobFor(const std::string& id, const Position& job, bool jobless, int age, bool poor, double hour,
                            std::int64_t ownDayPay = 0, bool* claims = nullptr);
    // Arrived where its odd job takes it: the work done there, and paid when finished.
    void advanceOddJob(const std::string& id, int seconds = 1);   // (A step's seconds of its work.)
    // Made into `account` as starting money: counted as made, and kept out of the month's profit.
    void startingMoney(const std::string& account, std::int64_t coins);
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
    // (TRIAL town_budget, doc 42: a treasury's extras, its buyers' funds, odd jobs and works, come only from what it takes
    // in above its wages, by running averages; not from its savings.) Not saved.
    void noteIncoming(const std::string& to, const std::string& kind, std::int64_t coins);
    std::map<std::string, std::int64_t> incomeToday_, wagesToday_, budgetLeft_;
    std::map<std::string, double> incomeAvg_, wagesAvg_;
    std::int64_t townBudget(const std::string& treasury, std::int64_t wanted);   // What of `wanted` the budget allows (and takes).
    // Buys what `wanted` from the shops' tills, up to `budget`, never a shop's last few; what was got, by item.
    std::int64_t buyForSurplus(const std::string& buyer, const std::vector<std::string>& shops,
                               const std::function<bool(const std::string& item)>& wanted, std::int64_t budget,
                               const std::string& kind, std::map<std::string, int>* got, bool lastToo = false);
    mutable std::vector<House> houses_;                     // From the heads' positions (houses(), built on first use).
    mutable bool housesKnown_ = false;
    void foundHouses();
    mutable std::map<std::string, std::size_t> outwork_;   // Resident -> its ground's index in its community's list.
    mutable std::map<std::string, std::map<std::size_t, int>> outworkTaken_;   // Community -> ground -> how many.
    mutable std::int64_t outworkDay_ = -1;
    std::set<std::string> offered_;                        // Who has put something on the plate this Restday.            // Worker -> the day its employer first couldn't pay (not saved).
    mutable std::unordered_map<std::string, std::vector<std::string>> employers_;   // Work cell -> shops' and houses' positions.
    mutable std::map<std::string, std::string> richest_;    // Home cell -> its household's richest (for the day).
    mutable std::int64_t richestDay_ = -1;
    const std::string* richestAt(const std::string& homeCell, const std::string& besides) const;
    mutable std::map<std::string, int> people_;              // Treasury -> residents who pay it (for the day).
    mutable std::int64_t peopleDay_ = -1;
    void indexEmployers() const;
    std::vector<Reckoning> reckonings_;                    // Since the world last took them.
    std::vector<Procurement> procurements_;                // Asked for since the world last took them.
    std::map<std::string, double> owed_;                   // "account|item" -> a buyer's use not yet taken from stock.
    // The orchestrator's side of the society (RatwOrchestrate.cpp): its runner, the day the last snapshot was for, and
    // what it counts between snapshots (each holder's ordinary spending, coins between towns, what residents earn).
    // (A copy of the society, a candidate checked before it is used, plans on its own: never on this one's thread.)
    struct RunnerSlot
    {
        std::shared_ptr<orchestra::Runner> runner;
        RunnerSlot() = default;
        RunnerSlot(const RunnerSlot&) {}
        RunnerSlot& operator=(const RunnerSlot&) { return *this; }
        RunnerSlot(RunnerSlot&&) = default;
        RunnerSlot& operator=(RunnerSlot&&) = default;
        orchestra::Runner* operator->() const { return runner.get(); }
        explicit operator bool() const { return bool(runner); }
    };
    RunnerSlot orchestraRunner_;
    std::unordered_map<std::string, std::string> tills_;   // Position -> its business's till ("till:<position>"), for tillOf.
    bool orchestraThreaded_ = false, keepBriefs_ = false;
    std::shared_ptr<const orchestra::Dials> dialsOverride_;
    std::int64_t snapshotDay_ = -1, countingFrom_ = -1;   // (countingFrom_: the day counting began.)
    std::int64_t reckonedDay_ = -1;                  // The day of the last weekly reckoning (taxes and tithes).
    std::unordered_map<std::string, std::string> accountTown_;   // Account -> its town, for the coins between towns.
    std::unordered_map<std::string, std::int64_t> holderSpent_;  // Holder -> its ordinary outgoings since the snapshot.
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
    void noteForOrchestra(const std::string& kind, const std::string& from, const std::string& to, std::int64_t coins);
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
