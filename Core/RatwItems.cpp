#include "RatwItems.h"

#include "RatwJsonDoc.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>

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
        if (item.id.empty() || item.name.empty() || item.price < 1 || item.price > 1000 || (slot == "jewelry" && item.spots.empty()))
            continue;
        c.wearables.push_back(std::move(item));
    }
    for (const auto& b : businesses.array("businesses"))
    {
        Business business;
        business.id = b.string("id");
        business.label = b.string("label");
        for (const auto& s : b.array("sells"))
            business.sells.push_back(s.asString({}));
        for (const auto& m : b.array("match"))
            business.match.push_back(m.asString({}));
        if (!business.match.empty())
            c.businesses.push_back(std::move(business));
    }
    c.loaded = !c.wearables.empty();
    if (!c.loaded && c.error.empty())
        c.error = "no wearables in " + (dir / "items.json").string();
    return c;
}

const Catalog& catalog()
{
    static const Catalog c = build();
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
    for (const auto& item : catalog().wearables)
        if (item.id == id)
            return &item;
    return nullptr;
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
    std::string work = workLabel;
    std::transform(work.begin(), work.end(), work.begin(), [](unsigned char ch) { return char(std::tolower(ch)); });
    for (const auto& b : catalog().businesses)
        for (const auto& m : b.match)
            if (!m.empty() && work.find(m) != std::string::npos)
                return &b;
    return nullptr;
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
    for (const auto& item : catalog().goods)
        if (item.id == id)
            return &item;
    return nullptr;
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
