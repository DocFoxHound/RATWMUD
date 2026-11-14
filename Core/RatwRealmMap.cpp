// The realm's map, whole (RatwRealmMap.h).
#include "RatwRealmMap.h"
#include "RatwJsonDoc.h"
#include "RatwWorld.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <map>
#include <set>

namespace ratw::realm
{
namespace
{
std::string lower(std::string s)
{
    for (auto& c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
bool wallAt(const std::vector<std::string>& rows, int x, int y)
{
    if (y < 0 || y >= int(rows.size()) || x < 0 || x >= int(rows[y].size()))
        return false;
    const auto* info = terrainInfo(rows[y][x]);
    return info && info->kind == Terrain::Wall;
}
bool openAt(const std::vector<std::string>& rows, int x, int y)
{
    if (y < 0 || y >= int(rows.size()) || x < 0 || x >= int(rows[y].size()))
        return false;
    const auto* info = terrainInfo(rows[y][x]);
    return info && !info->solid;
}
// A place's name without its floor ("Rope End, the ground floor" is Rope End).
std::string placeName(const std::string& name)
{
    const auto comma = name.find(", ");
    return comma == std::string::npos ? name : name.substr(0, comma);
}
std::string firstUpper(std::string s)
{
    if (!s.empty())
        s[0] = char(std::toupper(static_cast<unsigned char>(s[0])));
    return s;
}
std::string townWords(const std::string& id)
{
    std::string out;
    bool start = true;
    for (char c : id)
    {
        if (c == '_' || c == '-')
        {
            out += ' ';
            start = true;
            continue;
        }
        out += start ? char(std::toupper(static_cast<unsigned char>(c))) : c;
        start = false;
    }
    return out;
}
// "North-west", "Middle", "East": a part of a region named only by where it lies, which says nothing on a map.
bool compassWords(const std::string& words)
{
    static const std::set<std::string> plain{"north", "south", "east", "west", "middle", "centre", "center", "central",
                                             "north-east", "north-west", "south-east", "south-west", "northeast",
                                             "northwest", "southeast", "southwest", "the north", "the south", "the east",
                                             "the west", "the middle"};
    return plain.count(lower(words)) != 0;
}
double tenth(double v) { return std::round(v * 10) / 10; }
} // namespace

std::vector<std::string> gridRows(const std::string& body)
{
    std::vector<std::string> rows;
    std::size_t at = 0;
    bool inGrid = false;
    while (at <= body.size())
    {
        auto end = body.find('\n', at);
        if (end == std::string::npos)
            end = body.size();
        std::string line = body.substr(at, end - at);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (inGrid)
        {
            if (!line.empty())
                rows.push_back(std::move(line));
        }
        else if (line == "grid:")
            inGrid = true;
        at = end + 1;
    }
    return rows;
}

std::string kindOf(const std::string& text)
{
    const std::string w = " " + lower(text) + " ";
    const auto any = [&](std::initializer_list<const char*> words) {
        for (const char* word : words)
            if (w.find(word) != std::string::npos)
                return true;
        return false;
    };
    // Most telling first: a work label says more than a name ("cooking for Northgate" at The Grey Goose).
    if (any({"chapel", "church", "cathedral", "abbey", "temple", "shrine", "priest"}))
        return "church";
    if (any({" watch", "barracks", "gatehouse", "guard", "gaol"}))
        return "watch";
    if (any({" inn ", " inn,", " inn'", "tavern", "taproom", "alehouse", "innkeep", "cooking for", "landlord", "sailors' rest"}))
        return "inn";
    if (any({"bank", "moneychanger", "counting", "pawn"}))
        return "bank";
    if (any({"baker", "bakehouse", "loaf", "oven", "crust"}))
        return "bakery";
    if (any({"butcher", "cleaver", "slaughter", "smokehouse"}))
        return "butcher";
    if (any({"smith", "forge", "ironmonger", "armourer", "armorer", "foundry", "ironworks", "nailer", "anvil", "rivet"}))
        return "smith";
    if (any({"tailor", "weaver", "loom", "linen", "needle", "draper", "wool", "clothier"}))
        return "tailor";
    if (any({"apothecar", "physic", "herbal", "mortar", "phial", "infirmary", "mending", "healer", "barber"}))
        return "apothecary";
    if (any({"stable", "carter", "wheelwright", "farrier", "courier"}))
        return "stables";
    if (any({"wharf", "dock", "boat", "shipwright", "harbour", "harbor", "pier", "ferry"}))
        return "wharf";
    if (any({"market hall", "market"}))
        return "market";
    if (any({"brewer", "brewery", "brewhouse", "distill", " still", " wine", "vault", "cellar", "hopyard", "tun "}))
        return "brewery";
    if (any({"store", "provision", "shopkeep", "keeping shop", " shop ", "sundries", "chandler", "grocer", "general", "jewel", "silver"}))
        return "store";
    if (any({"farmhouse", "dairy", "byre", "barn", "granary", "pig yard", "rabbitry", "hive", "orchard", "apple",
             "farm"}))
        return "farm";
    if (any({"works", "workshop", "mill", "joiner", "carpenter", "cooper", "potter", "glass", "tannery", "rope",
             "kiln", "mason", "sawmill", "press", "printer", "scriven", "yard", "shed", "salt"}))
        return "workshop";
    if (any({"office", "hall", "residence", "council", "records", "school", "scriptorium", "clock tower",
             "magistrate", "wing", "refectory"}))
        return "hall";
    if (any({"house", "cottage", "bunkhouse", "rooms", "rents", " row", "lodging", "close", "terrace", "corner", "court",
             "buildings", "gables", "end"}))
        return "house";
    return "";
}

void footprintMiddle(const std::vector<std::string>& rows, double doorX, double doorY, int innerWidth, int innerHeight,
                     double& outX, double& outY)
{
    outX = doorX;
    outY = doorY;
    const int dx = int(std::floor(doorX)), dy = int(std::floor(doorY));
    // Inward: the side of the door with walls (the building), across from open ground (the street).
    static const int Steps[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
    int in = -1;
    for (int i = 0; i < 4 && in < 0; ++i)
        if (wallAt(rows, dx + Steps[i][0], dy + Steps[i][1]) && openAt(rows, dx - Steps[i][0], dy - Steps[i][1]))
            in = i;
    for (int i = 0; i < 4 && in < 0; ++i)
        if (wallAt(rows, dx + Steps[i][0], dy + Steps[i][1]))
            in = i;
    const int depth = std::max(2, std::max(innerWidth, innerHeight));
    if (in < 0)
        return;                                    // No walls beside it: the door alone marks the place.
    const int ix = Steps[in][0], iy = Steps[in][1];
    // A step in by half the interior's depth along the way in: what is left when the walls can't say.
    const int along = ix != 0 ? innerWidth : innerHeight;
    outX = dx + .5 + ix * std::max(1.0, along / 2.0);
    outY = dy + .5 + iy * std::max(1.0, along / 2.0);
    // The block of walls and roofs the door opens into, within the interior's reach of the door.
    const int reach = depth + 2;
    const std::size_t most = std::size_t(std::max(1, innerWidth)) * std::size_t(std::max(1, innerHeight)) * 3 + 16;
    std::set<std::pair<int, int>> seen;
    std::vector<std::pair<int, int>> todo{{dx + ix, dy + iy}};
    seen.insert(todo.front());
    int x0 = todo.front().first, x1 = x0, y0 = todo.front().second, y1 = y0;
    while (!todo.empty())
    {
        const auto [x, y] = todo.back();
        todo.pop_back();
        x0 = std::min(x0, x), x1 = std::max(x1, x), y0 = std::min(y0, y), y1 = std::max(y1, y);
        if (seen.size() > most)
            return;                                // A terrace's whole row of roofs: the step in says more.
        for (const auto& s : Steps)
        {
            const int nx = x + s[0], ny = y + s[1];
            if (std::abs(nx - dx) > reach || std::abs(ny - dy) > reach || !wallAt(rows, nx, ny) || !seen.insert({nx, ny}).second)
                continue;
            todo.push_back({nx, ny});
        }
    }
    outX = (x0 + x1 + 1) / 2.0;
    outY = (y0 + y1 + 1) / 2.0;
}

std::string build(Input input, const Fetch& fetch, const std::function<bool()>& stop, int maxSide)
{
    using json::Value;
    auto event = Value::object();
    event.add("type", "realmMap");
    event.add("build", input.build);
    if (input.cells.empty())
    {
        event.add("x0", 0), event.add("y0", 0), event.add("step", 1), event.add("cols", 0), event.add("rows", 0);
        event.add("ground", ""), event.add("labels", Value::array()), event.add("buildings", Value::array());
        return json::dump(event);
    }
    double minX = 1e18, minY = 1e18, maxX = -1e18, maxY = -1e18;
    for (const auto& c : input.cells)
        minX = std::min(minX, c.x), minY = std::min(minY, c.y), maxX = std::max(maxX, c.x + c.width),
        maxY = std::max(maxY, c.y + c.height);
    const int x0 = int(std::floor(minX)), y0 = int(std::floor(minY));
    const int width = std::max(1, int(std::ceil(maxX)) - x0), height = std::max(1, int(std::ceil(maxY)) - y0);
    const int step = std::max(1, int(std::ceil(double(std::max(width, height)) / std::max(16, maxSide))));
    const int cols = (width + step - 1) / step, rowCount = (height + step - 1) / step;
    std::string ground(std::size_t(cols) * std::size_t(rowCount), ' ');
    std::vector<std::uint16_t> covered(ground.size(), 0);
    std::map<std::string, const std::vector<std::string>*> rowsOf;
    std::map<std::string, std::vector<std::string>> fetched;
    std::map<std::string, std::vector<const DoorIn*>> doorsIn;
    for (const auto& d : input.doors)
        doorsIn[d.cell].push_back(&d);
    struct Building
    {
        const DoorIn* door;
        double x, y;
        std::string kind;
    };
    std::vector<Building> buildings;
    std::set<std::string> placed;
    for (auto& c : input.cells)
    {
        if (stop && stop())
            return std::string();
        const std::vector<std::string>* rows = &c.rows;
        if (c.rows.empty() && fetch)
        {
            std::string body;
            if (fetch(c.id, body))
                rows = &(fetched[c.id] = gridRows(body));
        }
        // The ground, a sample a block of step x step tiles: the commonest glyph in it, walls (buildings) and roads
        // weighed thrice and water half again, so a town and its ways still show when the map is small.
        std::array<float, 128> weight{};
        const int sx0 = std::max(0, int(std::floor((c.x - x0) / step))), sy0 = std::max(0, int(std::floor((c.y - y0) / step)));
        const int sx1 = std::min(cols - 1, int(std::floor((c.x + c.width - 1 - x0) / step)));
        const int sy1 = std::min(rowCount - 1, int(std::floor((c.y + c.height - 1 - y0) / step)));
        for (int sy = sy0; sy <= sy1; ++sy)
            for (int sx = sx0; sx <= sx1; ++sx)
            {
                weight.fill(0);
                int count = 0;
                const int bx = x0 + sx * step - int(std::floor(c.x)), by = y0 + sy * step - int(std::floor(c.y));
                for (int y = std::max(0, by); y < std::min(c.height, by + step) && y < int(rows->size()); ++y)
                {
                    const auto& row = (*rows)[y];
                    for (int x = std::max(0, bx); x < std::min(c.width, bx + step) && x < int(row.size()); ++x)
                    {
                        const auto code = static_cast<unsigned char>(row[x]);
                        if (code >= 128 || code == ' ')
                            continue;
                        const auto* info = terrainInfo(char(code));
                        weight[code] += !info ? 1.f : info->kind == Terrain::Wall || info->way ? 3.f : info->kind == Terrain::Water ? 1.5f : 1.f;
                        ++count;
                    }
                }
                const std::size_t at = std::size_t(sy) * cols + sx;
                if (count == 0 || count < covered[at])
                    continue;
                covered[at] = std::uint16_t(std::min(count, 65535));
                ground[at] = char(std::max_element(weight.begin(), weight.end()) - weight.begin());
            }
        for (const DoorIn* d : doorsIn[c.id])
        {
            if (!placed.insert(d->interior).second)
                continue;                          // (A back door: the building is placed by its first.)
            double mx = d->x, my = d->y;
            footprintMiddle(*rows, d->x, d->y, d->innerWidth, d->innerHeight, mx, my);
            // What it is by its own name first (the Market Hall, with a butcher and a baker in it, is a market), and
            // by its workers' words where the name says little ("The Grey Goose": "cooking for Northgate").
            auto kind = kindOf(placeName(d->name));
            if (kind.empty() || kind == "house" || kind == "workshop")
                if (auto worked = kindOf(d->words); !worked.empty())
                    kind = worked;
            buildings.push_back({d, c.x + mx, c.y + my, kind});
        }
        fetched.erase(c.id);                       // (Each cell's rows only while it is worked on.)
    }
    event.add("x0", x0);
    event.add("y0", y0);
    event.add("step", step);
    event.add("cols", cols);
    event.add("rows", rowCount);
    event.add("ground", ground);
    // The names, larger places first (rank 0 shows on the whole map, 2 only close in).
    auto labels = Value::array();
    const auto label = [&](const std::string& name, const char* kind, int rank, double x, double y) {
        if (name.empty())
            return;
        auto l = Value::object();
        l.add("n", name);
        l.add("k", kind);
        l.add("r", rank);
        l.add("x", tenth(x));
        l.add("y", tenth(y));
        labels.push(std::move(l));
    };
    // Towns: their buildings' middle (the median, so a farm far out in its fields doesn't pull it off), named for the
    // town's one cell where it has one ("Greyfen Crossing"), else for the town.
    std::map<std::string, std::vector<const Building*>> towns;
    for (const auto& b : buildings)
        if (!b.door->town.empty() && b.door->town != "unassigned")
            towns[b.door->town].push_back(&b);
    std::set<std::string> townNames;
    for (const auto& [town, list] : towns)
    {
        std::vector<double> xs, ys;
        for (const auto* b : list)
            xs.push_back(b->x), ys.push_back(b->y);
        std::nth_element(xs.begin(), xs.begin() + xs.size() / 2, xs.end());
        std::nth_element(ys.begin(), ys.begin() + ys.size() / 2, ys.end());
        std::string name = townWords(town);
        int own = 0;
        const CellIn* only = nullptr;
        for (const auto& c : input.cells)
            if (c.region == town)
                ++own, only = &c;
        if (own == 1 && only->name.find(", ") == std::string::npos)
            name = only->name;
        townNames.insert(lower(name));
        label(name, list.size() >= 150 ? "city" : "town", list.size() >= 150 ? 0 : 1, xs[xs.size() / 2], ys[ys.size() / 2]);
    }
    // Regions: the cells sharing one, named for the words their names share ("The Larkdowns, East").
    std::map<std::string, std::vector<const CellIn*>> regions;
    for (const auto& c : input.cells)
        if (!c.region.empty() && c.region != "unassigned")
            regions[c.region].push_back(&c);
    for (const auto& [region, cells] : regions)
    {
        if (towns.count(region))
            continue;                              // (A town's own cells: the town's name says it.)
        std::map<std::string, int> prefixes;
        double sx = 0, sy = 0;
        for (const auto* c : cells)
        {
            sx += c->x + c->width / 2.0, sy += c->y + c->height / 2.0;
            if (const auto comma = c->name.find(", "); comma != std::string::npos)
                ++prefixes[c->name.substr(0, comma)];
        }
        const auto best = std::max_element(prefixes.begin(), prefixes.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
        if (best != prefixes.end() && !townNames.count(lower(best->first)))
            label(best->first, "region", 0, sx / cells.size(), sy / cells.size());
    }
    // Districts and the places within a region: the rest of a cell's name ("Northgate", "the Upper Mere"), or the
    // whole of a name with nothing to share ("Fellhall"); one label for cells of one name in one region.
    // (A name a town already has is the town's; cells of one name near each other, a lake across a seam, are one place.)
    struct District
    {
        std::string name;
        double x = 0, y = 0;
        int cells = 0;
    };
    std::vector<District> districts;
    for (const auto& c : input.cells)
    {
        const auto comma = c.name.find(", ");
        const std::string words = firstUpper(comma == std::string::npos ? c.name : c.name.substr(comma + 2));
        if (words.empty() || compassWords(words) || townNames.count(lower(words)))
            continue;
        const double mx = c.x + c.width / 2.0, my = c.y + c.height / 2.0;
        auto same = std::find_if(districts.begin(), districts.end(), [&](const District& d) {
            return d.name == words && std::hypot(d.x / d.cells - mx, d.y / d.cells - my) <= 3 * std::max(c.width, c.height);
        });
        if (same == districts.end())
            districts.push_back({words, mx, my, 1});
        else
            same->x += mx, same->y += my, ++same->cells;
    }
    for (const auto& d : districts)
        label(d.name, "district", 2, d.x / d.cells, d.y / d.cells);
    event.add("labels", labels);
    auto list = Value::array();
    for (const auto& b : buildings)
    {
        auto j = Value::object();
        j.add("n", placeName(b.door->name));
        if (!b.kind.empty())
            j.add("k", b.kind);
        j.add("x", tenth(b.x));
        j.add("y", tenth(b.y));
        list.push(std::move(j));
    }
    event.add("buildings", list);
    return json::dump(event);
}
} // namespace ratw::realm
