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
// The Country and the city's loop (Data/Items/city_loop.json, Docs/Design/68-the-country-and-the-city-loop.md): the
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
    std::string name;                               // For the reports ("the sea trade", the Country's posts by rule).
    std::string payer;
    std::string wage;                               // Which of the world's wages it pays ("keeper"...); "" for its payer's.
    std::vector<std::string> words;
};
// The Houses' estates (city_loop.json `estates`; Docs/Design/72-pulling-the-country-into-the-world.md, Phase 6): each
// House's own purse, which collects its lands' rents (the works', the workshops' and the food farms', by their `house`)
// and pays its own people, the Country's posts it holds (matched by `words`: first against a post's label, then against
// its workplace's cell). `fund` "country": the Country's own purse (Grayrock's: it runs the wharf, so the sea trade's
// takings, its basket and the arrivals stay in it); else "estate:<id>". `grants`: the Crown's, which keeps the posts no
// other estate's words match and covers the others' posts when they are short (a grant, the Crown's levy on the land).
struct Estate
{
    std::string id, house, name, fund;
    bool grants = false;
    std::vector<std::string> words;
};
// The loop's rules a world may tune (city_loop.json `rules`, a world's `rule` records; doc 68, Progress, "First fixes"):
// each by name, its range and the value used where city_loop.json has none.
struct RuleInfo
{
    const char* name;
    int low, high, fallback;
};
inline constexpr RuleInfo Rules[] = {
    {"comfortsMost", 0, 100000, 200},    // The most comforts a household buys a day.
    {"almsADay", 0, 1000, 10},           // Pennies a head the alms make a household up to.
    {"almsNoEarner", 0, 300, 100},       // A household with no earner (no member holding a paid post: the old, orphans,
                                         // the out of work living alone) is made up to this percent of the alms a head,
                                         // the part over the day's of what the treasury holds over its cap (doc 73, round
                                         // 4: stable8c's short households, 68% with no earner and 63% alone, held 10p a
                                         // head, a day's food and none over); round 6: so is one whose earners' wages
                                         // together are under the day's alms a head, after the no-earner's and keeping
                                         // tonight's wage support wanted back too; 100 or less: as any household. (Its low 0,
                                         // as map_editor's RULE_LIMITS and Atlas take every rule from 0: a world holding
                                         // less than a rule's low fails to load, Roster's "Invalid rule record".)
    {"treasuryCap", 0, 100000000, 2000}, // What the treasury holds over this pays the short tills' wages,
    {"wageSupport", 0, 100, 100},        // this percent of each till's shortfall.
    {"workBankHours", 0, 1000, 0},       // A crew's banked hours, in hours of the crew; 0: its longest craft and an hour.
    {"fairFood", 0, 1, 1},               // Food in a household: the least fed first, and only what someone can pay for.
    {"services", 0, 1, 1},               // Households buy the city's services from their comforts budget.
    {"servicesCityShare", 0, 100, 30},   // The percent of a service's price that goes to the treasury.
    // Trades (doc 68, Progress, "Trades: medicine, patrons, office heads, and the follow-ups"):
    {"suppliers", 0, 1, 1},              // A keeper's craft inputs come from the city's makers first (`suppliers`).
    {"patrons", 0, 1, 1},                // The city, the Houses and the wharf pay for their kit, boats and halls (`patrons`).
    {"armourEvery", 1, 3650, 120},       // Days between new pieces of kit, for each armed wolf.
    {"buyAheadDays", 0, 5, 1},           // Days of food more a household buys the day before a Restday or festival.
    {"floatDrawDays", 1, 28, 7},         // A till's working float: its mean draws a day over this many days (1: yesterday's).
    {"floatWagesDays", 0, 60, 7},        // Days of its posts' wages a till keeps, over its float, before its keeper's profit.
    {"workshopWagesDays", 0, 60, 4},     // A country workshop's, before its House's share (doc 72, after country5: the
                                         // works' floatDays, 2; a House's till needs no week of wages in hand; 3 after
                                         // country5b: with 2, Larkfield's smithy, its float a day's iron bars, was
                                         // short on its Restdays; 4 after country6a: with 3, still on some, 98.0%).
    {"workshopWageMost", 0, 100, 2},     // The most pennies a country workshop's wages drift over the world's (doc 72, after
                                         // country6a: its House takes the profit, but the drift's step, judged on that
                                         // share, raised its crews' wages ~130p a day over their base and left the House
                                         // little; 2: a keeper 16p, a hand 10p at most, the works' fixed wages). Down as
                                         // any till's (wageDriftMost, wageDriftHandMost).
    {"subsidyToNeed", 0, 1, 1},          // The carriage subsidy only while the carters lack their crew's wages tonight.
    {"estatePostsDays", 1, 30, 2},       // Nights of its posts' wages an estate keeps before its basket and patrons (doc 72;
                                         // countryPostsDays before doc 73, read as this).
    {"estates", 0, 1, 1},                // Each House its own estate purse (`estates`, doc 72, Phase 6); 0: the Wharf's one.
    // The Wharf (Docs/Design/73-the-wharf.md, 6; city_loop.json `wharf`): the realm's valve to the world beyond it, its
    // imports and takes at the stable price, the beyond's trade credit and the DM's moves of it.
    {"wharfValve", 0, 1, 1},             // 0: the Wharf is the Country renamed (no beyond, no takes, no export basket).
    {"wharfFee", 0, 50, 10},             // The Wharf's percent (wharfage): of its imports' takings, and over its exports'.
    {"wharfKeepDays", 1, 365, 28},       // Days of a farm's yield of a good it keeps before the Wharf takes the rest,
    {"wharfKeepLeast", 0, 10000, 20},    // and pieces it always keeps.
    {"wharfImportScale", 0, 1000, 100},  // Percent on the imports' `most` a day (0: no imports of the lists' goods),
    {"wharfTakeScale", 0, 1000, 100},    // and on the takes' (0: no takes).
    {"wharfExportMost", 0, 100000, 300}, // Pennies a day the beyond pays in at most (takes, the export basket, carriage, fee).
    {"wharfCreditMost", 0, 1000000, 2000},    // The most trade credit held (what the realm has paid beyond and not had back).
    {"wharfInjectADay", 0, 1000, 0},     // The standing levers: pennies a day added to the credit at 18:00 (money in, as
    {"wharfTakeADay", 0, 1000, 0},       // goods bought), and taken from it (money out, as the beyond buys less).
    {"wharfActionMost", 0, 1000000, 2000},    // The most one DM action (wharf.credit) moves the credit (0: refused).
    // The food farms' land rent (doc 72, open question 2): percent of a farm's takings to its House's estate each night,
    // from its basket's share (after its wages, never its float); an area's own landRent<Area> over it (sized on country3,
    // year 3: about half its thinnest farm's margin in its leanest season, so each keeps half its basket at worst).
    {"landRent", 0, 100, 16},            // Every food area's without its own: the honey's, the meat's (margins ~32%).
    {"landRentFish", 0, 100, 9},         // The fish's (its sea boats' autumn margin ~18% of their takings).
    {"landRentFruit", 0, 100, 4},        // The orchards' and vines' (the vineyards' winter margin ~9%).
    {"landRentGrain", 0, 100, 28},       // The grain's (~57%).
    {"landRentSalt", 0, 100, 8},         // The salt's (its autumn margin ~17%).
    // A farm's own over its area's (country_farms.json, a farm's `landRent`: landRent<Its>; doc 72, Phase 4's payer).
    {"landRentDairy", 0, 100, 32},       // The cattle and dairy farms' (their margin 68-72% after wages in country5's year
                                         // 3, the pigs' 45%; it was 24, on country3c's 67-70%).
    {"landRentThin", 0, 100, 0},         // The thinnest farms' (doc 72, Phase 5: the lake boats, the salt works, three
                                         // orchards, 5-8p a day over their wages and rent in country4's year 3).
    {"farmSurplus", 0, 1, 1},            // A food farm's basket at most its design's (country_farms.json basketADay), what
                                         // its till holds over its float and that each night its House's (after wharf7b,
                                         // doc 73: the dairies', the grain farms' and two orchards' baskets ~237p a day
                                         // over their design while the estates went short); 0: the basket all over it.
    {"shopRot", 0, 1, 1},                // A maker's perishable by-product (made, used by none of its crafts, not food,
                                         // items.json keeps > 0: the butchers' hides) held over its keep loses a keeps-th
                                         // of what is over each night (doc 73, round 2: ~19 hides a day piling, 18,835
                                         // at the butchers by stable8's year 3); 0: it keeps.
    // The luxuries (city_loop.json `luxuries`; doc 72, Phase 5, the user's decision: cheese, a comic luxury for wolves):
    // never bought as food; every household wants them, but buys them only from what it holds over its comforts' keep
    // after its food and its needs (the fuel among them), at most this percent of that a day,
    {"luxuryShare", 0, 100, 10},
    {"luxuryMost", 0, 100, 1},           // and at most this many pieces a head a day;
    {"luxuryAbove", 0, 10000, 200},      // a household not of the Houses (by its class) only over this many pennies a head
                                         // (the Houses' households over the comforts' keep: after country5, the rich and
                                         // the noble its buyers, not the middle class);
    {"luxuryHouses", 0, 100, 40},        // the Houses' own tables: this percent a day of what the Country's purse holds
    {"luxuryHousesMost", 0, 1000, 24},   // over its reserve and its posts' nights, and at most this many pieces a day;
    {"luxuryPriceBand", 0, 1000, 700},   // a shop's price for one may climb this percent over the season's (its own band,
    {"luxuryPriceStep", 0, 1000, 50},    // this many thousandths a day when it sells out; outside the city's price level).
    // The treasury's surplus (doc 68, Progress, "The treasury's surplus: works and alms"): what it holds over its cap and
    // surplusKeep at 18:00, after its posts, the short tills' wages and the alms, goes back out each day:
    {"surplus", 0, 1, 1},                // On: the surplus to public works orders and the poorest households.
    {"surplusKeep", 0, 100000000, 4000}, // Pennies over the cap kept for the lean seasons (the summer's and autumn's fall).
    {"surplusWorks", 0, 100, 20},        // Percent of the surplus to public works orders (`works`), till to till;
    {"surplusAlms", 0, 100, 80},         // percent to the poorest households, per head, poorest first,
    {"surplusAlmsTo", 0, 1000, 40},      // up to this many pennies a head.
    // The self-correcting rules (doc 68, Progress, "Self-correcting rules"): values that drift, gently and within bounds,
    // so that the economy rebalances itself. Each has its switch; what each has drifted to is saved with the society.
    {"stockPrices", 0, 1, 1},            // 1. A shop's price for a ware follows its stock: up when it sells out, down on a glut,
    {"stockPriceBand", 0, 90, 25},       // within this percent of the catalogue's (the season's) price,
    {"stockPriceStep", 0, 1000, 10},     // by this many thousandths a day (a quarter of it back toward 1 on a quiet day).
    {"shelfFollow", 0, 1, 1},            // 2. A shelf follows its ware's sales, smoothed (off: a quarter over yesterday's),
    {"shelfDays", 1, 365, 7},            // over about this many days,
    {"shelfCover", 25, 1000, 125},       // this percent of the smoothed sales a day on the shelf,
    {"shelfGrow", 0, 500, 25},           // and a day it sells out counted as this percent more than its shelf.
    {"wageDrift", 0, 1, 1},              // 3. A till's wages follow its cover: a penny a post every so many days,
    {"wageDriftDays", 1, 365, 28},       // judged over this many days:
    {"wageDriftSpare", 0, 1000, 50},     // up when it paid every wage and its keeper's profit was this percent of them;
    {"wageDriftMost", 0, 100, 12},       // down when short on most days; never more than this many pennies either way on
    {"wageDriftHandMost", 0, 100, 12},   // a keeper's wage, nor more than this many on a hand's,
    {"wageDriftFloor", 0, 1000, 7},      // nor a post lowered under this (a day's food).
    {"treasuryDrift", 0, 1, 1},          // 4. The gate toll and the alms follow the treasury's level:
    {"treasuryTarget", 0, 100000000, 5500},  // its slow mean against this (5,500p, about where it settles: doc 68, Progress,
                                         // "Births follow deaths; arrivals from the Country; the treasury's target"),
    {"treasuryBand", 0, 100, 15},        // outside this percent of it (inside: no change; the season's swing),
    {"treasuryDays", 1, 3650, 182},      // the mean over about this many days (half a year);
    {"treasuryJudgeDays", 1, 365, 28},   // judged every this many days (28, from a fortnight);
    {"tollStep", 0, 100, 5},             // the toll moves by this many tenths of a point at each judging,
    {"tollDriftMost", 0, 90, 2},         // at most this many points from the world's gate toll (2 since doc 73's round 9; 5);
    {"almsDriftMost", 0, 1000, 2},       // the alms by a penny a head at each, up to this many over almsADay (never under).
};
const RuleInfo* ruleInfo(const std::string& name);
// A rule's name now for one it had before (doc 73: countryPostsDays is estatePostsDays), else the name as given: a world's
// rule records, the data file and the tools read the old name as the new.
std::string ruleName(const std::string& name);
// A city service (city_loop.json `services.list`): a catalogue item a business does for a household, bought from its
// comforts budget about once every `every` days; done at once (nothing is kept). `for: "patrons"`: a patron's (the
// armoury's mending, a hull caulked, a public building's upkeep; doc 68, "Trades"), done for each unit of its business's
// patrons (an armed wolf, a hull, a building) about once every `every` days and paid by the patron, never a household.
struct ServiceDef
{
    std::string id, business;
    int every = 30;
    bool patron = false;                            // A patron's: for each of its patrons' units, never a household's.
};
// A city maker that supplies another business's inputs first (city_loop.json `suppliers`; doc 68, Progress, "Trades"):
// what a `buyer`'s keeper draws for its crafts it buys from a `seller` of the city holding it, at the seller's price
// (till to till), and from the store room (the Country) only what the city's sellers don't have. `goods`: the goods it
// covers (empty: every input the seller sells).
// `price` (doc 72, Phase 5: the resale goods): the percent of the catalogue's (the season's) price the seller is paid; 0,
// its path's own: a craft input at the seller's price (100), a good the buyer sells on at the store room's (resaleMarkup),
// so that a reseller's costs never rise for buying from the city's or the country's makers rather than the store room.
struct Supplier
{
    std::string buyer, seller;
    std::vector<std::string> goods;
    int price = 0;
};
// A trade's patrons (city_loop.json `patrons.list`; doc 68, "Trades"): who pays for its patron services (and, for the
// armoury, new kit). Each group's units are the posts of `roles` (the watch: "guard"), else `units` (the Houses'
// retainers, the wharf's hulls, the public buildings: not placed, a number); `payer` is "treasury" or a market's id (the
// Houses' estates and the wharf: the Country).
struct PatronGroup
{
    std::string id, name, payer;
    std::vector<std::string> roles;
    int units = 0;
};
struct Patrons
{
    std::string business;                           // The trade that does the work ("armory", "shipwright", ...).
    std::vector<PatronGroup> groups;
    std::vector<std::string> pieces;                // New kit a unit buys now and then (armourEvery), of what the shop holds.
};
// The city's public works (city_loop.json `works.list`; doc 68, Progress, "The treasury's surplus: works and alms"): a
// trade the treasury orders from with its surplus, for the upkeep of the public buildings and services, its share of
// the orders (`weight`) and what it orders (`goods`, at the catalogue's price): a service (items.json category service)
// done at once, else a good from the shop's shelves, used up in the works.
struct WorksOrder
{
    std::string business;
    std::vector<std::string> goods;
    int weight = 1;
};
// The seasons in the Country's supply (city_loop.json's `seasons`; doc 68, Progress, "Seasons and the DM's rules"): a
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
    // The Country's own stores of the group's goods (doc 68, Progress, "The follow-ups"): a maker that can't make one for
    // the season's lack of an input draws the good itself, where it sells it on too (the preservers' winter fruit).
    bool stores = false;
};
// The Wharf's lists (city_loop.json `wharf`; Docs/Design/73-the-wharf.md, 4.2): the goods it imports when the realm's
// farms can't fill a draw (`imports`) and the goods it takes when they pile on the farms (`takes`), each on or off and its
// most a day realm-wide (times the rules wharfImportScale and wharfTakeScale); `never`, goods it neither lands nor takes
// whatever the lists or a world say (cheese, the user's). A good on both lists, or listed and on `never`, is dropped
// from both at load (`refused`, for the catalogue's report): the Wharf never routes a good both ways.
struct WharfGood
{
    int most = 0;
    bool on = true;
    // A take's (doc 73, after wharf7b): its place in the list (the takes run in the file's order, not the alphabet's), the
    // city's trades (businesses.json ids) whose shops it is taken from too, beside the farms and works (`shops`: the
    // butchers' hides and bones, the byproduct of their carcasses that no draw takes), and the pieces such a shop keeps
    // (`keep`; -1: wharfKeepLeast, or wharfKeepDays of what it sells a day, or its shelf, the most of them).
    int rank = 0;
    int keep = -1;
    std::set<std::string> shops;
};
struct WharfLists
{
    std::map<std::string, WharfGood> imports, takes;
    std::set<std::string> never;
    std::vector<std::string> refused;
    bool refuses(const std::string& good) const { return never.count(good) > 0; }
};
struct CityLoop
{
    std::vector<Market> markets;
    WharfLists wharf;                               // The Wharf's imports and takes (doc 73).
    std::vector<SeasonGroup> seasons;
    std::vector<WageRule> wages;
    std::vector<Estate> estates;                    // The Houses' estates (doc 72, Phase 6); the granting one the Crown's.
    double foodDays = 2, shortDays = 1;             // A household tops each member up to foodDays; under shortDays it tries again.
    int comfortsAbove = 60;                         // Pennies a head held before a household buys comforts,
    double comfortsShare = .1;                      // and the share of what is over it spent a day,
    std::vector<std::string> comforts;              // on these (used up at once).
    std::vector<std::string> luxuries;              // Never food: bought from the luxuries' budget (the rules luxury*).
    std::map<std::string, int> rules;               // Every rule (Rules) by name, as city_loop.json gives it.
    std::vector<ServiceDef> services;
    std::vector<Supplier> suppliers;
    // The Houses' city businesses (city_loop.json `houseBusinesses`; doc 72, after iron6): a till whose post works at one of
    // these buildings is its House's, as a country workshop is: what it holds each night over its float and
    // workshopWagesDays of its wages goes to that House's estate, not its keeper, and its wages drift at most
    // workshopWageMost. Building (an area id) -> House.
    std::map<std::string, std::string> houseTills;
    std::vector<Patrons> patrons;
    std::vector<WorksOrder> works;
    std::map<std::string, int> shelves;             // A dear good's shelf (theriac: 1), in place of GoodsKept.
};
const CityLoop& cityLoop();
// The Country's food farms (Data/Economy/country_farms.json; Docs/Design/71-the-country-s-industries.md, 4): modelled
// producers with virtual crews that take an area of the Country's food over when the area is switched on. These are the
// defaults a world's own settings change (EconomySpec::farms): a farm's own setting first, then its area's, then these.
struct FarmDef
{
    std::string id, kind, area;
    std::string house;                              // The noble House whose land it is on ("": its area's), for its rent.
    std::string landRent;                           // Its own land rent rule's name, landRent<This> ("": its area's).
    int keepers = 1, hands = 0;                     // Its crew (virtual posts: nobody holds them yet).
    int keeper = -1, hand = -1;                     // Its wages a day (-1: its area's, then the defaults').
    std::int64_t reserve = 0, purse = 0;            // Its till's float and its crew's settled purse, made when it is founded.
    double basketADay = 0;                          // The design's basket a day (for reports; the basket is what is over).
    // Pieces a day of each good at each season's middle day (Spring, Summer, Autumn, Winter), blended as the seasons are
    // (Society::seasonal): what it grows, and what the Country's own people eat of it there (the Country's table).
    std::map<std::string, std::array<double, 4>> yield, table;
    // What it makes of a yield it can't sell before it spoils (doc 72, the user's decision after country3b: the dairies'
    // spare milk into butter and cheese): each morning, what of `from` is past keeping goes to these in order, `share`
    // percent of it to each (the last all that is left), `in` pieces of it a piece of `good`, while its store holds under
    // `store` pieces of `good`; what none takes spoils as before. `aDay` (doc 72, Phase 4's follow-up: the shops wanted
    // far more butter and cheese than the spare made): pieces of `from` a day at each season's middle, blended, milked for
    // this making alone and never offered to the shops (the cows kept for the cheese room), made each morning while its
    // store has room (what it can't take that morning isn't milked for it), journalled "made at <farm> of its own
    // <from>"; all zero: none, only the spare.
    struct Make
    {
        std::string good, from;
        int in = 1, share = 100, store = 0;
        std::array<double, 4> aDay{};
        bool dedicated() const { return aDay[0] > 0 || aDay[1] > 0 || aDay[2] > 0 || aDay[3] > 0; }
    };
    std::vector<Make> makes;
};
struct FarmArea
{
    std::string id, name;
    std::vector<std::string> goods;                 // On, these are its farms' only (the Country no longer sends them).
    int on = -1;                                    // -1: as the schedule says; 0: off; 1: on.
    int yieldScale = 100, crewScale = 100;          // Percent on its farms' yields, and on their hands (rounded half up).
    int keeper = -1, hand = -1;                     // Its farms' wages (-1: the defaults').
    int priceFactor = 0;                            // The farms' price a year, percent of the catalogue's; 0: the season's.
    double takings = 0;                             // The design's takings a day (the year's mean): the Country's basket's blend.
    // A works' area (Docs/Design/72-pulling-the-country-into-the-world.md, Phase 0: the woods, the peat, later the
    // quarries), not a food farm's: its works pay their crews and send what is over their floats to the House whose
    // land they are on each night (the estates' rent), and buy no basket; `house` its works' House ("" none: a farm's).
    bool works = false;
    std::string house;
    std::vector<FarmDef> farms;
};
struct Farms
{
    int keeper = 16, hand = 10;                     // The farms' wages a day (fixed: the basket takes what is over them).
    int floatDays = 2, openingStockDays = 7;
    // The Country's own people whose food the farms' `table` is (doc 71: the Houses' estates' and the other industries'
    // workers, still in the Country): a works' crew, once its area is on, lives in the realm and buys its food in the
    // shops, so it no longer eats at the table (doc 72, Phase 3; Society::tableShare).
    int estatePeople = 24, otherWorkers = 68;
    int start = -1, every = 28;                    // The switch: the first area on day `start` (-1: none), one every `every`
    std::vector<std::string> order;                 // days after it, in this order.
    std::vector<std::pair<std::string, int>> basket;          // The farms' basket of the city's goods, and shares.
    std::vector<std::pair<std::string, int>> wharfBasket;   // The Country's basket once the farms have the food.
    std::vector<std::string> switchTrades;          // The trades whose orders the switch takes (for reports).
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
// are (farm_people.json `market`), owned by `house`, whose estate takes what its till holds over its float and its wages
// each night (doc 42, Phase 5b: a workshop is a House's; into the Country's purse, the Houses' estates). Merged into its
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
