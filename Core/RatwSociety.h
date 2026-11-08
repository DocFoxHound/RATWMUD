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
#include "RatwMarket.h"
#include <limits>
#include "RatwStatus.h"

// The residents' society: their lives (schedules, posts, careers, families), and the money and goods they hold.
//
// The economy is staged (Docs/Design/79-a-staged-economy.md): only players' money is real (their purses, Chapters'
// treasuries and the escrow that holds their coins, kept to the penny: realBalanced()). Everything an NPC holds is
// staged: each household's band (RatwStatus.cpp) feeds it from its food shop's shelf and sets its purse nightly, the
// treasuries and tills are set within their ranges, and prices are the Market Board's (RatwMarket.cpp). Nothing here
// pays a wage, buys a household's food or sells on its own; a staged payer that must pay a player is topped up, and
// every coin to or from a player passes the faucet door (RatwStagedAccounts.cpp).

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
// Every prefix facilityAccount knows (doc 79, 9.1: a test checks classify sorts each).
const std::vector<std::string>& facilityPrefixes();
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
    // The House whose district a cell is in (by the House's name: "Greatly"; "" for none; World::districtHouse: the cell's
    // own claim by a House, else the street its door leads out to): the Crown's charters' (Docs/Design/75, 4.2).
    std::function<std::string(const std::string& cell)> houseOf;
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
    std::int64_t purse = 0;       // Arrival: the pennies they bring (a plain amount, set: doc 79, 6.5).
    double skill = 0;             // Arrival: what it knows of the post's work (its trade suiting it: as the trade needs).
    std::vector<std::string> parents;
    // Birth: the child's sex, "male" or "female" (characters are male or female only: the user), decided with the
    // request (birthSex) so a deterministic run repeats; its pronouns follow (Game::pronounsOf). Empty: as the template.
    std::string sex;
    // "farm" (Docs/Design/71-the-country-s-industries.md, Phase C): an unplaced NPC placed by the farms' rule when a farm's
    // has a post to fill (Society::staffFarms): a farm's keeper or hand, a community market's keeper or hand (a post of
    // its own: `work`, `workLabel`, `role`, the hours), or a keeper's family (no work: workLabel "-"). `farm`: its farm,
    // or "market:<community>".
    Spot work, serve;                             // (serve: a market keeper's customers' spot, found by the world.)
    std::string workLabel, role, farm;
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

// What the staged economy keeps between days (Docs/Design/79-a-staged-economy.md, Phase 2; the save's "staged", only when
// there is any: a save without it reads as nothing staged yet). Nothing here is money: the coins stay in the accounts.
struct StagedState
{
    struct Band
    {
        int band = status::Comfortable;
        std::int64_t since = 0;                     // The day it took this band.
        std::int64_t liftedUntil = -1;              // Lifted by a player's help until this day (-1: not).
    };
    std::map<std::string, Band> bands;              // By household (Society::householdOf's key).
    struct Override
    {
        int band = status::Comfortable;
        std::int64_t until = 0;                     // The day it ends.
    };
    std::map<std::string, Override> overrides;      // A DM's band for a household (6.3, precedence 1).
    std::map<std::string, std::int64_t> holds;      // Account -> the day its hold ends (6.5, holdUntil).
    std::map<std::string, std::int64_t> lean;       // Account -> the day its lean days end (6.5).
    std::map<std::string, std::int64_t> escrowSource;   // Escrow account -> how many of its coins came from the staged side.
    struct Event
    {
        std::string kind;                           // bereaved, lostPost, robbed, failing, helped, earning.
        std::int64_t day = 0;
        double weight = 0;
    };
    std::map<std::string, std::vector<Event>> events;   // Resident -> its status events of the last eventDays days.
    struct Tally
    {
        std::int64_t made = 0, sunk = 0;            // Coins to and from players.
        std::int64_t goodsMade = 0, goodsSunk = 0;  // Goods, at the catalogue's price (for the report, not the balance).
    };
    std::map<std::int64_t, std::map<std::string, std::map<std::string, Tally>>> faucets;   // Day -> holder -> reason.
    std::map<std::string, std::int64_t> welcomed;   // Account holder -> the game week of its last welcome in coins.
    // Goods the world no longer knows (doc 79, 14), set aside at restore from a real or escrow account: account -> good ->
    // pieces. Nothing made, nothing lost: they go back into the account when the catalogue knows the good again.
    std::map<std::string, std::map<std::string, int>> quarantine;
    // (The slider the bands read is the Board's: SocietyState::board, doc 79, 5.2.)
    std::int64_t hour = -1, day = -1;               // The last staged hour run and day turned (-1: not yet).
    bool empty() const
    {
        return bands.empty() && overrides.empty() && holds.empty() && lean.empty() && escrowSource.empty() && events.empty() &&
               faucets.empty() && welcomed.empty() && quarantine.empty() && hour < 0 && day < 0;
    }
};
struct SocietyState
{
    bool enabled = false;
    std::map<std::string, EconomyAccount> accounts;
    std::map<std::string, ResidentLife> residents;
    std::vector<EconomyEntry> ledger;
    std::int64_t nextEntry = 1, budgetDay = 0;      // (budgetDay: the society's day clock.)
    int herbPatch = 40;                             // (The demo roster's.)
    double decisionRemainder = 0;
    CareerState careers;
    market::Board board;                            // The Market Board (doc 79, Phase 1): saved only while not empty.
    StagedState staged;                             // (Doc 79, Phase 2.)
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
// A world's economy settings (its "economy", "loop", "rule", "service" and "season" records; live.economy): what the
// treasury and its stores start with, whether residents' needs are paused, the rules as this world has changed them, and
// the services and season groups it has changed (doc 79, 14: what stays of the economy's settings).
struct EconomySpec
{
    std::int64_t treasury = 1000;
    int storeHerbs = 100, storeMeals = 50;
    // Residents' needs paused (doc 66 Phase 5's pause, off by default): nobody grows hungry, nobody is fed.
    bool needsPaused = false;
    // The rules as this world has changed them ("rule" records), by name (items::Rules); a rule not listed is as
    // Data/Economy/market.json `rules` says, else items::Rules' own.
    std::map<std::string, int> rules;
    // The city's services as this world has changed them ("service" records; market.json `services`): on or off, the
    // price, about once every `every` days a household. Kept for the services to come; nothing reads them yet.
    struct Service
    {
        bool on = true;
        int price = 1, every = 30;
    };
    std::map<std::string, Service> services;
    // The season groups as this world has changed them ("season" records; market.json `seasons`): supply, price and shelf
    // by season in percent, and the group's normal pieces a day. The Board reads `price` and `shelf` (doc 79, 7.1).
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
    // A child the host could not make: owed again, due now.
    void birthFailed(const ResidentRequest& request);
    // An arrival from the Country (ResidentRequest "arrival"): the unplaced NPC becomes a resident at the home chosen for
    // it, placed without work (it holds the post it came for once welcomed), with the purse the request names (a plain
    // amount, set: doc 79, 6.5). False if it is no longer unplaced.
    bool arrive(const ResidentRequest& request);
    std::size_t unplacedCount() const { return authored_.unplaced.size(); }
    // Jobs by class (Docs/Design/67-ridgemere-2-the-city.md, Progress, "Jobs by class"): a post's class, "upper" (a House's
    // own, a larger independent business, high office), "middle" (a shop, a stall, a counter, a Great House's works run for
    // it, lower office), "lower" (hands, waged and service work, the Country's labour), "any" (the Watch, the clergy: their
    // own cases), or "" (no post that pays: lessons, play, leisure); and a wolf's class, resident or unplaced ("upper",
    // "middle" or "lower"): its own work's as written, else its family's, else its household's, else middle.
    // (With `pass`, a pass over many wolves at once, as the staged day's turn: ClassPass below.)
    struct ClassPass;
    std::string postClass(const Position& p, ClassPass* pass = nullptr) const;
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
    // Restores a checkpoint, leniently (doc 79, 14): the world may have gained or lost residents since it was written;
    // those still here keep their saved lives, new ones start fresh, the staged accounts of those gone and every dead
    // account (9.1) are dropped, unknown goods are quarantined in real and escrow accounts and dropped from staged ones,
    // and staged accounts out of range are settled. Real and escrow money is kept to the penny. False only for a broken
    // save (a real or escrow account negative, overflowing or past the goods limits; a broken ledger, counter or life).
    bool restore(const SocietyState& candidate);
    // A resident's needs set (a live world's copied state, Game::applyExternalNpcStates), each within 0..100.
    void setNeeds(const std::string& id, double hunger, double fatigue);
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
    // A player's trade with a shop (its keeper `merchant`, keeping it): its goods (wares) at the Market Board's price
    // (doc 79, 7; RatwMarket.cpp: the per-piece ladder at its pressure, inside the clamp, with I1-I5), the shop's budget,
    // take allowance and the account holder's caps; `holder` is the player's account holder (the Game's accountOf; empty:
    // the player). What the shop's list, the keeper's word and the trade itself say is this one price.
    EconomyResult quote(const std::string& player, const std::string& merchant, const std::string& item,
                        int quantity, bool buy, const std::string& holder = {}) const;
    EconomyResult trade(const std::string& player, const std::string& merchant, const std::string& item,
                        int quantity, bool buy, const std::string& holder = {});
    // The Market Board (Docs/Design/79-a-staged-economy.md, 5 and 7; RatwMarket.cpp). The markets the world found (World::ensureTradePoints), else Society's own (one market of every till);
    // a till's market; the keepers' quarters (their voice tables', for the district factor); the DM's prosperity (the
    // Board's slider heads there by SliderStepADay a day; `now` skips it); the Board's rules (the rules of the names).
    const market::Board& board() const { return state_.board; }
    void setTradePoints(market::Topology topology);
    const market::Topology& tradePoints() const;
    std::string marketOf(const std::string& till) const;
    // A household's market, the Board's own (the bands' cut, Times and pressure, its food shop): its home's (a market's
    // region, or the region of homes it serves), else a member's shop's or workplace's, else the first market's.
    std::string marketOfHome(const std::string& homeCell, const std::vector<std::string>& members) const;
    void setQuarters(std::map<std::string, std::string> quarters);
    void setProsperity(int target, bool now = false);
    const market::Rules& marketRules() const { return marketRules_; }
    // The Board's clock (game days): decide()'s, or a test's.
    void setMarketClock(double absoluteDay) { marketAt_ = absoluteDay; }
    double marketClock() const;
    std::int64_t marketDay() const;               // The market day (it turns at dawn: market::DawnHour).
    double pressure(const std::string& market, const std::string& base) const;   // P now (decayed).
    double pushedBy(const std::string& market, const std::string& holder, const std::string& base) const;
    double depthOf(const std::string& market, const std::string& base) const;
    // The factor before pressure: the season (rescaled), region, district and Times.
    double baseFactor(const std::string& till, const std::string& item) const;
    // I3's backstop's base: the cheapest lot of `item` and `quantity` any shop could sell now (7.6).
    std::int64_t cheapestLot(const std::string& item, int quantity) const;
    double cheapestEach(const std::string& item) const;
    // I5's cap (7.6): the most a sold lot of a crafted good pays, 1.6 × its inputs' cheapest buy price (none: no cap).
    std::int64_t craftCap(const std::string& item, int quantity) const;
    std::int64_t budgetOf(const std::string& till) const;     // Today's whole buying budget.
    std::int64_t budgetLeft(const std::string& till) const;   // What is left of it today.
    int takeAllowance(const std::string& till, const std::string& item) const;
    int takenToday(const std::string& till, const std::string& item) const;
    std::int64_t soldToday(const std::string& holder) const;  // Pennies shops paid this holder today.
    // The faction discount's weekly cap per Chapter (7.8, D28; the rule factionDiscountMost): what is left this week,
    // and the discount paid.
    std::int64_t discountRoom(const std::string& chapter) const;
    void discountPaid(const std::string& chapter, std::int64_t pennies);
    // The discount itself, merchant to player (the Game's afterFactionTrade): staged, at most the Chapter's room this
    // week, recorded as paid; the merchant topped up by shift (Phase 2's topUp: a staged payer), so the cap here is the
    // only limit on it and it is checked once, here. Off: what the merchant's purse pays, as ever. What was paid.
    std::int64_t payFactionDiscount(const std::string& merchant, const std::string& player, const std::string& chapter,
                                    std::int64_t back);
    // A row of the shop list (7.1): the list count (12 for a good under 3p) and the price of that lot, the lot the
    // buy button buys (the list count, or what is on the shelf), the smallest sell lot worth a penny and its quote, the
    // trend (+1 ▲, -1 ▼) with its cause in words, and the shop's buying budget left today.
    struct Listing
    {
        int listCount = 1, buyCount = 1, sellLot = 0, trend = 0;
        std::int64_t listPrice = 0, buyingToday = 0;
        EconomyResult buy, sell, buyOne, sellOne;   // (The lots', and one piece's: the list's unit prices.)
        std::string cause;
    };
    Listing listing(const std::string& player, const std::string& merchant, const std::string& item, const std::string& holder = {}) const;
    // The Board's hour (decide() calls it while staged): the market day's turn at dawn (marketMorning: yesterday's net
    // positions ripple through the recipes, the day's counts reset, the sweep, the slider's step).
    void marketTick(double absoluteDay);
    void marketMorning(std::int64_t day);
    // A ware's shelf target (7.7) and the morning restock (a keeper's first time at work in the market day; true when
    // anything was set out), and whether its keeper is still seen setting out the wares (a few minutes after).
    int shelfTarget(const std::string& till, const std::string& ware) const;
    bool stagedRestock(const std::string& keeper);
    bool settingOut(const std::string& keeper) const;
    std::uint64_t rosterRevision() const { return rosterRevision_; }
    // What a shop sets out (7.7: its wares less what it only buys, unless it makes or supplies them): the restock's, and
    // a market's `sold` (I3's backstop: a good no shop sets out is priced at the clamp's floor).
    std::vector<std::string> stagedShelfGoods(const std::string& founder) const;
    // Builds what the shop lists read lazily (Society's own markets, each market's least district factor, the keepers'
    // wares), so the snapshot threads only read (World::prepareReading; doc 31, Phase 4). Only while staged.
    void prepareMarket() const;
    // The farm and works crews by community (a market's catchment counts its own: 7.3): every post at a farm or works.
    std::map<std::string, int> farmCrews() const;
    // A good's pressure set outright, within its range (the DM's "reset prices" to come, Phase 9; and tests), with its
    // cause; and a merchant put at a stall or back in its shop (decide() sets them each second; for tests).
    void setPressure(const std::string& market, const std::string& base, double P, const std::string& cause = "event");
    void setAtStall(const std::string& merchant, bool at)
    {
        if (at)
            atStall_.insert(merchant);
        else
            atStall_.erase(merchant);
    }
    // A good's catalogue price a piece, in pennies (its quality's: items.json, doc 35): every shop's.
    static std::int64_t catalogPrice(const std::string& item);
    // What a shop pays a player for a good (doc 15: a little over half): the Market Board's SellShare, one constant.
    static constexpr double SellBackShare = market::SellShare;
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
    // Goods quarantined in an account at restore (doc 79, 14: "goods the world no longer knows"), or null.
    const std::map<std::string, int>* quarantined(const std::string& account) const
    {
        const auto found = state_.staged.quarantine.find(account);
        return found == state_.staged.quarantine.end() ? nullptr : &found->second;
    }
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
    // anywhere and nobody is fed; a hungry one (a save's) still eats what it carries. A switch (EconomySpec::needsPaused),
    // off by default. Always paused outside an authored world.
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
    static constexpr int ShopOpen = 8, ShopShut = 18;   // (A household's food hour falls between: doc 79, 6.5.)
    static constexpr int ShelfMost = 1000;            // The most of one good a shelf's target sets out (7.7).
    // A resident needs about this much nourishment a day (hunger grows .0035 a second; food counts 1.1 times when eaten).
    static constexpr double NourishADay = .0035 * 14400 / 1.1;
    // The households (doc 79, 6.2): by home, joined by kin (a child under 16, a spouse), by a key each. With `homeless`,
    // a resident with no home is a household of its own ("|<id>"), so the homeless are counted and fed.
    std::map<std::string, std::vector<std::string>> households(bool homeless = false) const;
    // A household's class (doc 79, 6.1): upper where any member is, else most of its members' (middle at a tie).
    std::string householdClass(const std::vector<std::string>& members, ClassPass* pass = nullptr) const;
    // Every household's class at once (the staged day's turn), each the same as householdClass's: with one ClassPass,
    // each wolf's class, each work room's shops and each home's residents are worked out once for the pass, not once a
    // member (O(residents + posts), not their product: dev-old's 1,083 residents took 13 ms, one by one).
    std::map<std::string, std::string> householdClasses(const std::map<std::string, std::vector<std::string>>& homes) const;
    // What a pass over the classes works out once (nothing outside it changes while it runs: a local, never a member,
    // so no thread shares one). Each cache filled on first use.
    struct ClassPass
    {
        std::map<std::pair<std::string, int>, std::string> classes;                     // classOf(id, depth).
        bool shopsBuilt = false, homesBuilt = false, kinBuilt = false;
        std::unordered_map<std::string, std::pair<int, const Position*>> shops;            // Work cell -> shops, the last.
        std::unordered_map<std::string, std::vector<const std::string*>> homes;            // Home cell -> residents, by id.
        std::unordered_map<std::string, std::vector<const Kin*>> kin;                      // Wolf -> its written kin records.
    };
    // What a post is (doc 79, 6.3): "keeper" (a merchant), "farm" (a farm's or a
    // works' post: `farm`, `farmKeeper`), "watch" (a guard; `rule` its head's wage rule, or -1), "hand" (a post whose
    // work room holds exactly one shop: `shop`), "public" or "estate" (the first postRules() rule with a word in its title:
    // `rule`), or "" (none).
    struct PostKind
    {
        std::string kind;
        int rule = -1, farm = -1;
        bool farmKeeper = false;
        const Position* shop = nullptr;
    };
    PostKind postKind(const Position& post, ClassPass* pass = nullptr) const;
    // The posts' rules by the words of their titles (city_loop.json's `wages` before doc 79; RatwCareers.cpp): each its
    // name, payer ("treasury" or "estates") and wage word ("keeper": a head's), in order.
    static const std::vector<items::WageRule>& postRules();
    // The staged steps (doc 79, 6.5 and 9.2; RatwSociety.cpp), each game hour while !needsPaused():
    // the households and their classes at the day's turn, each household's food at its hour (8:00-18:00), and the
    // nightly set at 0:00. The purse a resident is drawn toward (its household class's range: lower 3-12p, middle 12-60p,
    // upper 60-300p, the homeless 0-3p) and the treasuries' range (status.json's treasuryFloor to treasuryCeiling).
    void stagedHourly(std::int64_t hour);
    void stagedFood(const std::string& home, const std::vector<std::string>& members, const std::string& cls);
    void stagedNight(std::int64_t day);
    std::int64_t stagedPurse(const std::string& id, const std::string& home, const std::string& cls, std::int64_t day) const;
    // A staged account set to `coins` (doc 79, 9.2: a raw set, no ledger or journal entry, nothing counted).
    void stagedSet(const std::string& account, std::int64_t coins);

    // ---- Doc 79, Phase 2: bands and staged accounts (RatwStagedAccounts.cpp; sections 6 and 9) ----
    // A resident's economic status (6.2): its household's band and class, the household's score, whether a DM's override
    // or a guarantee set it, and until when a player's help lifts it. Worked out once a day at the staged day's turn
    // (single-threaded) and at restore; the threaded half of deciding only reads it. Null for one in no household (a
    // player, the dead).
    struct Status
    {
        int band = status::Comfortable;
        std::string cls, household, market;         // Class (upper, middle, lower; homeless), its household and market.
        double score = 0;
        bool overridden = false, guaranteed = false, reason = false;   // (reason: one that may be destitute.)
        std::int64_t liftedUntil = -1;
    };
    const Status* statusOf(const std::string& id) const;
    std::string householdOf(const std::string& id) const;   // "" for none.
    // The status table this world runs (status.json's, with this world's override over it: setStatusOverride).
    const status::Table& statusTable() const;
    // A host's or a test's changes to status.json (doc 79, 12; Phase 9 reads them from live.economy.rules): read over the
    // defaults by status::read, lenient; null clears them. Takes effect at the next day's turn.
    void setStatusOverride(const json::Value* v, std::vector<std::string>* warnings = nullptr);
    // The day's pass (public for tests): households scored, ranked and cut into bands. `now` (the DM's "now") skips the
    // slider's smoothing and the bands' hysteresis.
    void stagedBands(std::int64_t day, bool now = false);
    // A DM's band for a household until a day (-1 band: cleared), and a player's help lifting one (6.7). Both take effect at
    // once (the bands are worked out again).
    bool overrideBand(const std::string& household, int band, std::int64_t until);
    bool liftHousehold(const std::string& household, std::int64_t until);
    // The smoothed slider the bands read, and the DM's setting it heads to (-100 hard times .. 100 boom): the Board's one
    // slider (setProsperity, above; Phase 9 sets it from live.economy's `prosperity` rule).
    double prosperity() const { return double(state_.board.slider); }
    int prosperitySetting() const { return state_.board.sliderTarget; }
    // The kind of an account (9.1), by its prefix: real (a player, a Chapter), escrow (holding real coins in trust, with
    // its staged-sourced part counted apart), staged (elastic: residents, tills, treasuries, town works), or dead (the old
    // conserving economy's: dropped at restore with their goods).
    enum class AccountKind
    {
        Real,
        Escrow,
        Staged,
        Dead
    };
    static AccountKind classify(const std::string& id);
    // The account holder behind a player's character (doc 79, words: the central account, else the login, else the
    // character): every per-player cap counts by it. The Game sets it as a character enters; not saved.
    void setHolder(const std::string& player, const std::string& holder);
    std::string holderOf(const std::string& account) const;
    // The faucet door (9.3): coins and goods to and from players, by holder and reason, each day (log only in play, D16).
    // A cap's room left today for a player's holder (status.json caps: "rewardsADay", "theftADay", "stakesADay"), and
    // what a holder has had today from a group of faucets ("reward", "theft", "stakes", "welcome", "refund", "trade").
    std::int64_t faucetRoom(const std::string& player, const std::string& cap) const;
    std::int64_t faucetToday(const std::string& holder, const std::string& group) const;
    static std::string faucetGroup(const std::string& reason);
    static bool registeredFaucet(const std::string& reason);   // (D16: a reason in a group the door knows.)
    // Real money (9.4): player and Chapter cash plus escrow's real coins; balanced while it equals what it opened at plus
    // the faucets less the sinks since (the test gate; never checked in play). openRealBalance starts the count again.
    std::int64_t realTotal() const;
    bool realBalanced() const;
    void openRealBalance();
    std::int64_t realFaucets() const { return faucetTotal_; }
    std::int64_t realSinks() const { return sinkTotal_; }
    // What a payer can pay a player now (9.2): all of it from a staged payer (topped up on demand by shift) but a till,
    // else what it holds (a till's, as topUp never tops one up). With the staged economy off, what it holds.
    std::int64_t payable(const std::string& payer, std::int64_t amount) const;
    // What of an escrow's payout of `coins` a player may have under a daily cap (9.3; "rewardsADay" for prizes and
    // rewards): its real coins whole, its staged-sourced ones within the holder's room today, the rest sent back to `back`
    // (a staged account; the treasury if not) at once. The caller pays what this returns. Off, or not escrow to a
    // player: `coins`.
    std::int64_t cappedPayout(const std::string& escrow, const std::string& player, std::int64_t coins, const std::string& cap,
                              const std::string& back = {});
    // A table game broken up (Game::endTable with no winner): the pot shared evenly among those still seated, its odd
    // pennies to `house`. Staged, each player's share is a payout under the stakes cap like a winner's (cappedPayout,
    // "stakesADay", the excess to `house`), and residents still seated are paid first, so they take back the pot's
    // staged part before any reaches a player (doc 79, 9.3). Off: the shares in seat order, as before.
    void shareStakes(const std::string& pot, const std::vector<std::string>& seats, const std::string& house);
    // The door's log (D16, log only in play): reasons staged coins reached a player by that are in no faucet group the
    // door knows (each once a day, with its holder and coins: the reason names its call site), and the pennies staged
    // payers were topped up by since the world began. Taken by the Game for its log.
    std::vector<std::string> takeDoorNotes();
    std::int64_t stagedTopUps() const { return topUps_; }
    // Doc 79, 6.5: a born child or an arrival placed in a post takes its household's band at once (the next pass
    // re-ranks it with the rest); one in a household of its own, at the next hour's pass.
    void stagedJoined(const std::string& id, const std::vector<std::string>& kin);
    // The holders whose day's tallies or welcome changed since last taken (for the journal: doc 79, 9.2), each (day, holder).
    std::vector<std::pair<std::int64_t, std::string>> takeStagedChanged();
    // The accounts whose quarantined goods the last restore moved (set aside or given back: doc 79, 14), since last taken:
    // the Game journals them at once, so a crash before the next checkpoint replays them as moved (Game::journalRestored).
    std::set<std::string> takeQuarantineMoved() { return std::exchange(quarantineMoved_, {}); }
    bool leanNow(const std::string& account) const;   // Robbed in its lean days: not refilled, and a second theft finds nothing.
    bool heldNow(const std::string& account) const;   // Keeping a change (a theft, a gift, a prize, players' pay) for now.
    // A status event for a resident (6.3: bereaved, lostPost, robbed, failing, helped, earning), today, read by the score
    // for eventDays. (Most are noted by the door itself: a theft, a player's gift or pay.)
    void noteStatusEvent(const std::string& resident, const std::string& kind, double weight = 0);
    // What Phase 1's Board gives the bands (doc 79, 5): a market's Times (-2 hard .. 2 boom), a good's pressure there, and
    // the shelf a household's food comes from (how many of a good a till gives up now, at most `want`). Until Phase 1
    // fills them: ordinary Times, no pressure, and food made (Phase 0's), so no shelf is ever bare. tillShelf is the
    // plain shelf (the till's own stock), for Phase 1 and the tests.
    struct StagedHooks
    {
        std::function<int(const Society&, const std::string& market)> times;
        std::function<double(const Society&, const std::string& market, const std::string& good)> pressure;
        std::function<int(Society&, const std::string& till, const std::string& good, int want)> shelf;
    };
    void setStagedHooks(StagedHooks hooks) { hooks_ = std::move(hooks); }
    static int tillShelf(Society& s, const std::string& till, const std::string& good, int want);
    // The Board's own hooks (the merge of Phases 1 and 2): a market's Times (marketTimes), a good's pressure there, and the
    // shelf: the till's stock, which the morning restock sets out (stagedRestock, its targets covering the day's food
    // provision: provisionFor) and players buy down, so the poor it serves go short when players empty it (6.5, T2). The
    // World installs them while staged (World::ensureTradePoints), in play and in sims; tests may too.
    static StagedHooks boardHooks();
    bool stagedHooked() const { return bool(hooks_.shelf); }
    // A market's Times, -2 hard .. 2 boom (5.3: the slider for now; harvests, raids, events and the DM's nudge come in
    // Phases 4, 5, 7 and 9). Never any player-caused pressure. The Board's prices and the bands both read it.
    int marketTimes(const std::string& market) const;
    // The pieces of a good the households whose food shop is this till take from it in a day (6.5; at most two days' food a
    // head, a Restday's buying ahead too), worked out with the bands: what the shelf target covers besides players (7.7).
    int provisionFor(const std::string& till, const std::string& ware) const;
    // (The quarters a resident lives in, "heights", "estate", "sump", for the score and the guarantees: setQuarters, above,
    // the same map the Board's district factor reads.)
    // The day's provisions (6.6), each (a household's member, its food keeper), for the world's familiarity step; taken.
    std::vector<std::pair<std::string, std::string>> takeProvisions();
    // The pay word for a resident's post (11): pennies a day at the slider, by its kind; 0 for no post that pays.
    std::int64_t payWord(const std::string& id) const;
    // The Mind's "Means" note (11, at most 300 characters): the band, how it gets by, the pay word, the market's Times.
    std::string meansNote(const std::string& id) const;
    // Where in the year the society is (0 at Spring's first day, up to 365; -1 until it first decides, when no season
    // applies): the Board's season factors read it (doc 79, 7.1).
    double yearDay() const { return yearDay_; }
    void setYearDay(double absoluteDay);           // (decide() does it; for tests and harnesses.)
    // The Country's farms and works as places of work (doc 71, Phase C; doc 79, 17; RatwFarms.cpp): each farm of
    // country_farms.json with its crew of posts (keepers and hands) and its work rooms; how many of its posts there are
    // and how many are held (counted each day). No money: every farm is on, and nothing pays or feeds through it.
    struct FarmNow
    {
        const items::FarmDef* def = nullptr;
        std::size_t area = 0;                       // Its area in items::farms().areas.
        int keepers = 0, hands = 0;                 // Its crew as written.
        bool outdoors = false;                      // A works' work room is the cell out of doors its centre is in (doc 72).
        int postKeepers = 0, postHands = 0, heldKeepers = 0, heldHands = 0;
    };
    const std::vector<FarmNow>& farms() const { return farms_; }
    // The farms' people (doc 71, Phase C; RatwFarmPeople.cpp): the farm a post is at (its work room one of the farm's
    // workplaces, items::farmPeople()), -1 for none, and whether it is the farm's keeper's (its title the keeper's label);
    // the labels a farm's posts have; and how many have been placed since the start (not saved), by "keeper", "family",
    // "hand <community>", "market <community>" (a market's keepers and hands).
    int farmOfPost(const Position& p, bool* keeper = nullptr) const;
    std::string farmLabel(const FarmNow& farm, int post) const;   // 0: its keeper's; 1: a hand's; 2: its second's.
    // The cell a post's holders are judged near from (CareerWorld::near, the lodgings): its work's, but for a works' post
    // out of doors (doc 72) its crew's home ground: its community's (a home there), or the city's for a works whose crew
    // lives in Ridgemere (a city farm's work room), as staffFarms places them.
    std::string reachOf(const Position& p) const;
    const std::map<std::string, int>& farmPlaced() const { return farmPlaced_; }
    // The world's economy settings replaced while it runs (the DM's economy.sync): its rules and seasons at once; the
    // treasury's and stores' starting amounts are kept (they were spent long ago).
    void adoptEconomy(const EconomySpec& economy);
    // A rule as this world has it: EconomySpec::rules, else market.json `rules`, else items::Rules' own, within its range;
    // -1 for a name that isn't a rule.
    int rule(const std::string& name) const;
    static constexpr int SmithSwords = 3;            // A smith's swords on its shelf to begin with.
    static constexpr int CheapPrice = 6;              // A shop's handful of goods cost at most this (pennies), for now.
    static constexpr int GoodsKept = 4;               // How many of each good a new shop starts with, and buys up to.
    // A new player's own meals to start with (doc 36), and its welcome's coins (doc 79, 9.2; status.json's caps over them).
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
    // A child's first pennies at its birth, from the better-off parent; and what an arrival from the Country brings: its
    // own purse up to this (a plain amount, set: doc 79, 6.5).
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
    // The staged economy's hours (doc 79): the last it ran (-1: not yet; the first is not run), and today's households
    // with their class and the hour each is fed (rebuilt at the day's turn; the hour and day saved in StagedState).
    std::int64_t stagedHour_ = -1, stagedDay_ = -1;
    struct StagedHome
    {
        std::vector<std::string> members;
        std::string cls;
        int hour = ShopOpen;
        int band = status::Comfortable;             // (Doc 79, Phase 2: its band, market, food shop and food.)
        std::string market, till, food;
        bool lifted = false;
    };
    std::map<std::string, StagedHome> stagedHomes_;
    void stagedHouseholds(std::int64_t day);
    // The Market Board's (doc 79, Phase 1; RatwMarket.cpp). None of these is saved but the Board (state_.board).
    struct StagedLot
    {
        std::string market, base;
        double sum = 0, P0 = 0, P = 0, push = 0;
        std::int64_t lot = 0;
    };
    StagedLot stagedLadder(const std::string& till, const std::string& item, int quantity, bool buy, const std::string& holder,
                           bool stall, bool atRest = false) const;
    // A ladder's start, worked out once a quote (the market, the factor before pressure, the depth, P, the holder's push
    // and cap), and the ladder run from it for a lot.
    struct LadderStart
    {
        std::string market, base;
        double depth = market::DepthLeast, factor = 1, each = 1, P = 0, push = 0, cap = .15;
    };
    LadderStart ladderStart(const std::string& till, const std::string& item, const std::string& holder, bool atRest = false) const;
    StagedLot ladder(const LadderStart& start, int quantity, bool buy, bool stall) const;
    struct SellBounds;
    EconomyResult stagedQuote(const std::string& player, const std::string& seller, const std::string& item, int quantity,
                              bool buy, const std::string& holder, StagedLot* lot, const SellBounds* bounds = nullptr) const;
    EconomyResult stagedTrade(const std::string& player, const std::string& trader, const std::string& item, int quantity,
                              bool buy, const std::string& holder);
    // I3's and I5's bases for a good, worked out once a quote (sellBounds): the least factor any shop setting it out could
    // sell it at now (infinity: none does), the least its recipes' inputs cost a piece made (infinity: no recipe), and its
    // quality's share of the base's catalogue price (at least 1).
    struct SellBounds
    {
        double least = std::numeric_limits<double>::infinity(), craft = std::numeric_limits<double>::infinity(), quality = 1;
    };
    SellBounds sellBounds(const std::string& item) const;
    std::int64_t cheapestLot(const std::string& item, const SellBounds& bounds, int quantity) const;
    std::int64_t craftCap(const SellBounds& bounds, int quantity) const;
    std::int64_t stagedSoldFor(const std::string& seller, const std::string& item, const LadderStart& start, const StagedLot& sold,
                               int quantity, const SellBounds& bounds) const;
    int stagedLeastLot(const std::string& seller, const std::string& item, const LadderStart& start, int from, const SellBounds& bounds) const;
    market::Rules readMarketRules() const;
    market::Rules marketRules_;                     // (readMarketRules, as buildRules last read the world's rules.)
    bool exchange(const std::string& seller, const std::string& buyer, const std::string& item, int quantity,
                  std::int64_t total, const std::string& kind);
    void rollMarket();
    void stepSlider(std::int64_t day, bool now = false);   // (The one slider's day's step: the Board's and the bands'.)
    double stagedSeason(const std::string& item) const;
    // A good's season group (market.json's, 7.1) with this world's season record laid over its price and shelf.
    const market::Data::Season* stagedSeasonGroup(const std::string& base, std::array<int, 4>* price, std::array<int, 4>* shelf) const;
    double districtFactor(const std::string& till, const std::string& base) const;
    double districtGroupFactor(const std::string& till, const std::string& group) const;
    const std::map<std::string, double>* districtRow(const std::string& till) const;
    double marketBase(const std::string& market, const std::string& base, double district) const;
    double leastDistrict(const std::string& market, const std::string& base) const;
    market::Topology worldTopology_;
    mutable market::Topology ownTopology_;
    mutable std::uint64_t ownTopologyKey_ = 0;
    mutable std::unordered_map<std::string, double> districtLeast_;   // Market|group -> its least district factor,
    mutable bool districtWarm_ = false;                                // filled whole (prepareMarket) before any read.
    std::map<std::string, std::string> quarters_;
    double marketAt_ = -1;
    // Doc 79, Phase 2 (RatwStagedAccounts.cpp): each resident's status (rebuilt with the bands), the household each is
    // in, the world's status override, the holders, the Board's hooks, the day's provisions, who held a
    // post at the last pass (for a lost post), and the real money's count since it opened. None saved.
    std::unordered_map<std::string, Status> status_;
    std::shared_ptr<const status::Table> statusOverride_;
    std::unordered_map<std::string, std::string> holders_;
    StagedHooks hooks_;
    std::vector<std::pair<std::string, std::string>> provisions_;
    std::unordered_map<std::string, std::map<std::string, int>> provisionDemand_;   // Till -> food -> pieces a day (provisionFor).
    std::set<std::string> postedAtPass_;
    std::unordered_map<std::string, std::string> jobCategories_;   // (talk::jobCategory by post, label, role, child or not.)
    std::int64_t realOpening_ = -1, faucetTotal_ = 0, sinkTotal_ = 0;
    std::int64_t topUps_ = 0;                       // Pennies staged payers were topped up by (the door's log).
    std::vector<std::string> doorNotes_;            // (D16: unregistered faucet reasons, for the Game's log.)
    std::set<std::string> doorNoted_;               // (Those noted today: "day|reason".)
    std::set<std::pair<std::int64_t, std::string>> stagedChanged_;   // (Holders whose tallies or welcome changed: the journal's.)
    std::set<std::string> quarantineMoved_;         // (Accounts whose quarantine the last restore changed: the journal's.)
    bool stagedRecount_ = false;                    // (A new household member alone: the bands again at the next hour.)
    // The door (9.3), from record: a move between accounts sorted and counted.
    void stagedDoor(const std::string& kind, const std::string& from, const std::string& to, const std::string& item, int quantity,
                    std::int64_t coins);
    void tally(const std::string& holder, const std::string& reason, std::int64_t made, std::int64_t sunk, std::int64_t goodsMade,
               std::int64_t goodsSunk);
    // A staged payer short of what it must pay a real or escrow account, topped up first (9.2); logged.
    void topUp(const std::string& payer, const std::string& payee, std::int64_t coins);
    // The nightly trims and settles (9.2): staged wallets' goods, the old tallies, events and holds.
    void stagedTrim(const std::string& id, int band);
    void stagedPrune(std::int64_t day);
    // At restore (9.2): the staged hour and day as saved, the bands worked out from the saved state, and staged accounts out
    // of range settled.
    void stagedRestored();
    // A household's food keeper (the till) and the good it gives, chosen at the day's turn (6.5).
    void stagedKeepers();
    void stagedHomesFor(std::int64_t day);          // (Today's households and their classes, before the bands.)
    bool restoring_ = false;                        // (At restore: each household keeps its saved band.)
    // The rules as this world has them (buildRules: Rules[], market.json's, the world's own), and where in the year it is.
    std::map<std::string, int> rules_;
    double yearDay_ = -1;
    void buildRules();
    int ownRule(const std::string& name) const { return rule(name); }
    // The farms (doc 71; RatwFarms.cpp): built from items::farms() (buildFarms, with the rules).
    std::vector<FarmNow> farms_;
    void buildFarms();
    // The farms' people (RatwFarmPeople.cpp): each farm's real posts counted (FarmNow::postKeepers...), from the
    // positions; and the farms' posts filled from the unplaced NPCs (once a day, tendCareers), with their community's
    // market when its first farm is: "farm" requests the host carries out (arrive).
    std::unordered_map<std::string, std::size_t> farmWorkplace_;   // A farm's work room -> its farm.
    std::map<std::string, int> farmPlaced_;
    void countFarmPosts();
    void staffFarms(const CareerWorld& world, std::vector<CareerNote>& notes);
    std::set<std::string> dead_;                    // Residents who have died (died, revived): nobody feeds or pays them.
    std::vector<Position> positions_;               // From authored_; rebuilt with it (buildPositions).
    std::unordered_map<std::string, std::size_t> positionIndex_;
    mutable std::unordered_map<std::string, std::string> heldBy_, learning_;   // resident -> position (cache).
    mutable bool careersIndexed_ = false;
    std::vector<ResidentRequest> requests_;
    std::map<std::string, int> fills_;
    std::string classOf(const std::string& id, int depth, ClassPass* pass = nullptr) const;
    std::string workOutClass(const std::string& id, int depth, ClassPass* pass) const;   // (classOf's work, uncached.)
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
