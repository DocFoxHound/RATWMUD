#include "RatwItems.h"

#include "RatwJsonDoc.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <set>
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
    std::map<std::string, std::vector<std::string>> supplies, buys;
    Ladder ladder;
    Households households;
    CityLoop cityLoop;
    Farms farms;
    FarmPeople farmPeople;
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

int tierOf(const std::string& name)
{
    return name == "basic" ? 1 : name == "professional" ? 2 : name == "exceptional" ? 3 : 0;
}

int wearNumber(const json::Value& item, const char* key)
{
    return int(item.object("wear").number(key, 0));
}

// The farms' defaults (country_farms.json): its areas and their farms, each good one the catalogue knows.
void readFarms(Catalog& c, const json::Value& file)
{
    const auto known = [&](const std::string& id) {
        return std::any_of(c.goods.begin(), c.goods.end(), [&](const Item& g) { return g.id == id; });
    };
    const auto id = [](const std::string& s) {
        return !s.empty() && s.size() <= 48 &&
               std::all_of(s.begin(), s.end(), [](unsigned char ch) { return std::islower(ch) || std::isdigit(ch) || ch == '_'; });
    };
    // A wage or another whole number, or -1 where it is null (the next level's).
    const auto orUnset = [](const json::Value& v, int high) {
        return v.isNumber() ? std::clamp(int(std::lround(v.asNumber())), 0, high) : -1;
    };
    const auto four = [](const json::Value& v, std::array<double, 4>& into) {
        if (!v.isArray() || v.size() != 4)
            return false;
        for (std::size_t i = 0; i < 4; ++i)
            into[i] = std::clamp(v.items()[i].asNumber(0), 0., 100000.);
        return true;
    };
    auto& f = c.farms;
    const auto& d = file.object("defaults");
    f.keeper = std::max(0, orUnset(d.object("wages")["keeper"], 1000));
    f.hand = std::max(0, orUnset(d.object("wages")["hand"], 1000));
    f.floatDays = std::clamp(int(d.number("floatDays", 2)), 0, 365);
    f.openingStockDays = std::clamp(int(d.number("openingStockDays", 7)), 0, 365);
    f.estatePeople = std::clamp(int(d.number("estatePeople", 24)), 0, 100000);
    f.otherWorkers = std::clamp(int(d.number("otherWorkers", 68)), 0, 100000);
    const auto& schedule = d.object("schedule");
    f.start = std::clamp(int(schedule.number("start", -1)), -1, 365000000);
    f.every = std::clamp(int(schedule.number("every", 28)), 1, 3650);
    for (const auto& a : schedule.array("order"))
        if (id(a.asString({})))
            f.order.push_back(a.asString({}));
    for (const auto& [item, share] : file.object("basket").fields())
        if (known(item) && share.asNumber(0) >= 1)
            f.basket.push_back({item, int(std::min(1000000., share.asNumber(0)))});
    // (Doc 73: the Country's basket is the Wharf's; an older file's countryBasket read as it.)
    for (const auto& [item, share] : file.object(file.has("wharfBasket") ? "wharfBasket" : "countryBasket").fields())
        if (known(item) && share.asNumber(0) >= 1)
            f.wharfBasket.push_back({item, int(std::min(1000000., share.asNumber(0)))});
    for (const auto& t : file.array("switchTrades"))
        if (id(t.asString({})))
            f.switchTrades.push_back(t.asString({}));
    std::set<std::string> farmIds, areaGoods;
    for (const auto& a : file.array("areas"))
    {
        FarmArea area;
        area.id = a.string("id");
        area.name = a.string("name", area.id);
        if (!id(area.id) || std::any_of(f.areas.begin(), f.areas.end(), [&](const FarmArea& x) { return x.id == area.id; }))
            continue;
        for (const auto& g : a.array("goods"))
            if (known(g.asString({})) && areaGoods.insert(g.asString({})).second)
                area.goods.push_back(g.asString({}));   // (A good in one area at most: the first that names it.)
        area.on = a["on"].isBool() ? int(a["on"].asBool()) : -1;
        area.yieldScale = std::clamp(int(a.number("yieldScale", 100)), 0, 1000);
        area.crewScale = std::clamp(int(a.number("crewScale", 100)), 0, 1000);
        area.keeper = orUnset(a.object("wages")["keeper"], 1000);
        area.hand = orUnset(a.object("wages")["hand"], 1000);
        if (a.string("price") == "year")
            area.priceFactor = std::clamp(int(a.number("priceFactor", 100)), 10, 1000);
        double takings = 0;
        for (const auto& v : a.array("revenueADay"))
            takings += std::max(0., v.asNumber(0)) / 4;
        area.takings = takings;
        area.works = a.boolean("works", false);
        area.house = a.string("house");
        for (const auto& x : a.array("farms"))
        {
            FarmDef farm;
            farm.id = x.string("id");
            farm.kind = x.string("kind");
            farm.area = area.id;
            farm.house = x.string("house");
            farm.landRent = x.string("landRent");
            if (!id(farm.id) || !farmIds.insert(farm.id).second)
                continue;
            farm.keepers = std::clamp(int(x.object("crew").number("keepers", 1)), 0, 100);
            farm.hands = std::clamp(int(x.object("crew").number("hands", 0)), 0, 100);
            farm.keeper = orUnset(x.object("wages")["keeper"], 1000);
            farm.hand = orUnset(x.object("wages")["hand"], 1000);
            farm.reserve = std::clamp<std::int64_t>(std::llround(x.number("reserve", 0)), 0, 1000000);
            farm.purse = std::clamp<std::int64_t>(std::llround(x.number("purse", 0)), 0, 1000000);
            farm.basketADay = std::clamp(x.number("basketADay", 0), 0., 1000000.);
            for (const auto& [item, values] : x.object("yield").fields())
                if (std::array<double, 4> v{}; known(item) && four(values, v))
                    farm.yield[item] = v;
            for (const auto& [item, values] : x.object("table").fields())
                if (std::array<double, 4> v{}; farm.yield.count(item) && four(values, v))
                    farm.table[item] = v;
            for (const auto& m : x.array("makes"))
            {
                FarmDef::Make make;
                make.good = m.string("good");
                make.from = m.string("from");
                make.in = std::clamp(int(m.number("in", 1)), 1, 1000);
                make.share = std::clamp(int(m.number("share", 100)), 0, 100);
                make.store = std::clamp(int(m.number("store", 0)), 0, 100000);
                if (std::array<double, 4> v{}; four(m["aDay"], v))
                    make.aDay = v;
                if (known(make.good) && farm.yield.count(make.from) && make.good != make.from && make.store > 0)
                    farm.makes.push_back(std::move(make));
            }
            area.farms.push_back(std::move(farm));
        }
        f.areas.push_back(std::move(area));
    }
}

// The farms' people (communities.json, the places; farm_people.json, the rules; doc 71, Phase C).
void readFarmPeople(Catalog& c, const json::Value& places, const json::Value& rules, const json::Value& workshops)
{
    auto& p = c.farmPeople;
    const auto spots = [](const json::Value& list) {
        std::vector<std::pair<double, double>> out;
        for (const auto& s : list.items())
            if (s.isObject())
                out.push_back({std::floor(s.number("x", 0)) + .5, std::floor(s.number("y", 0)) + .5});   // (Tile centres.)
        return out;
    };
    const auto room = [&](const json::Value& r, const char* spotsKey) {
        PlaceRoom out;
        out.area = r.string("area");
        out.name = r.string("name", out.area);
        out.forName = r.string("for");
        out.spots = spots(r[spotsKey]);
        out.capacity = std::clamp(int(r.number("capacity", double(out.spots.size()))), 0, 64);
        return out;
    };
    const auto words = [](const json::Value& list) {
        std::vector<std::string> out;
        for (const auto& w : list.items())
            if (const auto s = w.asString({}); !s.empty())
            {
                std::string l = s;
                std::transform(l.begin(), l.end(), l.begin(), [](unsigned char ch) { return char(std::tolower(ch)); });
                out.push_back(l);
            }
        return out;
    };
    p.householdMost = std::clamp(int(places.number("householdMost", 6)), 1, 64);
    for (const auto& f : places.array("farms"))
    {
        FarmPlace farm;
        farm.farm = f.string("farm");
        farm.kind = f.string("kind");
        farm.steading = f.string("steading", farm.farm);
        farm.community = f.string("community");
        for (const auto& w : f.array("workplaces"))
            if (auto r = room(w, "work"); !r.area.empty())
                farm.workplaces.push_back(std::move(r));
        farm.home = room(f.object("home"), "beds");
        if (!farm.farm.empty() && !farm.workplaces.empty() && !farm.home.area.empty() && !farm.community.empty())
            p.farms.push_back(std::move(farm));
    }
    // Doc 72's works: those built (`works`, its id in country_farms.json), each its work spots out of doors.
    for (const auto& w : places.array("works"))
    {
        FarmPlace works;
        works.farm = w.string("works");
        works.kind = w.string("kind");
        works.steading = w.string("name", works.farm);
        works.community = w.string("community");
        works.works = true;
        if (const auto& at = w["workplace"]; at.isObject())
            if (auto r = room(at, "work"); !r.area.empty() && !r.spots.empty())
                works.workplaces.push_back(std::move(r));
        if (!works.farm.empty() && !works.workplaces.empty() && !works.community.empty() &&
            std::none_of(p.farms.begin(), p.farms.end(), [&](const FarmPlace& f) { return f.farm == works.farm; }))
            p.farms.push_back(std::move(works));
    }
    for (const auto& x : places.array("communities"))
    {
        CommunityPlace community;
        community.id = x.string("id");
        community.name = x.string("name", community.id);
        community.region = x.string("region", community.id);
        community.town = x.boolean("town", false);
        for (const auto& h : x.array("homes"))
            if (auto r = room(h, "beds"); !r.area.empty() && !r.spots.empty())
                community.homes.push_back(std::move(r));
        for (const auto& h : x.array("flats"))
            if (auto r = room(h, "beds"); !r.area.empty() && !r.spots.empty())
                community.flats.push_back(std::move(r));
        for (const auto& m : x.array("market"))
        {
            MarketPlace market;
            market.area = m.string("area");
            market.name = m.string("name", market.area);
            market.business = m.string("business");
            market.work = spots(m["work"]);
            for (const auto& post : m.array("posts"))
                if (const auto name = post.string("post"); !name.empty())
                    market.posts.push_back({name, std::clamp(int(post.number("count", 1)), 0, 16)});
            if (!market.area.empty() && !market.work.empty())
                community.market.push_back(std::move(market));
        }
        if (!community.id.empty())
            p.communities.push_back(std::move(community));
    }
    // The country workshops (doc 72, Phase 5; workshops.json): each into its community's market, the map's entry for its
    // building (its work spots) where there is one, else with the file's own spots; their business, posts and name ours.
    for (const auto& w : workshops.array("workshops"))
    {
        Workshop shop;
        shop.area = w.string("building");
        shop.community = w.string("community");
        shop.name = w.string("name", shop.area);
        shop.business = w.string("business");
        shop.house = w.string("house");
        shop.work = spots(w["work"]);
        for (const auto& post : w.array("posts"))
            if (const auto name = post.string("post"); !name.empty())
                shop.posts.push_back({name, std::clamp(int(post.number("count", 1)), 0, 16)});
        auto at = std::find_if(p.communities.begin(), p.communities.end(),
                               [&](const CommunityPlace& x) { return x.id == shop.community; });
        if (shop.area.empty() || shop.business.empty() || shop.posts.empty() || at == p.communities.end() ||
            std::any_of(p.workshops.begin(), p.workshops.end(), [&](const Workshop& x) { return x.area == shop.area; }))
            continue;
        auto m = std::find_if(at->market.begin(), at->market.end(), [&](const MarketPlace& x) { return x.area == shop.area; });
        if (m == at->market.end())
        {
            // (The map's own entry for it: its community's `workshops`, the work spots by its stations.)
            for (const auto& x : places.array("communities"))
                if (x.string("id") == shop.community)
                    for (const auto& laid : x.array("workshops"))
                        if (laid.string("area", laid.string("id")) == shop.area)
                            if (auto laidWork = spots(laid["work"]); !laidWork.empty())
                                shop.work = std::move(laidWork);
            if (shop.work.empty())
                continue;
            at->market.push_back({});
            m = at->market.end() - 1;
            m->area = shop.area;
            m->work = shop.work;
        }
        m->name = shop.name;
        m->business = shop.business;
        m->posts = shop.posts;
        p.workshops.push_back(std::move(shop));
    }
    for (const auto& t : rules.array("trades"))
        if (const auto id = t.string("id"); !id.empty())
            p.trades.push_back({id, words(t["words"])});
    p.fallback = words(rules["fallback"]);
    // (Trade ids are matched as written: the fallback list names them, lowered; keep the ids' own case for reports.)
    for (auto& f : p.fallback)
        for (const auto& [id, unused] : p.trades)
        {
            std::string l = id;
            std::transform(l.begin(), l.end(), l.begin(), [](unsigned char ch) { return char(std::tolower(ch)); });
            if (l == f)
                f = id;
        }
    for (const auto& [farm, name] : rules.object("places").fields())
        if (const auto text = name.asString({}); !text.empty())
            p.places[farm] = text;
    for (const auto& s : rules.array("suffixes"))
        if (const auto text = s.asString({}); !text.empty())
            p.suffixes.push_back(text);
    const auto hours = [](const json::Value& v, double& from, double& to) {
        if (v.isArray() && v.size() == 2 && v.items()[0].isNumber() && v.items()[1].isNumber())
        {
            from = std::clamp(v.items()[0].asNumber(), 0., 23.5);
            to = std::clamp(v.items()[1].asNumber(), 0., 23.5);
        }
    };
    hours(rules.object("hours")["farm"], p.farmStart, p.farmEnd);
    hours(rules.object("hours")["market"], p.marketStart, p.marketEnd);
    const auto ids = [](const json::Value& list) {
        std::vector<std::string> out;
        for (const auto& w : list.items())
            if (const auto s = w.asString({}); !s.empty())
                out.push_back(s);
        return out;
    };
    for (const auto& [kind, k] : rules.object("kinds").fields())
    {
        FarmKindPeople people;
        people.trades = ids(k["trades"]);
        people.words = words(k["words"]);
        people.keeper = k.string("keeper");
        people.hand = k.string("hand");
        const auto& second = k.object("second");
        people.second = second.string("label");
        people.secondCount = people.second.empty() ? 0 : std::clamp(int(second.number("count", 1)), 0, 16);
        people.secondWords = words(second["words"]);
        if (!people.keeper.empty() && !people.hand.empty())
            p.kinds[kind] = std::move(people);
    }
    for (const auto& [post, m] : rules.object("market").fields())
    {
        MarketPostPeople people{m.string("label"), m.string("role", "civilian"), words(m["words"]), ids(m["trades"])};
        if (!people.label.empty() && (people.role == "merchant" || people.role == "civilian"))
            c.farmPeople.market[post] = std::move(people);
    }
    p.loaded = !p.farms.empty() && !p.kinds.empty();
}

Catalog build()
{
    Catalog c;
    json::Value items, businesses;
    const auto dir = itemsDir();
    if (!readJson(dir / "items.json", items, c.error) || !readJson(dir / "businesses.json", businesses, c.error))
        return c;
    // The worth ladder (Data/Economy/ladder.json): the catalogue's labourer's day, and its rungs, each a range of LD an
    // item's `at` places it in on a log scale. A world without the file has no places.
    std::map<std::string, std::pair<double, double>> rungs;
    if (const auto file = dir.parent_path() / "Economy" / "ladder.json"; std::filesystem::exists(file))
    {
        json::Value ladder;
        std::string problem;
        if (readJson(file, ladder, problem))
        {
            c.ladder.catalogueDay = std::max(1.0, ladder.number("catalogueDay", 12));
            for (const auto& r : ladder.array("rungs"))
                if (const double from = r.number("from", 0), to = r.number("to", 0); from > 0 && to > from)
                    rungs[r.string("id")] = {from, to};
        }
    }
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
        good.tier = tierOf(i.string("tier"));
        if (const auto r = rungs.find(i.object("worth").string("rung")); r != rungs.end())
        {
            const double at = std::clamp(i.object("worth").number("at", .5), 0.0, 1.0);
            good.worth = r->second.first * std::pow(r->second.second / r->second.first, at);
        }
        if (const auto& w = i.object("weapon"); i.has("weapon"))
        {
            good.weapon.kind = w.string("class");
            good.weapon.type = w.string("type", "cut");
            good.weapon.damage = std::max(0.0, w.number("damage", 0));
            good.weapon.stamina = std::max(0.0, w.number("stamina", 0));
            good.weapon.weight = std::max(0.0, w.number("turnWeight", 0));
            good.weapon.knockLoose = std::max(0.0, w.number("knockLoose", 1));
            good.weapon.reach = std::max(1, int(w.number("reach", 1)));
            good.weapon.pierce = std::max(0, int(w.number("pierce", 0)));
            good.weapon.hit = int(w.number("hitBonus", 0));
            if (good.weapon.damage <= 0)
                good.weapon.kind.clear();           // (No blow, no weapon a fight can use.)
        }
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
        item.protect = std::max(0.0, i.object("armor").number("protect", 0));
        item.durability = int(i.number("durability", 0));
        item.vsCut = i.object("armor").object("vs").number("cut", 0);
        item.vsThrust = i.object("armor").object("vs").number("thrust", 0);
        item.vsBlunt = i.object("armor").object("vs").number("blunt", 0);
        item.dex = int(i.object("armor").number("dex", 0));
        item.spikes = std::max(0.0, i.object("armor").number("spikes", 0));
        item.tier = tierOf(i.string("tier"));
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
        // The skill each craft's trade needs: the player recipe of the same id's (recipes.json), else none.
        std::map<std::string, int> skills;
        if (json::Value recipes; readJson(dir / "recipes.json", recipes, craftError))
            for (const auto& r : recipes.array("recipes"))
                skills[r.string("id")] = std::clamp(int(r.number("skill", 0)), 0, 200);
        craftError.clear();
        for (const auto& k : crafts.array("crafts"))
        {
            Craft craft;
            craft.skill = skills.count(k.string("id")) ? skills[k.string("id")] : 0;
            craft.id = k.string("id");
            craft.seconds = k.number("seconds", 60);
            craft.hours = k.number("hours", craft.seconds / 3600.);
            for (const auto& m : k.array("makers"))
                craft.makers.push_back(m.asString({}));
            if (!craft.id.empty() && !craft.makers.empty() && craft.seconds > 0 && craft.seconds <= 86400 && craft.hours > 0 &&
                craft.hours <= 2000 &&
                counts(k.object("in"), craft.in) && craft.in.size() <= 2 && counts(k.object("out"), craft.out))
                c.crafts.push_back(std::move(craft));
        }
        for (const auto& k : crafts.array("producers"))
        {
            Producer producer;
            producer.id = k.string("id");
            producer.seconds = k.number("seconds", 1800);
            producer.hours = k.number("hours", producer.seconds / 3600.);
            for (const auto& m : k.array("match"))
                if (!m.asString({}).empty())
                    producer.match.push_back(m.asString({}));
            for (const auto& season : k.array("seasons"))
                producer.seasons.push_back(int(season.asNumber(-1)));
            if (k.has("offSeason"))
                counts(k.object("offSeason"), producer.offSeason);
            if (!producer.id.empty() && !producer.match.empty() && producer.seconds > 0 && producer.seconds <= 86400 &&
                producer.hours > 0 && producer.hours <= 2000 &&
                counts(k.object("out"), producer.out) &&
                std::all_of(producer.seasons.begin(), producer.seasons.end(), [](int s) { return s >= 0 && s <= 3; }))
                c.producers.push_back(std::move(producer));
        }
        for (const auto& [business, goods] : crafts.object("buys").fields())
            for (const auto& g : goods.items())
                if (known(g.asString({})))
                    c.buys[business].push_back(g.asString({}));
        for (const auto& [business, goods] : crafts.object("supplies").fields())
            for (const auto& g : goods.items())
                if (known(g.asString({})))
                    c.supplies[business].push_back(g.asString({}));
        // Households' needs (doc 35, Part 7; bought again by the city's loop, doc 68).
        const auto& h = crafts.object("households");
        c.households.reserve = std::clamp(int(h.number("reserve", 6)), 0, 1000);
        for (const auto& n : h.array("needs"))
        {
            HouseholdNeed need;
            for (const auto& g : n.array("any"))
                if (known(g.asString({})))
                    need.any.push_back(g.asString({}));
            need.everyDays = n.number("everyDays", 0);
            need.winterDays = n.number("winterDays", 0);
            need.perPerson = n.boolean("perPerson");
            if (!need.any.empty() && need.everyDays > 0 && need.everyDays <= 3650 && need.winterDays >= 0 && need.winterDays <= 3650)
                c.households.needs.push_back(std::move(need));
        }
    }
    // Steering the economy (doc 74, 3 and 4): the table as the doc has it, then city_loop.json's `steering` over it.
    {
        const double W = std::numeric_limits<double>::quiet_NaN();
        auto& t = c.cityLoop.steering;
        t.levers = {{"shareOut", {30, 12, 0, 0, 0}},      {"postsShare", {50, 25, 0, 0, 0}},
                    {"tollLean", {1, .5, 0, 0, 0}},    {"surplusKeep", {0, 1000, W, W, W}},
                    {"treasuryCap", {1500, W, W, W, W}},  {"surplusAlmsTo", {80, 60, W, W, W}},
                    {"groundRent", {0, 0, 0, 15, 30}},    {"rentScale", {100, 100, 100, 112, 125}},
                    {"dividend", {0, 0, 0, 15, 30}},      {"richAbove", {W, W, W, 100, 150}},
                    {"spendAbove", {25, 32, W, W, W}}};
        t.targets = {{"gini", {.33, .41, .487, W, W}},        {"giniBand", {.04, .04, .035, W, W}},
                     {"upper", {W, W, 13, 18.5, 24}},           {"upperBand", {W, W, 2, 2.5, 3}},
                     {"upperMost", {14, 14, W, W, W}},        {"top10Most", {27, 31, W, W, W}},
                     {"top10Least", {W, W, W, 37, 40}},       {"hungerMost", {.02, .05, .15, .15, .15}},
                     {"shortMost", {.1, .5, 3.5, 4, 5}}};
    }
    // The Country and the city's loop (doc 68): optional; without it, one market with an empty basket.
    // (Doc 73: the file was country.json; an older data folder's is still read.)
    json::Value country;
    std::string countryError;
    if (readJson(dir / "city_loop.json", country, countryError) ||
        (!std::filesystem::exists(dir / "city_loop.json") && readJson(dir / "country.json", country, countryError)))
    {
        const auto known = [&](const std::string& id) {
            return std::any_of(c.goods.begin(), c.goods.end(), [&](const Item& g) { return g.id == id; });
        };
        for (const auto& m : country.array("markets"))
        {
            Market market;
            market.id = m.string("id");
            market.name = m.string("name", market.id);
            for (const auto& [item, share] : m.object("basket").fields())
                if (known(item) && share.asNumber(0) >= 1 && share.asNumber(0) <= 1000000)
                    market.basket.push_back({item, int(share.asNumber(0))});
            const bool idOk = !market.id.empty() && market.id.size() <= 40 &&
                              std::all_of(market.id.begin(), market.id.end(), [](unsigned char ch) { return std::islower(ch) || std::isdigit(ch) || ch == '_'; });
            if (idOk && std::none_of(c.cityLoop.markets.begin(), c.cityLoop.markets.end(), [&](const Market& x) { return x.id == market.id; }))
                c.cityLoop.markets.push_back(std::move(market));
        }
        for (const auto& r : country.object("wages").array("rules"))
        {
            WageRule rule;
            rule.payer = r.string("payer");
            if (rule.payer == "country")
                rule.payer = "estates";             // (Doc 73: the Country's posts are the estates', the Wharf's among them.)
            rule.name = r.string("name", rule.payer);
            rule.wage = r.string("wage");
            for (const auto& w : r.array("words"))
                if (!w.asString({}).empty())
                    rule.words.push_back(w.asString({}));
            if (!rule.payer.empty() && !rule.words.empty())
                c.cityLoop.wages.push_back(std::move(rule));
        }
        // The Houses' estates (doc 72, Phase 6): an id and a House each, none twice; at most one grants.
        for (const auto& e : country.object("estates").array("list"))
        {
            Estate estate{e.string("id"), e.string("house"), e.string("name"), e.string("fund"), e.boolean("grants"), {}};
            if (estate.fund == "country")
                estate.fund = "wharf";              // (Doc 73: the Country's purse is the Wharf's.)
            if (estate.name.empty())
                estate.name = estate.house;
            for (const auto& w : e.array("words"))
                if (!w.asString({}).empty())
                    estate.words.push_back(w.asString({}));
            const auto& all = c.cityLoop.estates;
            const bool idOk = !estate.id.empty() && estate.id.size() <= 40 &&
                              std::all_of(estate.id.begin(), estate.id.end(), [](unsigned char ch) { return std::islower(ch) || std::isdigit(ch) || ch == '_'; });
            if (idOk && !estate.house.empty() &&
                std::none_of(all.begin(), all.end(), [&](const Estate& x) { return x.id == estate.id || x.house == estate.house; }) &&
                (!estate.grants || std::none_of(all.begin(), all.end(), [](const Estate& x) { return x.grants; })))
                c.cityLoop.estates.push_back(std::move(estate));
        }
        const auto& food = country.object("food");
        c.cityLoop.foodDays = std::clamp(food.number("days", 2), .5, 10.);
        c.cityLoop.shortDays = std::clamp(food.number("shortDays", 1), .1, c.cityLoop.foodDays);
        const auto& comforts = country.object("comforts");
        c.cityLoop.comfortsAbove = std::clamp(int(comforts.number("above", 60)), 0, 1000000);
        c.cityLoop.comfortsShare = std::clamp(comforts.number("share", .1), 0., 1.);
        for (const auto& g : comforts.array("goods"))
            if (known(g.asString({})))
                c.cityLoop.comforts.push_back(g.asString({}));
        for (const auto& g : country.object("luxuries").array("goods"))   // (Doc 72, Phase 5: cheese.)
            if (const auto id = g.asString({}); known(id) &&
                std::find(c.cityLoop.luxuries.begin(), c.cityLoop.luxuries.end(), id) == c.cityLoop.luxuries.end())
                c.cityLoop.luxuries.push_back(id);
        // The loop's rules and the city's services (doc 68, Progress, "First fixes").
        for (const auto& info : Rules)
        {
            // (A rule under its old name, doc 73, where the file has no new one.)
            const auto& rules = country.object("rules");
            const char* key = info.name;
            if (!rules.has(key) && std::string(key) == "estatePostsDays" && rules.has("countryPostsDays"))
                key = "countryPostsDays";
            c.cityLoop.rules[info.name] = std::clamp(int(std::lround(rules.number(key, info.fallback))), info.low, info.high);
        }
        for (const auto& s : country.object("services").array("list"))
        {
            ServiceDef def{s.string("id"), s.string("business"), std::clamp(int(s.number("every", 30)), 1, 3650),
                           s.string("for") == "patrons"};
            if (known(def.id) && !def.business.empty() &&
                std::none_of(c.cityLoop.services.begin(), c.cityLoop.services.end(), [&](const ServiceDef& x) { return x.id == def.id; }))
                c.cityLoop.services.push_back(std::move(def));
        }
        // Trades (doc 68, Progress, "Trades"): the city's suppliers, the armoury's patrons, dear goods' shelves.
        for (const auto& s : country.object("suppliers").array("list"))
        {
            Supplier supplier{s.string("buyer"), s.string("seller"), {}, std::clamp(int(s.number("price", 0)), 0, 1000)};
            for (const auto& g : s.array("goods"))
                if (known(g.asString({})))
                    supplier.goods.push_back(g.asString({}));
            if (!supplier.buyer.empty() && !supplier.seller.empty() && supplier.buyer != supplier.seller)
                c.cityLoop.suppliers.push_back(std::move(supplier));
        }
        // Steering the economy (doc 74): each lever's or target's five marks (a number, or null: the world's own value for
        // a lever, none for a target); a name the code doesn't know, or a row not of five, is left as the doc has it.
        if (const auto& steer = country.object("steering"); steer.isObject())
        {
            for (const auto& [kind, into] : {std::pair{"levers", &c.cityLoop.steering.levers}, std::pair{"targets", &c.cityLoop.steering.targets}})
                for (const auto& [name, row] : steer.object(kind).fields())
                {
                    const auto found = into->find(name);
                    const auto& marks = row.items();
                    if (found == into->end() || marks.size() != 5)
                        continue;
                    std::array<double, 5> v{};
                    bool sound = true;
                    for (std::size_t i = 0; i < 5; ++i)
                    {
                        if (marks[i].isNumber() && std::isfinite(marks[i].asNumber()) && std::abs(marks[i].asNumber()) <= 1e9)
                            v[i] = marks[i].asNumber();
                        else if (marks[i].isNull())
                            v[i] = std::numeric_limits<double>::quiet_NaN();
                        else
                            sound = false;
                    }
                    if (sound)
                        found->second = v;
                }
            c.cityLoop.steering.treasuryFloor = std::clamp(steer.number("treasuryFloor", 800), 0., 1e9);
            c.cityLoop.steering.treasuryLow = std::clamp(steer.number("treasuryLow", 500), 0., c.cityLoop.steering.treasuryFloor);
        }
        // The Houses' city businesses (doc 72, after iron6): a building and its House each, the first entry for a building.
        for (const auto& b : country.object("houseBusinesses").array("list"))
            if (const auto building = b.string("building"), house = b.string("house"); !building.empty() && !house.empty())
                c.cityLoop.houseTills.emplace(building, house);
        for (const auto& t : country.object("patrons").array("list"))
        {
            Patrons trade;
            trade.business = t.string("business");
            for (const auto& g : t.array("groups"))
            {
                PatronGroup group{g.string("id"), g.string("name"), g.string("payer"), {}, std::clamp(int(g.number("units", 0)), 0, 100000)};
                if (group.payer == "country")
                    group.payer = "estates";        // (Doc 73: as a wage rule's payer.)
                for (const auto& r : g.array("roles"))
                    if (!r.asString({}).empty())
                        group.roles.push_back(r.asString({}));
                if (!group.id.empty() && !group.payer.empty() && (!group.roles.empty() || group.units > 0))
                    trade.groups.push_back(std::move(group));
            }
            for (const auto& g : t.array("pieces"))
                if (known(g.asString({})))
                    trade.pieces.push_back(g.asString({}));
            if (!trade.business.empty() && !trade.groups.empty())
                c.cityLoop.patrons.push_back(std::move(trade));
        }
        // The treasury's surplus (doc 68, Progress, "The treasury's surplus"): the public works' trades and their goods.
        for (const auto& w : country.object("works").array("list"))
        {
            WorksOrder order{w.string("business"), {}, std::clamp(int(w.number("weight", 1)), 1, 1000)};
            for (const auto& g : w.array("goods"))
                if (known(g.asString({})))
                    order.goods.push_back(g.asString({}));
            if (!order.business.empty() && !order.goods.empty() &&
                std::none_of(c.cityLoop.works.begin(), c.cityLoop.works.end(), [&](const WorksOrder& x) { return x.business == order.business; }))
                c.cityLoop.works.push_back(std::move(order));
        }
        // The Wharf's lists (doc 73, 4.2): known goods only, `never` first, then a good on both lists dropped from both.
        {
            const auto& w = country.object("wharf");
            auto& lists = c.cityLoop.wharf;
            for (const auto& g : w.array("never"))
                if (const auto id = g.asString({}); !id.empty())
                    lists.never.insert(id);
            const auto read = [&](const char* key, std::map<std::string, WharfGood>& into) {
                for (const auto& [item, v] : w.object(key).fields())
                {
                    if (!known(item))
                    {
                        lists.refused.push_back(std::string(key) + ": no such good " + item);
                        continue;
                    }
                    if (lists.never.count(item))
                    {
                        lists.refused.push_back(std::string(key) + ": " + item + " is never the Wharf's");
                        continue;
                    }
                    WharfGood good{std::clamp(int(v.number("most", 0)), 0, 100000), !v.has("on") || v.boolean("on")};
                    good.rank = int(into.size());
                    good.keep = v.has("keep") ? std::clamp(int(v.number("keep", 0)), 0, 100000) : -1;
                    for (const auto& b : v.array("shops"))
                        if (const auto id = b.asString({}); !id.empty())
                            good.shops.insert(id);
                    into[item] = std::move(good);
                }
            };
            read("imports", lists.imports);
            read("takes", lists.takes);
            for (auto it = lists.imports.begin(); it != lists.imports.end();)
                if (lists.takes.erase(it->first))
                {
                    lists.refused.push_back("imports and takes: " + it->first + " on both");
                    it = lists.imports.erase(it);
                }
                else
                    ++it;
        }
        for (const auto& [item, n] : country.object("shelves").fields())
            if (known(item) && n.asNumber(0) >= 1)
                c.cityLoop.shelves[item] = std::clamp(int(n.asNumber(1)), 1, 1000);
        // The seasons (a group of goods each; a good in the first group that names it).
        std::set<std::string> grouped;
        for (const auto& g : country.object("seasons").array("groups"))
        {
            SeasonGroup group;
            group.id = g.string("id");
            group.name = g.string("name", group.id);
            const bool idOk = !group.id.empty() && group.id.size() <= 40 &&
                              std::all_of(group.id.begin(), group.id.end(), [](unsigned char ch) { return std::islower(ch) || std::isdigit(ch) || ch == '_'; });
            if (!idOk || std::any_of(c.cityLoop.seasons.begin(), c.cityLoop.seasons.end(), [&](const SeasonGroup& x) { return x.id == group.id; }))
                continue;
            for (const auto& item : g.array("goods"))
                if (known(item.asString({})) && grouped.insert(item.asString({})).second)
                    group.goods.push_back(item.asString({}));
            const auto four = [&](const char* key, std::array<int, 4>& into, int low, int high) {
                const auto& values = g.array(key);
                if (values.size() == 4)
                    for (std::size_t i = 0; i < 4; ++i)
                        into[i] = std::clamp(int(std::lround(values[i].asNumber(100))), low, high);
            };
            four("supply", group.supply, 0, 1000);
            four("price", group.price, 10, 1000);
            four("shelf", group.shelf, 0, 1000);
            group.normal = std::clamp(int(g.number("normal", 0)), 0, 1000000);
            group.stores = g.boolean("stores");
            if (!group.goods.empty())
                c.cityLoop.seasons.push_back(std::move(group));
        }
    }
    if (c.cityLoop.markets.empty())
        c.cityLoop.markets.push_back({"wharf", "the Wharf", {}});
    // The Country's food farms (Data/Economy/country_farms.json; doc 71, 4): optional; without it, none.
    if (const auto file = dir.parent_path() / "Economy" / "country_farms.json"; std::filesystem::exists(file))
    {
        json::Value farms;
        std::string problem;
        if (readJson(file, farms, problem))
            readFarms(c, farms);
    }
    // The farms' people (communities.json and farm_people.json; doc 71, Phase C): optional; without both, nobody is placed.
    if (const auto places = dir.parent_path() / "Economy" / "communities.json", rules = dir.parent_path() / "Economy" / "farm_people.json";
        std::filesystem::exists(places) && std::filesystem::exists(rules))
    {
        json::Value a, b, w;
        std::string problem;
        // (The country workshops, doc 72, Phase 5: optional; without the file, none.)
        if (const auto shops = dir.parent_path() / "Economy" / "workshops.json"; std::filesystem::exists(shops))
            readJson(shops, w, problem);
        if (readJson(places, a, problem) && readJson(rules, b, problem))
            readFarmPeople(c, a, b, w);
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
        // Better made wears better and looks it: armour protects by its quality (doc 47); status a step either way.
        v.protect = base.protect * qualityGuard(q);
        v.vsCut = base.vsCut * qualityGuard(q);
        v.vsThrust = base.vsThrust * qualityGuard(q);
        v.vsBlunt = base.vsBlunt * qualityGuard(q);
        v.spikes = base.spikes * qualityGuard(q);
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

const std::vector<Item>& allGoods()
{
    return catalog().goods;
}

const std::vector<Business>& businesses()
{
    return catalog().businesses;
}

const Business* business(const std::string& id)
{
    for (const auto& b : catalog().businesses)
        if (b.id == id)
            return &b;
    return nullptr;
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

const Item* blade(const std::string& id)
{
    const auto* g = good(id);
    return g && g->weapon.kind == "blade" ? g : nullptr;
}

const char* tierName(int tier)
{
    return tier == 1 ? "Basic" : tier == 2 ? "Professional" : tier == 3 ? "Exceptional" : "";
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
    // (Doc 47: a tier's step is a point or two of a blow, so a make's is less: a masterwork cuts about as the next metal.)
    return quality == 0 ? .92 : quality == 2 ? 1.05 : quality == 3 ? 1.1 : 1;
}

double qualityGuard(int quality)
{
    return quality == 0 ? .75 : quality == 2 ? 1.25 : quality == 3 ? 1.5 : 1;
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

const Ladder& ladder()
{
    return catalog().ladder;
}

const Households& households()
{
    return catalog().households;
}

double steerAt(const std::array<double, 5>& marks, double at, double own)
{
    at = std::clamp(std::isfinite(at) ? at : 0., -1., 1.);
    const auto value = [&](std::size_t i) { return std::isnan(marks[i]) ? own : marks[i]; };
    const double place = (at + 1) * 2;              // (0..4: the marks' index.)
    const auto i = std::min<std::size_t>(3, std::size_t(std::floor(place)));
    const double t = place - double(i);
    if (t <= 1e-12)
        return value(i);
    if (t >= 1 - 1e-12)
        return value(i + 1);
    const double a = value(i), b = value(i + 1);
    if (std::isnan(a) || std::isnan(b))
        return std::numeric_limits<double>::quiet_NaN();
    return a + (b - a) * t;
}

const CityLoop& cityLoop()
{
    return catalog().cityLoop;
}

const Farms& farms()
{
    return catalog().farms;
}

const FarmPeople& farmPeople()
{
    return catalog().farmPeople;
}

const FarmPlace* FarmPeople::farm(const std::string& id) const
{
    for (const auto& f : farms)
        if (f.farm == id)
            return &f;
    return nullptr;
}

const CommunityPlace* FarmPeople::community(const std::string& id) const
{
    for (const auto& c : communities)
        if (c.id == id)
            return &c;
    return nullptr;
}

const Workshop* FarmPeople::workshop(const std::string& area) const
{
    for (const auto& w : workshops)
        if (w.area == area)
            return &w;
    return nullptr;
}

std::set<std::string> FarmPeople::plainRegions(const std::function<bool(const std::string&)>& has) const
{
    std::set<std::string> out;
    for (const auto& c : communities)
    {
        if (c.town || c.region.empty())
            continue;
        const auto any = [&](const auto& rooms) {
            return std::any_of(rooms.begin(), rooms.end(), [&](const auto& r) { return has(r.area); });
        };
        if (any(c.homes) || any(c.flats) || any(c.market))
            out.insert(c.region);
    }
    return out;
}

std::string FarmPeople::tradeOf(const std::string& workLabel) const
{
    std::string l = workLabel;
    std::transform(l.begin(), l.end(), l.begin(), [](unsigned char ch) { return char(std::tolower(ch)); });
    for (const auto& [id, words] : trades)
        for (const auto& w : words)
            if (l.find(w) != std::string::npos)
                return id;
    return {};
}

std::string FarmPeople::placeName(const FarmPlace& farm) const
{
    if (const auto own = places.find(farm.farm); own != places.end())
        return own->second;
    const auto& steading = farm.steading;
    for (const auto& s : suffixes)
        if (steading.size() > s.size() && steading.compare(steading.size() - s.size(), s.size(), s) == 0)
            return steading.substr(0, steading.size() - s.size());
    return steading;
}

std::string ruleName(const std::string& name)
{
    return name == "countryPostsDays" ? "estatePostsDays" : name;
}

const RuleInfo* ruleInfo(const std::string& name)
{
    for (const auto& info : Rules)
        if (name == info.name)
            return &info;
    return nullptr;
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
