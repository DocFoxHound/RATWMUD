#pragma once
// The item catalog in the game (Docs/Design/35-items-crafting-industry.md): what of Data/Items the server knows. The
// wearables (Phase 4), every good a shop may sell (its name, kind and price: Docs/Design/39), and the kinds of shop;
// herbs, meals and the sword keep their old hard-coded ways.
#include <array>
#include <cstddef>
#include <map>
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
    // Hours of residents' work for one batch (Docs/Design/64-economy-migration.md, Phase 3; calibrated from the worth
    // ladder by tools/econ_model/apply_ladder.py). Without it, `seconds`' worth of hours.
    double hours = 60. / 3600;
    int skill = 0;                                  // The trade skill it needs (Data/Items/recipes.json, by its id).
    std::vector<std::pair<std::string, int>> in, out;
};
// Someone who brings goods in from the land (crafts.json `producers`): a farmer, a shepherd, a fisher, a stables.
struct Producer
{
    std::string id;
    std::vector<std::string> match;                 // Words in a resident's work label.
    double seconds = 1800;                          // Game seconds of work for one yield.
    double hours = .5;                              // Hours of residents' work for one yield (as Craft::hours).
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
// A household's need besides food (crafts.json `households`, doc 35, Part 7): one of `any`, every `everyDays` (in winter
// every `winterDays` where that is more than 0), once a home, or once a grown head (`perPerson`).
struct HouseholdNeed
{
    std::vector<std::string> any;
    double everyDays = 1, winterDays = 0;
    bool perPerson = false;
};
struct Households
{
    int reserve = 6;                                // Pennies a head a household keeps back for food before its needs.
    std::vector<HouseholdNeed> needs;
};
const Households& households();
// The Country and the city's loop (Data/Items/country.json, Docs/Design/68-the-country-and-the-city-loop.md): the
// markets outside the city (one, the Country, for now), each with its purse ("outside:<id>") and its basket (shares of
// its day's spending, by good); who pays a post that is neither a shop's nor the watch's (the first rule with a word in
// the post's title, lower case: a market's id or "treasury"); and the residents' buying.
struct Market
{
    std::string id, name;
    std::vector<std::pair<std::string, int>> basket;
};
struct WageRule
{
    std::string payer;
    std::string wage;                               // Which of the world's wages it pays ("keeper"...); "" for its payer's.
    std::vector<std::string> words;
};
// The loop's rules a world may tune (country.json `rules`, a world's `rule` records; doc 68, Progress, "First fixes"):
// each by name, its range and the value used where country.json has none.
struct RuleInfo
{
    const char* name;
    int low, high, fallback;
};
inline constexpr RuleInfo Rules[] = {
    {"comfortsMost", 0, 100000, 200},    // The most comforts a household buys a day.
    {"almsADay", 0, 1000, 10},           // Pennies a head the alms make a household up to.
    {"treasuryCap", 0, 100000000, 2000}, // What the treasury holds over this pays the short tills' wages,
    {"wageSupport", 0, 100, 100},        // this percent of each till's shortfall.
    {"workBankHours", 0, 1000, 0},       // A crew's banked hours, in hours of the crew; 0: its longest craft and an hour.
    {"fairFood", 0, 1, 1},               // Food in a household: the least fed first, and only what someone can pay for.
    {"services", 0, 1, 1},               // Households buy the city's services from their comforts budget.
    {"servicesCityShare", 0, 100, 30},   // The percent of a service's price that goes to the treasury.
};
const RuleInfo* ruleInfo(const std::string& name);
// A city service (country.json `services.list`): a catalogue item a business does for a household, bought from its
// comforts budget about once every `every` days; done at once (nothing is kept).
struct ServiceDef
{
    std::string id, business;
    int every = 30;
};
// The seasons in the Country's supply (country.json's `seasons`; doc 68, Progress, "Seasons and the DM's rules"): a
// group of goods (grain and its bread, fresh fruit, fresh fish, ...) with, by season (Spring, Summer, Autumn, Winter, each
// the value at the season's middle day, days between blended in a straight line), in percent of normal: how much the
// Country can supply a day (`supply`, of `normal` pieces a day for the whole group; 0 pieces: no cap), what its goods
// cost (`price`: store draws and the shops' prices to residents), and how full the shops keep their shelves of them
// (`shelf`: a maker stocking up ahead of a lean season). A good is in one group at most (the first that names it).
struct SeasonGroup
{
    std::string id, name;
    std::vector<std::string> goods;
    std::array<int, 4> supply{100, 100, 100, 100}, price{100, 100, 100, 100}, shelf{100, 100, 100, 100};
    int normal = 0;
};
struct Country
{
    std::vector<Market> markets;
    std::vector<SeasonGroup> seasons;
    std::vector<WageRule> wages;
    double foodDays = 2, shortDays = 1;             // A household tops each member up to foodDays; under shortDays it tries again.
    int comfortsAbove = 60;                         // Pennies a head held before a household buys comforts,
    double comfortsShare = .1;                      // and the share of what is over it spent a day,
    std::vector<std::string> comforts;              // on these (used up at once).
    std::map<std::string, int> rules;               // Every rule (Rules) by name, as country.json gives it.
    std::vector<ServiceDef> services;
};
const Country& country();
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
std::vector<std::string> kindsOf(const std::string& base);  // The good in every quality, common first (marks aside).
// What a quality's use lasts, against the common kind's: crude 0.6, fine 1.5, masterwork 2.5.
double qualityDurability(int quality);
// A weapon's blow, against the common kind's (doc 33's damage: crude 0.92, fine 1.05, masterwork 1.1), and armour's
// protection (0.75, 1.25, 1.5): within a tier (doc 47), a masterwork about the next tier's blade or kit.
double qualityDamage(int quality);
double qualityGuard(int quality);
// "the ruff", "the left foreleg"...: a fur spot or wear slot for a sentence.
std::string placeName(const std::string& where);
} // namespace ratw::items
