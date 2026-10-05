#include "RatwWild.h"

#include "RatwJsonDoc.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace ratw::wild
{
namespace
{
struct Data
{
    bool loaded = false;
    std::string error;
    std::vector<Species> species;
    Population population;
    Forage forage;
    std::map<char, std::string> ground;
};

// Data/Wild: RATW_DATA_DIR (the Data directory), else the working directory or one above it, else the source tree.
std::filesystem::path wildDir()
{
    namespace fs = std::filesystem;
    std::error_code ec;
    if (const char* dir = std::getenv("RATW_DATA_DIR"); dir && *dir)
        return fs::path(dir) / "Wild";
    for (auto at = fs::current_path(ec); !ec && !at.empty(); at = at.parent_path())
    {
        if (fs::exists(at / "Data" / "Wild" / "animals.json", ec))
            return at / "Data" / "Wild";
        if (at == at.parent_path())
            break;
    }
#ifdef RATW_SOURCE_DIR
    return fs::path(RATW_SOURCE_DIR) / "Data" / "Wild";
#else
    return fs::path("Data") / "Wild";
#endif
}

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

std::vector<int> seasonsOf(const json::Value& v)
{
    std::vector<int> out;
    for (const auto& s : v.array("seasons"))
        if (const int n = int(s.asNumber(-1)); n >= 0 && n <= 3)
            out.push_back(n);
    return out;
}

Data build()
{
    Data d;
    json::Value animals, forage;
    const auto dir = wildDir();
    if (!readJson(dir / "animals.json", animals, d.error) || !readJson(dir / "forage.json", forage, d.error))
        return d;
    for (const auto& [kind, tiles] : animals.object("ground").fields())
        for (char c : tiles.asString({}))
            d.ground.emplace(c, kind);
    for (const auto& a : animals.array("species"))
    {
        Species s;
        s.id = a.string("id");
        s.name = a.string("name");
        s.glyph = a.string("glyph");
        s.color = a.string("color");
        s.temper = a.string("temper", "flee");
        for (const auto& [kind, w] : a.object("habitats").fields())
            if (w.asNumber(0) > 0)
                s.habitats[kind] = w.asNumber(0);
        s.rarity = a.number("rarity", 1);
        s.health = a.number("health", 10);
        s.dex = a.number("dex", 50);
        s.strength = a.number("strength", 30);
        s.alert = a.number("alert", 8);
        s.sight = a.number("sight", 1);
        s.hearing = a.number("hearing", 1);
        s.smell = a.number("smell", 1);
        s.bite = a.number("bite", 0);
        for (const auto& [item, n] : a.object("yield").fields())
            if (n.asNumber(0) >= 1)
                s.yield.push_back({item, int(n.asNumber(0))});
        if (!s.id.empty() && !s.name.empty() && s.health > 0 && !s.habitats.empty())
            d.species.push_back(std::move(s));
    }
    const auto& p = animals.object("population");
    d.population.expected = p.number("expected", 3);
    d.population.capacity = std::max(1.0, p.number("capacity", 6));
    d.population.recoveryDays = std::max(.01, p.number("recoveryDays", 2));
    d.population.perPlayers = std::max(1.0, p.number("perPlayers", 50));
    d.population.most = std::max(1.0, p.number("most", 2));
    d.population.arrivalSeconds = std::max(1.0, p.number("arrivalSeconds", 30));
    d.population.atOnce = std::max(1, int(p.number("atOnce", 6)));
    d.population.wildFrom = p.number("wildFrom", .5);
    d.population.nearestStart = std::max(1, int(p.number("nearestStart", 10)));
    d.forage.patchTiles = std::max(1, int(forage.number("patchTiles", 8)));
    d.forage.picks = std::max(1, int(forage.number("picks", 4)));
    d.forage.regrowHours = std::max(.01, forage.number("regrowHours", 2));
    d.forage.seconds = std::max(0.0, forage.number("seconds", 4));
    for (const auto& g : forage.array("ground"))
    {
        ForageGround ground;
        ground.id = g.string("id");
        ground.tiles = g.string("tiles");
        ground.name = g.string("name");
        for (const auto& x : g.array("goods"))
        {
            ForageGood good;
            good.item = x.string("item");
            good.weight = x.number("weight", 1);
            good.count = std::max(1, int(x.number("count", 1)));
            good.seasons = seasonsOf(x);
            if (!good.item.empty() && good.weight > 0)
                ground.goods.push_back(std::move(good));
        }
        if (!ground.id.empty() && !ground.tiles.empty() && !ground.goods.empty())
            d.forage.ground.push_back(std::move(ground));
    }
    d.loaded = !d.species.empty();
    if (!d.loaded && d.error.empty())
        d.error = "no animals in " + (dir / "animals.json").string();
    return d;
}

const Data& data()
{
    static const Data d = build();
    return d;
}
} // namespace

bool load(std::string* error)
{
    if (error)
        *error = data().error;
    return data().loaded;
}

const std::vector<Species>& species()
{
    return data().species;
}

const Species* speciesById(const std::string& id)
{
    for (const auto& s : data().species)
        if (s.id == id)
            return &s;
    return nullptr;
}

const Population& population()
{
    return data().population;
}

const Forage& forage()
{
    return data().forage;
}

const std::string& groundOf(char tile)
{
    static const std::string none;
    const auto found = data().ground.find(tile);
    return found == data().ground.end() ? none : found->second;
}
} // namespace ratw::wild
