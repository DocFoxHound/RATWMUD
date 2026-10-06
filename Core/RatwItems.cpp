#include "RatwItems.h"

#include "RatwJsonDoc.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <unordered_map>

namespace ratw::items
{
namespace
{
struct Catalog
{
    bool loaded = false;
    std::string error;
    std::vector<Item> wearables;
    std::vector<Item> goods;                        // Every item, wearable or not, with its name, kind and price.
    std::vector<Business> businesses;
    std::vector<Craft> crafts;
    // Quality (doc 35, Part 4): every good but the old three has a crude, a fine and a masterwork kind beside its common
    // one ("hide~fine"), with its price, name and wear made over. Kept apart from the lists above (which stay the
    // common kinds), found by id through the indexes.
    std::vector<Item> goodVariants, wearVariants;
    std::unordered_map<std::string, const Item*> goodIndex, wearIndex;
    std::vector<Producer> producers;
    std::vector<HouseholdNeed> needs;
    std::vector<Institution> institutions;
    std::vector<ToolNeed> tools;
    std::map<std::string, std::vector<std::pair<std::string, double>>> upkeep;
    double institutionDays = 3, premium = 1.3;
    int reserve = 6;
    std::map<std::string, std::vector<std::string>> supplies, buys;
};

bool readJson(const std::filesystem::path& path, json::Value& out, std::string& error)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        error = "cannot read " + path.string();
        return false;
    }
    std::stringstream text;
    text << in.rdbuf();
    if (!json::parse(text.str(), out, error))
    {
        error = path.string() + ": " + error;
        return false;
    }
    return true;
}

// Data/Items: RATW_DATA_DIR (the Data directory), else the working directory or one above it, else the source tree.
std::filesystem::path itemsDir()
{
    namespace fs = std::filesystem;
    std::error_code ec;
    if (const char* dir = std::getenv("RATW_DATA_DIR"); dir && *dir)
        return fs::path(dir) / "Items";
    for (auto at = fs::current_path(ec); !ec && !at.empty(); at = at.parent_path())
    {
        if (fs::exists(at / "Data" / "Items" / "items.json", ec))
            return at / "Data" / "Items";
        if (at == at.parent_path())
            break;
    }
#ifdef RATW_SOURCE_DIR
    return fs::path(RATW_SOURCE_DIR) / "Data" / "Items";
#else
    return fs::path("Data") / "Items";
#endif
}

int wearNumber(const json::Value& item, const char* key)
{
    return int(item.object("wear").number(key, 0));
}

Catalog build()
{
    Catalog c;
    json::Value items, businesses;
    const auto dir = itemsDir();
    if (!readJson(dir / "items.json", items, c.error) || !readJson(dir / "businesses.json", businesses, c.error))
        return c;
    for (const auto& i : items.array("items"))
    {
        Item good;
        good.id = i.string("id");
        good.name = i.string("name");
        good.category = i.string("category");
        good.slot = i.string("slot");
        good.desc = i.string("desc");
        good.weight = i.number("weight");
        good.price = int(i.number("price"));
        good.durability = int(i.number("durability", 0));
        good.nourish = int(i.object("food").number("nourish", 0));
        good.drink = i.object("food").number("drink", 0) > 0;
        good.keeps = std::max(0.0, i.number("keeps", 0));
        if (!good.id.empty() && !good.name.empty() && good.price >= 0 && good.price <= 100000)
            c.goods.push_back(std::move(good));
        const auto slot = i.string("slot");
        if (slot.empty() || slot == "mouth" || slot == "loop")       // (The mouth stays the sword's; loops come later.)
            continue;
        Item item;
        item.id = i.string("id");
        item.name = i.string("name");
        item.category = i.string("category");
        item.slot = slot;
        item.desc = i.string("desc");
        item.weight = i.number("weight");
        item.price = int(i.number("price"));
        for (const auto& s : i.array("spots"))
        {
            if (s.asString({}) == "any")
                item.spots.assign(std::begin(FurSpots), std::end(FurSpots));
            else if (furSpot(s.asString({})) && std::find(item.spots.begin(), item.spots.end(), s.asString({})) == item.spots.end())
                item.spots.push_back(s.asString({}));
        }
        item.status = wearNumber(i, "status");
        item.warmth = wearNumber(i, "warmth");
        item.rain = wearNumber(i, "rain");
        item.jingle = wearNumber(i, "jingle");
        item.protect = int(i.object("armor").number("protect", 0));
        item.durability = int(i.number("durability", 0));
        item.vsCut = int(i.object("armor").object("vs").number("cut", 0));
        item.vsThrust = int(i.object("armor").object("vs").number("thrust", 0));
        item.vsBlunt = int(i.object("armor").object("vs").number("blunt", 0));
        item.dex = int(i.object("armor").number("dex", 0));
        if (item.id.empty() || item.name.empty() || item.price < 1 || item.price > 1000 || (slot == "jewelry" && item.spots.empty()))
            continue;
        c.wearables.push_back(std::move(item));
    }
    for (const auto& b : businesses.array("businesses"))
    {
        Business business;
        business.id = b.string("id");
        business.label = b.string("label");
        business.kind = b.string("kind");
        for (const auto& s : b.array("sells"))
            business.sells.push_back(s.asString({}));
        for (const auto& m : b.array("match"))
            business.match.push_back(m.asString({}));
        if (!business.match.empty())
            c.businesses.push_back(std::move(business));
    }
    // The starter crafts: optional (a catalog without them makes nothing), but a craft naming an unknown good is left out.
    json::Value crafts;
    std::string craftError;
    if (readJson(dir / "crafts.json", crafts, craftError))
    {
        const auto known = [&](const std::string& id) {
            return std::any_of(c.goods.begin(), c.goods.end(), [&](const Item& g) { return g.id == id; });
        };
        const auto counts = [&](const json::Value& o, std::vector<std::pair<std::string, int>>& out) {
            for (const auto& [id, n] : o.fields())
            {
                const double count = n.asNumber(0);
                if (!known(id) || count < 1 || count > 99 || count != std::floor(count))
                    return false;
                out.push_back({id, int(count)});
            }
            return !out.empty();
        };
        for (const auto& k : crafts.array("crafts"))
        {
            Craft craft;
            craft.id = k.string("id");
            craft.seconds = k.number("seconds", 60);
            for (const auto& m : k.array("makers"))
                craft.makers.push_back(m.asString({}));
            if (!craft.id.empty() && !craft.makers.empty() && craft.seconds > 0 && craft.seconds <= 86400 &&
                counts(k.object("in"), craft.in) && craft.in.size() <= 2 && counts(k.object("out"), craft.out))
                c.crafts.push_back(std::move(craft));
        }
        for (const auto& k : crafts.array("producers"))
        {
            Producer producer;
            producer.id = k.string("id");
            producer.seconds = k.number("seconds", 1800);
            for (const auto& m : k.array("match"))
                if (!m.asString({}).empty())
                    producer.match.push_back(m.asString({}));
            for (const auto& season : k.array("seasons"))
                producer.seasons.push_back(int(season.asNumber(-1)));
            if (k.has("offSeason"))
                counts(k.object("offSeason"), producer.offSeason);
            if (!producer.id.empty() && !producer.match.empty() && producer.seconds > 0 && producer.seconds <= 86400 &&
                counts(k.object("out"), producer.out) &&
                std::all_of(producer.seasons.begin(), producer.seasons.end(), [](int s) { return s >= 0 && s <= 3; }))
                c.producers.push_back(std::move(producer));
        }
        const auto& households = crafts.object("households");
        c.reserve = std::max(0, int(households.number("reserve", 6)));
        for (const auto& n : households.array("needs"))
        {
            HouseholdNeed need;
            for (const auto& g : n.array("any"))
                if (known(g.asString({})))
                    need.any.push_back(g.asString({}));
            need.everyDays = std::max(.1, n.number("everyDays", 1));
            need.winterDays = std::max(0.0, n.number("winterDays", 0));
            need.perPerson = n.boolean("perPerson");
            if (!need.any.empty())
                c.needs.push_back(std::move(need));
        }
        const auto& inst = crafts.object("institutions");
        c.institutionDays = std::max(.5, inst.number("days", 3));
        c.premium = std::max(1.0, inst.number("premium", 1.3));
        for (const auto& n : inst.array("list"))
        {
            Institution in;
            in.id = n.string("id");
            in.name = n.string("name");
            in.perGuard = n.string("per") == "guards";
            in.tithes = n.string("funds") == "tithes";
            if (const auto per = n.string("per"); per.rfind("producer:", 0) == 0)
                in.perProducer = per.substr(9);
            in.minResidents = std::max(0, int(n.number("minResidents", 0)));
            for (const auto& [item, rate] : n.object("basket").fields())
                if (known(item) && rate.asNumber(0) > 0)
                    in.basket.push_back({item, rate.asNumber(0)});
            if (!in.id.empty() && !in.basket.empty())
                c.institutions.push_back(std::move(in));
        }
        for (const auto& t : crafts.array("tools"))
        {
            ToolNeed need{t.string("producer"), t.string("item"), std::max(1.0, t.number("everyDays", 20))};
            if (!need.producer.empty() && known(need.item))
                c.tools.push_back(std::move(need));
        }
        for (const auto& [business, basket] : crafts.object("upkeep").fields())
            for (const auto& [item, rate] : basket.fields())
                if (known(item) && rate.asNumber(0) > 0)
                    c.upkeep[business].push_back({item, rate.asNumber(0)});
        for (const auto& [business, goods] : crafts.object("buys").fields())
            for (const auto& g : goods.items())
                if (known(g.asString({})))
                    c.buys[business].push_back(g.asString({}));
        for (const auto& [business, goods] : crafts.object("supplies").fields())
            for (const auto& g : goods.items())
                if (known(g.asString({})))
                    c.supplies[business].push_back(g.asString({}));
    }
    c.loaded = !c.wearables.empty();
    if (!c.loaded && c.error.empty())
        c.error = "no wearables in " + (dir / "items.json").string();
    return c;
}

// The quality kinds of every good, and the indexes by id (built once the lists will no longer move).
void addQualities(Catalog& c)
{
    const auto lowerFirst = [](std::string s) {
        if (!s.empty())
            s[0] = char(std::tolower(static_cast<unsigned char>(s[0])));
        return s;
    };
    const auto made = [&](const Item& base, int q) {
        Item v = base;
        v.id = withQuality(base.id, q);
        // (The game's sword is the dull bronze one; its kinds are bronze swords.)
        v.name = std::string(qualityName(q)) + " " + (base.id == "sword" ? std::string("bronze sword") : lowerFirst(base.name));
        v.durability = int(std::lround(base.durability * qualityDurability(q)));
        v.price = std::max(1, int(std::lround(base.price * qualityPrice(q))));
        // Better made wears better and looks it: armour a quarter stronger or weaker a step; status a step either way.
        v.protect = std::max(0, int(std::lround(base.protect * (q == 0 ? .75 : q == 2 ? 1.25 : q == 3 ? 1.5 : 1))));
        if (base.protect > 0 && q >= 2)
            v.protect = std::max(v.protect, base.protect + 1);
        v.status = base.status + (q == 0 ? -1 : q == 2 ? 1 : q == 3 ? 2 : 0);
        v.warmth = base.warmth + (q == 3 ? 1 : 0);
        return v;
    };
    for (const auto& g : c.goods)
        if (qualityApplies(g.id) && g.price >= 1)
            for (int q : {0, 2, 3})
                c.goodVariants.push_back(made(g, q));
    for (const auto& w : c.wearables)
        if (qualityApplies(w.id))
            for (int q : {0, 2, 3})
                c.wearVariants.push_back(made(w, q));
    for (const auto* list : {&c.goods, &c.goodVariants})
        for (const auto& g : *list)
            c.goodIndex.emplace(g.id, &g);
    for (const auto* list : {&c.wearables, &c.wearVariants})
        for (const auto& w : *list)
            c.wearIndex.emplace(w.id, &w);
}

const Catalog& catalog()
{
    static Catalog c = build();
    static const bool indexed = (addQualities(c), true);
    (void)indexed;
    return c;
}
} // namespace

bool load(std::string* error)
{
    const auto& c = catalog();
    if (error)
        *error = c.error;
    return c.loaded;
}

const Item* wearable(const std::string& id)
{
    const auto& index = catalog().wearIndex;
    const auto found = index.find(unmarked(id));
    return found == index.end() ? nullptr : found->second;
}

const std::vector<Item>& wearables()
{
    return catalog().wearables;
}

std::vector<std::string> slotsFor(const Item& item)
{
    if (item.slot == "head" || item.slot == "body" || item.slot == "harness" || item.slot == "paws")
        return {item.slot};
    if (item.slot == "throat")
        return {"neck"};
    if (item.slot == "shoulders")
        return {"back"};
    if (item.slot == "sling")
        return {"chest_left", "chest_right"};
    return {};
}

bool wearSlot(const std::string& slot)
{
    return std::find(std::begin(WearSlots), std::end(WearSlots), slot) != std::end(WearSlots);
}

bool furSpot(const std::string& spot)
{
    return std::find(std::begin(FurSpots), std::end(FurSpots), spot) != std::end(FurSpots);
}

bool spotAllowed(const Item& item, const std::string& spot)
{
    return item.slot == "jewelry" && std::find(item.spots.begin(), item.spots.end(), spot) != item.spots.end();
}

const Business* businessFor(const std::string& workLabel)
{
    // Asked for the same few work labels over and over (every resident, every decision): each thread remembers.
    thread_local std::unordered_map<std::string, const Business*> known;
    if (const auto found = known.find(workLabel); found != known.end())
        return found->second;
    const Business* matched = nullptr;
    std::string work = workLabel;
    std::transform(work.begin(), work.end(), work.begin(), [](unsigned char ch) { return char(std::tolower(ch)); });
    for (const auto& b : catalog().businesses)
        for (const auto& m : b.match)
            if (!matched && !m.empty() && work.find(m) != std::string::npos)
                matched = &b;
    return known.emplace(workLabel, matched).first->second;
}

std::vector<std::string> wearablesSold(const Business& business)
{
    std::vector<std::string> out;
    for (const auto& item : catalog().wearables)
        if (std::find(business.sells.begin(), business.sells.end(), item.id) != business.sells.end() ||
            std::find(business.sells.begin(), business.sells.end(), item.category) != business.sells.end())
            out.push_back(item.id);
    return out;
}

const Item* good(const std::string& id)
{
    const auto& index = catalog().goodIndex;
    const auto mark = id.find('@');                 // (Most ids carry no maker's mark: no copy to look them up.)
    const auto found = mark == std::string::npos ? index.find(id) : index.find(id.substr(0, mark));
    return found == index.end() ? nullptr : found->second;
}

std::string baseOf(const std::string& id)
{
    return id.substr(0, id.find_first_of("~@"));
}

std::string unmarked(const std::string& id)
{
    return id.substr(0, id.find('@'));
}

std::string makerOf(const std::string& id)
{
    const auto at = id.find('@');
    return at == std::string::npos ? std::string() : id.substr(at + 1);
}

std::string withMaker(const std::string& id, const std::string& maker)
{
    return maker.empty() || maker.find_first_of("~@") != std::string::npos ? unmarked(id) : unmarked(id) + "@" + maker;
}

double qualityDurability(int quality)
{
    return quality == 0 ? .6 : quality == 2 ? 1.5 : quality == 3 ? 2.5 : 1;
}

double qualityDamage(int quality)
{
    return quality == 0 ? .85 : quality == 2 ? 1.15 : quality == 3 ? 1.3 : 1;
}

int qualityOf(const std::string& id)
{
    const auto at = id.find('~');
    if (at == std::string::npos)
        return 1;
    const auto q = unmarked(id.substr(at + 1));
    return q == "crude" ? 0 : q == "fine" ? 2 : q == "masterwork" ? 3 : 1;
}

std::string withQuality(const std::string& base, int quality)
{
    const auto b = baseOf(base);
    if (!qualityApplies(b))
        return b;
    return quality == 0 ? b + "~crude" : quality == 2 ? b + "~fine" : quality == 3 ? b + "~masterwork" : b;
}

const char* qualityName(int quality)
{
    return quality == 0 ? "Crude" : quality == 2 ? "Fine" : quality == 3 ? "Masterwork" : "Common";
}

double qualityPrice(int quality)
{
    return quality == 0 ? .6 : quality == 2 ? 1.6 : quality == 3 ? 3 : 1;
}

bool qualityApplies(const std::string& base)
{
    return base != "herbs" && base != "meal" && base != "water" && base.find_first_of("~@") == std::string::npos;
}

std::vector<std::string> kindsOf(const std::string& base)
{
    const auto b = baseOf(base);
    if (!qualityApplies(b))
        return {b};
    return {b, b + "~crude", b + "~fine", b + "~masterwork"};
}

std::vector<std::string> goodsSold(const Business& business, int maxPrice)
{
    std::vector<std::string> out;
    for (const auto& item : catalog().goods)
        if (item.price >= 1 && item.price <= maxPrice &&
            (std::find(business.sells.begin(), business.sells.end(), item.id) != business.sells.end() ||
             std::find(business.sells.begin(), business.sells.end(), item.category) != business.sells.end()))
            out.push_back(item.id);
    return out;
}

bool traded(const std::string& item)
{
    for (const auto& craft : catalog().crafts)
        for (const auto& i : craft.in)
            if (i.first == item)
                return true;
    for (const auto& [business, goods] : catalog().supplies)
        if (std::find(goods.begin(), goods.end(), item) != goods.end())
            return true;
    for (const auto& need : catalog().needs)
        if (need.everyDays <= 7 && std::find(need.any.begin(), need.any.end(), item) != need.any.end())
            return true;
    for (const auto& in : catalog().institutions)
        for (const auto& [want, rate] : in.basket)
            if (want == item)
                return true;
    return item == "bread" || item == "porridge" || item == "meal";   // (What the hungry buy most.)
}

const std::vector<HouseholdNeed>& householdNeeds()
{
    return catalog().needs;
}

const std::vector<Institution>& institutions()
{
    return catalog().institutions;
}

const std::vector<ToolNeed>& toolNeeds()
{
    return catalog().tools;
}

const std::vector<std::pair<std::string, double>>* upkeepFor(const std::string& business)
{
    const auto& u = catalog().upkeep;
    const auto found = u.find(business);
    return found == u.end() ? nullptr : &found->second;
}

double institutionDays()
{
    return catalog().institutionDays;
}

double contractPremium()
{
    return catalog().premium;
}

int householdReserve()
{
    return catalog().reserve;
}

bool seasonal(const std::string& item)
{
    if (!load())
        return false;
    bool any = false;
    for (const auto& p : catalog().producers)
        for (const auto& [made, n] : p.out)
            if (made == item)
            {
                if (p.seasons.empty())
                    return false;
                any = true;
            }
    return any;
}

const Producer* producerFor(const std::string& workLabel)
{
    thread_local std::unordered_map<std::string, const Producer*> known;   // (As businessFor.)
    if (const auto found = known.find(workLabel); found != known.end())
        return found->second;
    const Producer* matched = nullptr;
    std::string work = workLabel;
    std::transform(work.begin(), work.end(), work.begin(), [](unsigned char ch) { return char(std::tolower(ch)); });
    for (const auto& p : catalog().producers)
        for (const auto& m : p.match)
            if (!matched && work.find(m) != std::string::npos)
                matched = &p;
    return known.emplace(workLabel, matched).first->second;
}

std::vector<const Craft*> craftsFor(const std::string& business)
{
    std::vector<const Craft*> out;
    for (const auto& craft : catalog().crafts)
        if (std::find(craft.makers.begin(), craft.makers.end(), business) != craft.makers.end())
            out.push_back(&craft);
    return out;
}

const std::vector<std::string>& suppliesFor(const std::string& business)
{
    static const std::vector<std::string> none;
    const auto found = catalog().supplies.find(business);
    return found == catalog().supplies.end() ? none : found->second;
}

const std::vector<std::string>& buysFor(const std::string& business)
{
    static const std::vector<std::string> none;
    const auto found = catalog().buys.find(business);
    return found == catalog().buys.end() ? none : found->second;
}

std::string placeName(const std::string& where)
{
    if (where == "foreleg_left") return "the left foreleg";
    if (where == "foreleg_right") return "the right foreleg";
    if (where == "hindleg_left") return "the left hind leg";
    if (where == "hindleg_right") return "the right hind leg";
    if (where == "chest_left") return "the left side of the chest";
    if (where == "chest_right") return "the right side of the chest";
    return "the " + where;
}
} // namespace ratw::items
