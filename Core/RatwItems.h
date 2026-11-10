#pragma once
// The item catalog in the game (Docs/Design/35-items-crafting-industry.md): what of Data/Items the server knows. The
// wearables (Phase 4), every good a shop may sell (its name, kind and price: Docs/Design/39), and the kinds of shop;
// herbs, meals and the sword keep their old hard-coded ways.
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace ratw::items
{
// Where something is worn on a wolf (doc 35, 1.1). The catalog's slot for each is in brackets in the doc.
inline constexpr const char* WearSlots[] = {"head", "neck", "body", "harness", "chest_left", "chest_right", "back", "paws"};
// Where jewellery clips to the fur (doc 35, 1.1); a spot takes any number of pieces.
inline constexpr const char* FurSpots[] = {"ears", "crown", "ruff", "chest", "back", "foreleg_left", "foreleg_right",
                                           "hindleg_left", "hindleg_right", "tail"};
// The most pieces of jewellery a wolf wears at once (all spots together): no limit in the design, this one for safety.
constexpr std::size_t MaxJewellery = 64;

// A weapon's numbers (Data/Items, doc 47): its blow (against health 100), what a swing costs in stamina and how far
// it sets back the next turn's bar, the kind of blow and how much armour it goes through, and its strike chance more.
struct Weapon
{
    std::string kind;                               // "blade" (the bit-swords: the only kind a fight takes yet), or "".
    std::string type = "cut";                       // "cut", "thrust" or "blunt".
    double damage = 0, stamina = 0, weight = 0;
    double knockLoose = 1;                          // How readily a hard blow knocks it from the jaws (1: a sword's).
    int reach = 1, pierce = 0, hit = 0;             // hit: points on the strike chance (in hundredths).
};

struct Item
{
    std::string id, name, category, slot, desc;    // slot: the catalog's ("throat", "sling", "jewelry"...).
    double weight = 0;
    int price = 0;
    std::vector<std::string> spots;                 // Jewellery: where it may go ("any" expanded).
    int status = 0, warmth = 0, rain = 0, jingle = 0;
    // Armour (doc 35, Part 8): what it takes off every blow, more against a kind of blow, and what it takes off
    // dexterity (as a negative). Fractions, for a quality's share (doc 47).
    double protect = 0, vsCut = 0, vsThrust = 0, vsBlunt = 0;
    double spikes = 0;                              // A spiked collar's: what a bite there costs the biter (doc 47).
    int dex = 0;
    int durability = 0;                             // How much use it takes before it falls apart (0: it doesn't wear).
    int nourish = 0;                                // Food: how much it feeds (a meal 50); 0 for anything not eaten.
    double keeps = 0;                               // Food: the days it keeps before it spoils (items.json; 0: it keeps).
    bool drink = false;                             // Drunk rather than eaten (ale, cider): not for hunger.
    std::vector<std::string> source;                // items.json `source`: where it comes from ("field", "hunting", "import"...).
    // Weapons and armour (doc 47): 1 Basic, 2 Professional, 3 Exceptional (0: none). Not a quality: a bronze sword is
    // Basic however well made.
    int tier = 0;
    Weapon weapon;                                  // A weapon's blow (weapon.kind empty for anything else).
    // What it's worth: its place on the worth ladder (items.json `worth`, Data/Economy/ladder.json), in a labourer's
    // day-wages (LD); 0 when it has none. Catalogue data (nothing prices by it since doc 66's Phase 5).
    double worth = 0;
};

struct Business
{
    std::string id, label;
    std::string kind;                               // "shop", "works" or "yard" (industry: a great house's first).
    std::vector<std::string> sells, match;          // match: words in a shopkeeper's work label that mark one.
};

// Loads the catalog once (from RATW_DATA_DIR, the working directory or above it, or the source tree); false, with
// the reason, if it can't. Every lookup loads it first.
bool load(std::string* error = nullptr);
// A wearable in the catalog (a slot other than the mouth's, which stays the sword's), or null.
const Item* wearable(const std::string& id);
// Every wearable, in catalog order.
const std::vector<Item>& wearables();
// The wear slots an item may go in ("chest_left" and "chest_right" for a sling), or none if it isn't worn in one.
std::vector<std::string> slotsFor(const Item& item);
bool wearSlot(const std::string& slot);
bool furSpot(const std::string& spot);
bool spotAllowed(const Item& item, const std::string& spot);
// The kind of shop a shopkeeper keeps, from the words of their work label ("keeping the tailor shop"), or null.
const Business* businessFor(const std::string& workLabel);
// Every good in the catalog, every business, and a business by its id (null if none): for players' notices (doc 54).
const std::vector<Item>& allGoods();
const std::vector<Business>& businesses();
const Business* business(const std::string& id);
// The wearables a business sells, by id or category, in catalog order.
std::vector<std::string> wearablesSold(const Business& business);
// Any item of the catalog (wearable or not), or null.
const Item* good(const std::string& id);
// A blade (doc 47): a bit-sword of any metal and quality ("iron_sword~fine"), or null.
const Item* blade(const std::string& id);
// "Basic", "Professional", "Exceptional" (doc 47), or "" for no tier.
const char* tierName(int tier);
// What a business sells (by id or category) costing at most `maxPrice` pennies, in catalog order.
std::vector<std::string> goodsSold(const Business& business, int maxPrice);
// A starter craft (Data/Items/crafts.json; doc 35, Phase 5, first part): a whole batch from at most two ingredients.
struct Craft
{
    std::string id;
    std::vector<std::string> makers;                // Business ids whose keepers make it.
    double seconds = 60;                            // Game seconds of work for one batch.
    int skill = 0;                                  // The trade skill it needs (Data/Items/recipes.json, by its id).
    std::vector<std::pair<std::string, int>> in, out;
};
// Someone who brings goods in from the land (crafts.json `producers`): a farmer, a shepherd, a fisher, a stables.
struct Producer
{
    std::string id;
    std::vector<std::string> match;                 // Words in a resident's work label.
    double seconds = 1800;                          // Game seconds of work for one yield.
    std::vector<int> seasons;                       // 0 spring .. 3 winter; empty: all year.
    std::vector<std::pair<std::string, int>> out;
    // Out of its seasons, what the work brings in instead (a farm threshing the barn's grain in winter), or nothing.
    std::vector<std::pair<std::string, int>> offSeason;
};
// The producer a resident is, from the words of their work label, or null.
const Producer* producerFor(const std::string& workLabel);
// The worth ladder's settings (Data/Economy/ladder.json): the catalogue's labourer's day, what an item's price was
// written against (an item's place on the ladder, Item::worth, is read with the catalogue).
struct Ladder
{
    double catalogueDay = 12;
};
const Ladder& ladder();
// A post's rule by the words of its title (doc 79, 6.3; RatwCareers.cpp's postRules, once city_loop.json's `wages`): its
// name, its payer ("treasury" or "estates": what kind of post it is, "public" or "estate") and its wage word.
struct WageRule
{
    std::string name;                               // For the reports ("the sea trade", the Country's posts by rule).
    std::string payer;
    std::string wage;                               // Which of the world's wages it pays ("keeper"...); "" for its payer's.
    std::vector<std::string> words;
};
// The economy's rules a world may tune (a world's `rule` records, live.economy.rules; doc 79, 12): each by name, its
// range and its default (Data/Economy/market.json `rules` over it). A rule record of a name not here is skipped (doc 79,
// 14: the old economy's rules, read past).
struct RuleInfo
{
    const char* name;
    int low, high, fallback;
};
inline constexpr RuleInfo Rules[] = {
    // The clamps (doc 79, 6.4): Never Hungry (no short days) and Never Broke (nobody destitute).
    {"neverHungry", 0, 1, 0},
    {"neverBroke", 0, 1, 0},
    {"buyAheadDays", 0, 5, 1},           // Days of food more a household is given the day before a Restday or festival.
    // The Market Board (doc 79, 5 and 12's "pressure settings"): a good's half-life in tenths of a game day (perishable
    // food, keeping food, the rest); the ripple's share a step (percent); the depth's scale (percent); the most one account
    // holder pushes a good's pressure (hundredths); the most shops pay one holder a day (pennies); the take allowance's
    // share of the depth (percent); the shelves' scale (percent); and the faction discount's cap per Chapter a week
    // (pennies).
    {"halfLifePerishable", 5, 300, 20},
    {"halfLifeKeeping", 5, 300, 50},
    {"halfLifeDurable", 5, 300, 80},
    {"ripple", 0, 70, 50},
    {"depthScale", 10, 1000, 100},
    {"playerPushMost", 0, 50, 15},
    {"playerSellMost", 0, 5000, 400},
    {"takeAllowance", 5, 100, 50},
    {"shelfScale", 10, 1000, 100},
    {"factionDiscountMost", 0, 1000, 30},
    // Staged trade (doc 79, 8.1-8.2 and 12): the wagons' frequency (percent of the roads' own), the most wagons on the roads
    // at once and the most farm carts; a bandit camp's most strength and a bounty's most reward (pennies; D14).
    {"caravanRate", 0, 300, 100},
    {"wagonsOnRoad", 0, 64, 16},
    {"cartsOnRoad", 0, 200, 48},
    {"campStrengthMost", 2, 20, 12},
    {"bountyMost", 0, 500, 60},
    // The port (doc 79, 8.3 and 12; Phase 5): the ships a week at a port. 0: none, even with a port. Its own 2 gives the
    // plan's 1-3 by the slider and the season (a week runs one either side of the rule, fewer in winter).
    {"shipsPerWeek", 0, 6, 2},
};
const RuleInfo* ruleInfo(const std::string& name);
// Data/Items as the catalogue reads it (RATW_DATA_DIR, the working directory or above, the source tree): its parent's
// Economy holds market.json (doc 79).
std::filesystem::path dataDirectory();
// The farms and works (Data/Economy/country_farms.json; doc 71, 4 and Phase C; doc 79, 17): each with its crew of posts
// (keepers and hands), by area. Places of work only: their money went with doc 79's Phase 3.
struct FarmDef
{
    std::string id, kind, area;
    int keepers = 1, hands = 0;                     // Its crew's posts.
    std::set<std::string> yields;                   // The goods it brings in (its `yield`'s): its market makes them (doc 79, 7.1).
};
struct FarmArea
{
    std::string id, name;
    bool works = false;                             // A works' area (doc 72: the woods, the peat, the quarries), not a farm's.
    std::vector<FarmDef> farms;
};
struct Farms
{
    std::vector<FarmArea> areas;
};
const Farms& farms();
// The farms' people (Docs/Design/71-the-country-s-industries.md, Phase C): where they live and work, from the map's
// contract (Data/Economy/communities.json: each farm's workplaces, farmhouse and community; each community's homes,
// flats and market), and how they are chosen (Data/Economy/farm_people.json: the old trades, each kind of farm's trades,
// labels and second workplace, the market's posts). Spots are tile centres in their interior. Without either file, none.
struct PlaceRoom
{
    std::string area, name, forName;                // forName: a flat's business (its keeper lives there).
    int capacity = 0;
    std::vector<std::pair<double, double>> spots;   // Beds (a home) or work spots (a workplace).
};
struct FarmPlace
{
    std::string farm, kind, steading, community;    // community: a community's id, or "ridgemere" (the city's farms).
    std::vector<PlaceRoom> workplaces;              // The first is its main work; the second (a mill, a dairy) its `second`.
    PlaceRoom home;                                 // The farmhouse (the keeper's family's).
    // A works of doc 72 (communities.json `works`): no farmhouse (its keeper lives with its hands, in its community or
    // the city), and its workplace out of doors, the cell its centre is in (a post there is the works' by its label).
    bool works = false;
};
struct MarketPlace
{
    std::string area, name, business;               // business: "general", "inn" or "bakery".
    std::vector<std::pair<std::string, int>> posts; // Post (farm_people.json `market`) and how many, in order.
    std::vector<std::pair<double, double>> work;
};
// A country workshop (Data/Economy/workshops.json; doc 72, Phase 5): a business of a community's (`area` the map's fixed
// building id; its keeper's flat `<area>_flat`, or the flat `for` its name), its posts staffed as the community's market's
// are (farm_people.json `market`), owned by `house` (doc 42, Phase 5b: a workshop is a House's; since doc 79 its till is
// a staged account, topped up and trimmed like every staged shop's, with no nightly sweep into an estate). Merged into its
// community's `market` (the map's entry for its building, where there is one, gives the work spots and stations); a
// world without the building has none of it.
struct Workshop
{
    std::string area, community, name, business, house;
    std::vector<std::pair<std::string, int>> posts;
    std::vector<std::pair<double, double>> work;
};
struct CommunityPlace
{
    std::string id, name, region;
    bool town = false;                              // `town`: made a town of its own (World::setupTowns); false, a plain place.
    std::vector<PlaceRoom> homes, flats;
    std::vector<MarketPlace> market;
};
struct FarmKindPeople
{
    std::vector<std::string> trades, words;         // Its old trades, and the words its best hands' labels hold.
    std::string keeper, hand;                       // Labels ({place}).
    std::string second;                             // The second workplace's posts' label ("" none), how many, and words.
    int secondCount = 0;
    std::vector<std::string> secondWords;
};
struct MarketPostPeople
{
    std::string label, role;                        // ({name}.)
    std::vector<std::string> words, trades;
};
struct FarmPeople
{
    bool loaded = false;                            // Both files read.
    int householdMost = 6;
    std::vector<FarmPlace> farms;
    std::vector<CommunityPlace> communities;
    std::vector<Workshop> workshops;
    const Workshop* workshop(const std::string& area) const;   // The workshop at a building (null: none).
    std::vector<std::pair<std::string, std::vector<std::string>>> trades;   // Old trade -> label words, first match.
    std::vector<std::string> fallback;              // Trades any farm takes from once its own are spent.
    std::vector<std::string> suffixes;              // Taken off a steading's name for its {place},
    std::map<std::string, std::string> places;      // or a farm's own {place} (one whose name holds a business's word).
    double farmStart = 6, farmEnd = 17, marketStart = 7, marketEnd = 19;
    std::map<std::string, FarmKindPeople> kinds;
    std::map<std::string, MarketPostPeople> market;
    const FarmPlace* farm(const std::string& id) const;
    const CommunityPlace* community(const std::string& id) const;
    // The regions kept plain places, never towns (the user, 2026-11-03: doc 71, Phase C Progress): each community's not
    // yet a town (`town` false) in a world that has its places (`has`: any of its homes, flats or market interiors), so
    // the old map's towns of the same names stay towns.
    std::set<std::string> plainRegions(const std::function<bool(const std::string&)>& has) const;
    // An unplaced NPC's old trade by its work label ("" none of them).
    std::string tradeOf(const std::string& workLabel) const;
    // A farm's {place}: its own in `places`, else its steading's name less the first suffix it ends with.
    std::string placeName(const FarmPlace& farm) const;
};
const FarmPeople& farmPeople();
// What a kind of shop makes, in file order (none if it makes nothing yet).
std::vector<const Craft*> craftsFor(const std::string& business);
// The ingredients a kind of shop sells to the makers (a stall's flour and milk, a butcher's bones), or none.
const std::vector<std::string>& suppliesFor(const std::string& business);
// What a kind of shop buys from players and sells on (gathered and hunted goods, doc 41), or none.
const std::vector<std::string>& buysFor(const std::string& business);
// Quality (doc 35, Part 4): crude (0, 0.6× the price), common (1, the plain id), fine (2, 1.6×) and masterwork (3, 3×).
// A good of a quality other than common is its id with "~crude", "~fine" or "~masterwork" ("hide~fine"); good() and
// wearable() find those too, with their price, name and wear made over. Herbs, meals, the sword and water have no
// qualities.
// A masterwork carries its maker's mark (doc 35, Part 4): "sword~masterwork@sorrel". good() and wearable() find it as
// the masterwork kind.
std::string baseOf(const std::string& id);                  // "hide~fine" -> "hide".
std::string makerOf(const std::string& id);                 // "sword~masterwork@sorrel" -> "sorrel"; "" for none.
std::string withMaker(const std::string& id, const std::string& maker);
std::string unmarked(const std::string& id);                // The id without its maker's mark.
int qualityOf(const std::string& id);                       // 0..3; 1 for a plain id.
std::string withQuality(const std::string& base, int quality);
const char* qualityName(int quality);                       // "Crude", "Common", "Fine", "Masterwork".
double qualityPrice(int quality);
bool qualityApplies(const std::string& base);
// What a quality's use lasts, against the common kind's: crude 0.6, fine 1.5, masterwork 2.5.
double qualityDurability(int quality);
// A weapon's blow, against the common kind's (doc 33's damage: crude 0.92, fine 1.05, masterwork 1.1), and armour's
// protection (0.75, 1.25, 1.5): within a tier (doc 47), a masterwork about the next tier's blade or kit.
double qualityDamage(int quality);
double qualityGuard(int quality);
// "the ruff", "the left foreleg"...: a fur spot or wear slot for a sentence.
std::string placeName(const std::string& where);
} // namespace ratw::items
