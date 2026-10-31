#pragma once

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
// What a world's treasury starts with (the capital's account, "treasury").
struct EconomySpec
{
    std::int64_t treasury = 1000;
    int storeHerbs = 100, storeMeals = 50;
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
    static constexpr int UnseenStep = 5;
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
    // A player's trade with a shop (its keeper `merchant`, keeping it): its goods (wares) at the catalogue's price, a
    // tenth off at a market stall; and what it buys from a player at SellBackShare of that.
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
    // Needs paused (Docs/Design/66-worlds-atlas-and-dm.md, Phase 5; the user, 2026-10-30): a resident's hunger, which its
    // buying met, neither grows nor sends it anywhere while this is true; a hungry one (a save's) still eats what it
    // carries. The user's economy rules replace it (where hunger would grow: the first half of decideAuthored, in
    // RatwResidents.cpp).
    static constexpr bool NeedsPaused = true;
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
    static constexpr int SmithSwords = 3;
    static constexpr int CheapPrice = 6;              // A shop's handful of goods cost at most this (pennies), for now.
    static constexpr int GoodsKept = 4;               // How many of each good a new shop starts with, and buys up to.
    static constexpr int SuppliesKept = 40;           // ...of a good it supplies or buys (crafts.json), a store's worth.
    // A new player's own meals to start with (doc 36), and its welcome grant from the treasury, in pennies.
    static constexpr int StartingMeals = 3;
    static constexpr std::int64_t PlayerWelcome = 20;
    // The dearest piece the ledger moves (a masterwork siege crossbow is 4,500p), in pennies: every cap under it (a
    // stall's price, a story's prize) keeps to it.
    static constexpr std::int64_t PieceMost = 5000;
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
    // A child's first pennies at its birth, from the better-off parent (given, never made); and a newcomer's purse, as an
    // authored resident's is to start with (made).
    static constexpr std::int64_t ChildPurse = 5, NewcomerPurse = 30;

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
