#pragma once
// The item catalog in the game (Docs/Design/35-items-crafting-industry.md): what of Data/Items the server knows. The
// wearables (Phase 4), every good a shop may sell (its name, kind and price: Docs/Design/39), and the kinds of shop;
// herbs, meals and the sword keep their old hard-coded ways.
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

struct Item
{
    std::string id, name, category, slot, desc;    // slot: the catalog's ("throat", "sling", "jewelry"...).
    double weight = 0;
    int price = 0;
    std::vector<std::string> spots;                 // Jewellery: where it may go ("any" expanded).
    int status = 0, warmth = 0, rain = 0, jingle = 0, protect = 0;
    // Armour (doc 35, Part 8): more protection against a kind of blow, and what it takes off dexterity (as a negative).
    int vsCut = 0, vsThrust = 0, vsBlunt = 0, dex = 0;
    int durability = 0;                             // How much use it takes before it falls apart (0: it doesn't wear).
    int nourish = 0;                                // Food: how much it feeds (a meal 50); 0 for anything not eaten.
    bool drink = false;                             // Drunk rather than eaten (ale, cider): not for hunger.
};

struct Business
{
    std::string id, label;
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
// The wearables a business sells, by id or category, in catalog order.
std::vector<std::string> wearablesSold(const Business& business);
// Any item of the catalog (wearable or not), or null.
const Item* good(const std::string& id);
// What a business sells (by id or category) costing at most `maxPrice` pennies, in catalog order.
std::vector<std::string> goodsSold(const Business& business, int maxPrice);
// A starter craft (Data/Items/crafts.json; doc 35, Phase 5, first part): a whole batch from at most two ingredients.
struct Craft
{
    std::string id;
    std::vector<std::string> makers;                // Business ids whose keepers make it.
    double seconds = 60;                            // Game seconds of work for one batch.
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
};
// Whether other trades buy this to work with: an ingredient of some craft, or something a shop supplies; or, a staple,
// something households or the town's own buyers use up every day or so (bread, candles: doc 35, Part 7).
bool traded(const std::string& item);
// The producer a resident is, from the words of their work label, or null.
const Producer* producerFor(const std::string& workLabel);
// What households use up besides food (crafts.json `households`; doc 35, Part 7).
struct HouseholdNeed
{
    std::vector<std::string> any;                   // One of these, whichever a shop has.
    double everyDays = 1, winterDays = 0;           // How often; in winter (0: as ever).
    bool perPerson = false;                         // For each grown wolf in the household.
};
const std::vector<HouseholdNeed>& householdNeeds();
// A town's own buyers (crafts.json `institutions`; doc 35, Part 7): the Town Works, the watch, the church.
struct Institution
{
    std::string id, name;
    bool perGuard = false;                          // Its basket is per guard (else per 100 residents) a day.
    std::vector<std::pair<std::string, double>> basket;
};
const std::vector<Institution>& institutions();
double institutionDays();                           // Days of its basket an institution keeps in stock.
double contractPremium();                           // A contract's reward against the goods' price.
int householdReserve();                             // Pennies a head a household keeps back for food.
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
// A weapon's blow, against the common kind's (doc 33's damage): crude 0.85, fine 1.15, masterwork 1.3.
double qualityDamage(int quality);
// "the ruff", "the left foreleg"...: a fur spot or wear slot for a sentence.
std::string placeName(const std::string& where);
} // namespace ratw::items
