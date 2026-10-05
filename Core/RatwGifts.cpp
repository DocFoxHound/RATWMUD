#include "RatwGifts.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>

namespace ratw::gifts
{
namespace
{
struct Catalog
{
    bool loaded = false;
    std::string error;
    std::map<std::string, std::string> names;       // Every family, by id.
    std::map<std::string, bool> npcOnly;
    std::vector<Ability> abilities;                 // Every ability, in catalog order.
    json::Value creator = json::Value::object();
};

// Data/Gifts: RATW_DATA_DIR (the Data directory), else the working directory or one above it, else the source tree.
std::filesystem::path giftsFile()
{
    namespace fs = std::filesystem;
    std::error_code ec;
    if (const char* dir = std::getenv("RATW_DATA_DIR"); dir && *dir)
        return fs::path(dir) / "Gifts" / "families.json";
    for (auto at = fs::current_path(ec); !ec && !at.empty(); at = at.parent_path())
    {
        if (fs::exists(at / "Data" / "Gifts" / "families.json", ec))
            return at / "Data" / "Gifts" / "families.json";
        if (at == at.parent_path())
            break;
    }
#ifdef RATW_SOURCE_DIR
    return fs::path(RATW_SOURCE_DIR) / "Data" / "Gifts" / "families.json";
#else
    return fs::path("Data") / "Gifts" / "families.json";
#endif
}

Catalog build()
{
    Catalog c;
    const auto path = giftsFile();
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        c.error = "cannot read " + path.string();
        return c;
    }
    std::stringstream text;
    text << in.rdbuf();
    json::Value doc;
    if (!json::parse(text.str(), doc, c.error))
    {
        c.error = path.string() + ": " + c.error;
        return c;
    }
    auto families = json::Value::array();
    for (const auto& f : doc.array("families"))
    {
        const auto id = f.string("id");
        if (id.empty() || f.string("name").empty() || !f.object("gifted").isObject() || !f.object("quickened").isObject())
        {
            c.error = path.string() + ": a family needs an id, a name and both tiers";
            return c;
        }
        c.names[id] = f.string("name");
        for (const char* tier : {"gifted", "quickened"})
            for (const auto& a : f.object(tier).array("abilities"))
            {
                Ability ab;
                ab.id = a.string("id");
                ab.name = a.string("name");
                ab.family = id;
                ab.kind = a.string("kind");
                ab.summary = a.string("summary");
                ab.quickened = std::string(tier) == "quickened";
                ab.work = ab.kind == "work" || a.boolean("work");
                ab.mana = a.number("mana");
                ab.perTurn = a.number("perTurn");
                ab.perTile = a.number("perTile");
                if (!ab.id.empty())
                    c.abilities.push_back(std::move(ab));
            }
        c.npcOnly[id] = f.boolean("npcOnly");
        if (!f.boolean("npcOnly"))
            families.push(f);
    }
    if (c.names.empty())
    {
        c.error = path.string() + ": no families";
        return c;
    }
    c.creator.add("tiers", doc.object("tiers"));
    c.creator.add("families", families);
    c.loaded = true;
    return c;
}

const Catalog& catalog()
{
    static std::once_flag once;
    static Catalog c;
    std::call_once(once, [] { c = build(); });
    return c;
}
}

bool load(std::string* error)
{
    const auto& c = catalog();
    if (error)
        *error = c.error;
    return c.loaded;
}

bool known(const std::string& family)
{
    return catalog().names.count(family) > 0;
}

bool playable(const std::string& family)
{
    const auto& c = catalog();
    const auto it = c.npcOnly.find(family);
    return it != c.npcOnly.end() && !it->second;
}

std::string name(const std::string& family)
{
    const auto& c = catalog();
    const auto it = c.names.find(family);
    return it == c.names.end() ? family : it->second;
}

const json::Value& creatorCatalog()
{
    return catalog().creator;
}

const Ability* ability(const std::string& id)
{
    for (const auto& a : catalog().abilities)
        if (a.id == id)
            return &a;
    return nullptr;
}

std::vector<const Ability*> abilities(const std::string& family, bool quickened)
{
    std::vector<const Ability*> out;
    for (const auto& a : catalog().abilities)
        if (a.family == family && a.quickened == quickened)
            out.push_back(&a);
    return out;
}
}
