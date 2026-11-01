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
    // Birth: the child's sex, "male" or "female" (characters are male or female only: the user), decided with the
    // request (birthSex) so a deterministic run repeats; its pronouns follow (Game::pronounsOf). Empty: as the template.
    std::string sex;
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
    // live.economy): residents' needs (needsPaused: doc 66 Phase 5's pause, off by default), the Country spending back
    // what the store room takes in (country, keeping countryReserve), the gate toll (gateToll percent of each draw's cost
    // to the treasury), carriage (pennies a wagon load of CarriageLoad weight, in and out), the day's wages by kind of post,
    // and the goods the Country no longer supplies (countryOff: their real producers' now).
    bool needsPaused = false;
    bool country = true;
    std::int64_t countryReserve = 200;
    int gateToll = 30;
    int carriage = 6;
    int wageKeeper = 14, wageHand = 8, wagePublic = 8, wageCountry = 8;
    std::set<std::string> countryOff;
    // Services the treasury pays a share of (doc 68, 5: the city's taxes helping its businesses), by service, in percent:
    // "carriage" (a share of every delivery's carriage, in and out, paid to the carters). Only from what the treasury
    // holds over its float and tonight's public wages, and at most a twentieth of its money a day (SubsidyDay).
    std::map<std::string, int> subsidies{{"carriage", 25}};
    // The residents' comforts (doc 68, 3; the world's "comforts" record): pennies a head a household holds before it buys
    // them, and the percent of what is over that it spends a day. -1: country.json's (40, a fifth).
    int comfortsAbove = -1, comfortsShare = -1;
    // The loop's rules as this world has changed them (doc 68, Progress, "First fixes"; "rule" records, migration 0058),
    // by name (items::Rules); a rule not listed is as country.json says.
    std::map<std::string, int> rules;
    // The city's services as this world has changed them ("service" records): on or off, the price, about once every
    // `every` days a household. A service not listed is as country.json and items.json say; one they no longer have is
    // kept and does nothing.
    struct Service
    {
        bool on = true;
        int price = 1, every = 30;
    };
    std::map<std::string, Service> services;
    // The seasons in the Country's supply (country.json's `seasons`; doc 68, Progress, "Seasons and the DM's rules"), a
    // group at a time as this world has changed them ("season" records): supply, price and shelf by season in percent,
    // and the group's normal pieces a day. A group not listed is as country.json says.
    struct Season
    {
        std::array<int, 4> supply{100, 100, 100, 100}, price{100, 100, 100, 100}, shelf{100, 100, 100, 100};
        int normal = 0;
    };
    std::map<std::string, Season> seasons;
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
    // Since doc 68 the coins go to the Country (CountryFund), less the gate toll (to the treasury); carriage on the goods
    // goes to the carters. StoreFund is the old account, read only to move an older save's coins to the Country.
    static constexpr const char* StoreFund = "fund:store";
    static constexpr const char* CountryFund = "outside:country";
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

    // The city's loop (Docs/Design/68-the-country-and-the-city-loop.md, RatwCityLoop.cpp). Each game hour: households
    // buy food, needs and comforts (8:00-18:00), the Country buys its basket (8:00-18:00), wages are paid (18:00), shelves
    // are reckoned from the day's sales (0:00). Runs from decide() while !needsPaused().
    static constexpr int ShopOpen = 8, ShopShut = 18, PayHour = 18;
    static constexpr std::int64_t TillFloat = 20, TreasuryFloat = 200;
    // (The city's alms, doc 68, 4: the rule "almsADay" now, a day's food money a head and some over; it was 7.)
    static constexpr int CarriageLoad = 1000;         // A wagon load, in items.json's weight.
    static constexpr const char* CarriageFund = "fund:carriage";   // Carriage where a world has no carters' yard.
    // A market's purse ("outside:<id>"), and which market supplies a good (the first in country.json: one for now).
    static std::string marketFund(const std::string& market) { return "outside:" + market; }
    std::string marketFor(const std::string& item) const;
    // Who pays a post: its kind ("keeper", "hand", "watch", "public", "country"; "" for none), the paying account and the
    // wage. A shop's keeper and its hands (a post whose work room holds exactly one shop): its till; the watch: the
    // treasury; otherwise the first country.json wage rule with a word in its title; otherwise nobody.
    struct Pay
    {
        std::string kind, payer;
        int wage = 0;
    };
    Pay payOf(const Position& post) const;
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
    // The seasons in the Country's supply (doc 68, Progress, "Seasons and the DM's rules"): country.json's groups as this
    // world has them (EconomySpec::seasons), and where in the year the society is (0 at Spring's first day, up to 365;
    // -1 until it first decides, when no season applies: every factor 1, no cap). A factor is the group's percent for
    // the season whose middle day is nearest on either side, blended in a straight line, as a fraction (1: normal).
    const std::vector<items::SeasonGroup>& seasonTable() const { return seasons_; }
    const items::SeasonGroup* seasonGroup(const std::string& item) const;
    double yearDay() const { return yearDay_; }
    void setYearDay(double absoluteDay);           // (decide() does it; for tests and harnesses.)
    static double seasonal(const std::array<int, 4>& percentBySeason, double yearDay);
    double priceFactor(const std::string& item) const;
    double shelfFactor(const std::string& item) const;
    double supplyFactor(const items::SeasonGroup& group) const;
    // What the Country can still supply of a good's group today (its normal × the supply factor, less what was drawn of
    // the group today); a large number for a good outside every capped group.
    int countryLeft(const std::string& item) const;
    int groupCap(const items::SeasonGroup& group) const;
    int groupDrawn(const std::string& group) const;
    // The most `quantity` of a good costs a resident in a shop now (for whether it can pay): the catalogue's price, or the
    // season's (times its price factor) rounded up if dearer. What is paid: the catalogue's, then the difference.
    std::int64_t seasonalCost(const std::string& item, int quantity) const;
    // The world's economy settings replaced while it runs (the DM's economy.sync: doc 68, Progress): every rule the loop
    // and the store room read, at once; the treasury's and stores' starting amounts are kept (they were spent long ago).
    void adoptEconomy(const EconomySpec& economy);
    // The residents' comforts as this world has them (EconomySpec, else country.json).
    int comfortsAbove() const;
    double comfortsShare() const;
    // The loop's rules as this world has them (doc 68, Progress, "First fixes"): EconomySpec::rules, else country.json's,
    // else items::Rules' own; -1 for a name that isn't a rule.
    int rule(const std::string& name) const;
    // The city's services as this world has them (country.json's list, with this world's changes; the price items.json's
    // unless changed), in country.json's order.
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
    // business uses (country.json `suppliers`: the cooper for the wine makers' casks), and how many armed wolves a
    // patron's group has (its posts, or its rule).
    std::vector<std::string> citySellers(const std::string& buyerBusiness, const std::string& item) const;
    int unitsOf(const items::PatronGroup& group) const;
    // Buys up to `want` of an input for `till` from the city's sellers of it (till to till, at their price: the
    // catalogue's, the season's difference after), the seller holding most first; how many came. Nothing when the rule
    // "suppliers" is off. Only a craft's inputs: a good it sells on is still the store room's (bought at the seller's
    // price it would leave the reseller nothing).
    int buyFromCity(const std::string& till, const items::Business* business, const std::string& item, int want);
    // The crew's hours a till may hold in hand (workBankHours: that many hours of the crew; 0: the business's longest
    // craft and an hour more, at least two hours' worth).
    double workBankMost(const std::string& till, const items::Business* business) const;
    // What the loop did, for reports (world_check --economy): since the day began, and the day before's.
    struct LoopDay
    {
        std::int64_t day = -1;
        std::int64_t residentsSpent = 0, countrySpent = 0, countryUnspent = 0, drawn = 0, toll = 0, carriage = 0;
        std::int64_t wagesOwed = 0, wagesPaid = 0, profit = 0, subsidy = 0, alms = 0;
        // The treasury's wage support (paid to the posts of tills short of their wages; counted in wagesPaid too), and
        // the city's services: done, what households paid, and the treasury's share of it (doc 68, "First fixes").
        std::int64_t wageSupport = 0, servicesSpent = 0, servicesCity = 0;
        int servicesSold = 0;
        // Trades (doc 68, "Trades"): what keepers bought from the city's makers for their crafts (pieces, pennies); the
        // patrons' work done and new kit, what the treasury and the Country paid, and by business (in serviceTrades'
        // kind).
        int cityTradePieces = 0, patronJobs = 0, patronPieces = 0;
        std::int64_t cityTrade = 0, patronsTreasury = 0, patronsCountry = 0;
        int boughtAhead = 0;                        // 1 on a day households bought ahead for a Restday or festival.
        int householdsShort = 0, foodBought = 0, needsBought = 0, comfortsBought = 0, tillsShort = 0;
        // By business (the services' trades): services done and what they took in, the city's share included.
        struct Trade
        {
            int sold = 0;
            std::int64_t spent = 0;
        };
        std::map<std::string, Trade> serviceTrades, patronTrades;
        // By season group (doc 68, Progress, "Seasons"): pieces drawn from the Country, pieces sold to residents and
        // what they paid for them.
        struct Group
        {
            int drawn = 0, sold = 0;
            std::int64_t spent = 0;
        };
        std::map<std::string, Group> groups;
    };
    const LoopDay& loopToday() const { return loopToday_; }
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
    void countryBuys(int hourOfDay);
    void patronsBuy(int hourOfDay, std::int64_t day);   // Each trade's patrons, once a day while it is open.
    std::map<std::string, std::int64_t> patronsDay_;    // Business -> the day its patrons last bought (not saved).
    std::unordered_map<std::string, std::vector<std::int64_t>> drawnDays_;   // Till -> its draws a day, the last 28 days.
    std::int64_t tillFloat(const std::string& till) const;   // A till's working float (floatDrawDays' mean draws).
    void payWages();
    void supportWages();                            // The treasury over its cap pays the short tills' posts (18:00).
    void turnShelves();
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
    std::set<std::string> dead_;                    // Residents who have died (died, revived): nobody feeds or pays them.
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
