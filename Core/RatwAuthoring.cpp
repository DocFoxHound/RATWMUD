#include "RatwWorld.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <set>
#include <sstream>
#include <tuple>

namespace ratw
{
namespace
{
namespace fs = std::filesystem;
constexpr std::size_t MaxCells = 256, MaxDoors = 65536, MaxTiles = 262144;
// A streamed world (RATW_WORLD 3) names its cells with "area" records and loads their tiles on demand.
constexpr std::size_t MaxAreas = 4000000;
bool identifier(const std::string& text, std::size_t limit)
{
    return !text.empty() && text.size() <= limit && text.front() >= 'a' && text.front() <= 'z' &&
           std::all_of(text.begin(), text.end(),
                       [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'; });
}
bool displayName(const std::string& value)
{
    std::size_t index = 0, count = 0; bool visible = false;
    while (index < value.size())
    {
        const auto lead = static_cast<unsigned char>(value[index++]);
        unsigned cp = lead, minimum = 0; int extra = 0;
        if (lead >= 0xc2 && lead <= 0xdf) { cp = lead & 0x1f; extra = 1; minimum = 0x80; }
        else if (lead >= 0xe0 && lead <= 0xef) { cp = lead & 0x0f; extra = 2; minimum = 0x800; }
        else if (lead >= 0xf0 && lead <= 0xf4) { cp = lead & 0x07; extra = 3; minimum = 0x10000; }
        else if (lead >= 0x80) return false;
        for (int n = 0; n < extra; ++n)
        {
            if (index >= value.size()) return false;
            const auto next = static_cast<unsigned char>(value[index++]);
            if ((next & 0xc0) != 0x80) return false;
            cp = (cp << 6) | (next & 0x3f);
        }
        if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff) || cp < 32 || cp == 127 || ++count > 120) return false;
        const bool space = cp == 32 || cp == 0x85 || cp == 0xa0 || cp == 0x1680 ||
            (cp >= 0x2000 && cp <= 0x200a) || cp == 0x2028 || cp == 0x2029 || cp == 0x202f ||
            cp == 0x205f || cp == 0x3000 || cp == 0xfeff;
        visible |= !space;
    }
    return visible;
}
bool quoted(std::istringstream& input, std::string& value)
{
    input >> std::ws;
    if (input.peek() != '"')
        return false;
    input >> std::quoted(value);
    return bool(input) &&
           std::none_of(value.begin(), value.end(), [](unsigned char ch) { return ch < 32 || ch == 127; });
}
bool end(std::istringstream& input)
{
    return bool(input) && (input >> std::ws).eof();
}
bool flag(int value)
{
    return value == 0 || value == 1;
}
bool center(const Cell& cell, Vec2 p)
{
    return std::isfinite(p.x) && std::isfinite(p.y) && p.x >= .5 && p.y >= .5 && p.x <= cell.width - .5 &&
           p.y <= cell.height - .5 && std::abs(p.x - std::floor(p.x) - .5) < 1e-8 &&
           std::abs(p.y - std::floor(p.y) - .5) < 1e-8;
}
bool near(double a, double b)
{
    return std::abs(a - b) < 1e-8;
}
bool same(Vec2 a, Vec2 b)
{
    return near(a.x, b.x) && near(a.y, b.y);
}
char opposite(char edge)
{
    switch (edge)
    {
    case 'N':
        return 'S';
    case 'S':
        return 'N';
    case 'E':
        return 'W';
    case 'W':
        return 'E';
    default:
        return '-';
    }
}
bool onEdge(const Cell& cell, const Door& door)
{
    return door.edge == 'N'   ? near(door.position.y, .5)
           : door.edge == 'S' ? near(door.position.y, cell.height - .5)
           : door.edge == 'W' ? near(door.position.x, .5)
           : door.edge == 'E' ? near(door.position.x, cell.width - .5)
                              : false;
}
// One "door" record after its keyword, as the manifest (and a streamed cell's seams) write it.
bool parseDoor(std::istringstream& fields, Door& door)
{
    int open = -1, locked = -1, boundary = -1, passage = -1;
    std::string edge;
    if (!quoted(fields, door.id) || !quoted(fields, door.name) || !quoted(fields, door.cellId))
        return false;
    fields >> door.position.x >> door.position.y;
    if (!quoted(fields, door.targetCell))
        return false;
    fields >> door.arrival.x >> door.arrival.y;
    if (!quoted(fields, door.linkedDoor))
        return false;
    fields >> open >> locked >> boundary >> passage;
    if (!quoted(fields, edge) || !end(fields) || !identifier(door.id, 160) || !identifier(door.cellId, 48) ||
        !identifier(door.targetCell, 48) || !identifier(door.linkedDoor, 160) || door.name.empty() ||
        door.name.size() > 512 || !flag(open) || !flag(locked) || !flag(boundary) || !flag(passage) ||
        edge.size() != 1 || std::string("NESW-").find(edge[0]) == std::string::npos)
        return false;
    door.open = open == 1;
    door.locked = locked == 1;
    door.boundary = boundary == 1;
    door.passage = passage == 1;
    door.edge = edge[0];
    door.portal = true;
    return true;
}
bool contained(const fs::path& root, const fs::path& path)
{
    auto filePart = path.begin();
    for (auto rootPart = root.begin(); rootPart != root.end(); ++rootPart, ++filePart)
        if (filePart == path.end() || *rootPart != *filePart)
            return false;
    return filePart != path.end();
}
} // namespace

Result World::ensureLoaded(const std::string& cellId)
{
    auto found = cells_.find(cellId);
    if (found == cells_.end())
        return {false, "Unknown cell.", cellId};
    if (found->second.loaded)
        return {true, "Cell already loaded.", cellId};
    if (!source_.load)
        return {false, "No source for streamed cells.", cellId};
    std::string text, seams;
    const auto problem = source_.load(cellId, text, seams);
    if (!problem.empty())
        return {false, problem, cellId};
    Cell full;
    std::istringstream input(text);
    const auto read = parseCell(input, cellId, full, false);
    if (!read.ok)
        return read;
    auto& area = found->second;
    if (full.id != cellId || full.width != area.width || full.height != area.height)
        return {false, "A streamed cell disagrees with its header.", cellId};
    // Seams: this cell's side of each open boundary, checked against its own tiles and the far cell's header.
    std::vector<Door> sides;
    std::istringstream lines(seams);
    std::string line;
    while (std::getline(lines, line))
    {
        if (line.empty())
            continue;
        std::istringstream fields(line);
        std::string command;
        fields >> command;
        Door d;
        if (command != "door" || !parseDoor(fields, d) || d.cellId != cellId || !d.passage || !d.boundary || !d.open ||
            d.locked || doors_.count(d.id))
            return {false, "Invalid seam in a streamed cell.", cellId};
        const auto* far = cell(d.targetCell);
        const auto* tile = full.tile(int(d.position.x), int(d.position.y));
        if (!far || d.targetCell == cellId || !center(full, d.position) || !center(*far, d.arrival) || !onEdge(full, d) ||
            !tile || tile->solid)
            return {false, "A seam in a streamed cell has a bad anchor: " + d.id, cellId};
        sides.push_back(std::move(d));
    }
    // The header's live state (weather, wind, light, territory) was kept up while the tiles were away.
    area.tiles = std::move(full.tiles);
    area.loaded = true;
    seamAnchors_[cellId] = sides;                   // Kept after the cell unloads (see moveOffstage()).
    std::vector<std::string> seamIds;
    for (auto& d : sides)
    {
        seamIds.push_back(d.id);
        const auto id = d.id;
        doors_.emplace(id, std::move(d));
    }
    indexSeams(cellId, seamIds);
    if (homesReady_ && !area.outdoors && !homeStoreSpots_.count(cellId))
    {
        placeHomeStores(cellId);                    // A home's stores and beds once its interior is in memory (doc 36).
        placeBeds(cellId);
        society_.setHomeStores(homeStoreSpots_);
        society_.setBeds(beds_);
    }
    return {true, "Cell loaded.", cellId};
}

Result World::loadWorldFile(const std::string& path)
{
    std::error_code error;
    const fs::path manifest = fs::canonical(path, error);
    if (error || !fs::is_regular_file(manifest, error) || error)
        return {false, "Cannot open authored world manifest.", path};
    const auto bytes = fs::file_size(manifest, error);
    if (error || bytes > 32 * 1024 * 1024)
        return {false, "Authored world manifest is too large.", path};
    const fs::path root = manifest.parent_path();
    std::ifstream input(manifest);
    if (!input)
        return {false, "Cannot read authored world manifest.", path};
    std::set<fs::path> seen;
    return loadWorld(input, [&](const std::string& relative, std::string& text) -> std::string {
        std::error_code failure;
        const fs::path cellPath = fs::canonical(root / fs::path(relative), failure);
        if (failure || !contained(root, cellPath) || !fs::is_regular_file(cellPath, failure) || failure ||
            !seen.insert(cellPath).second)
            return "Cell file is missing, reused, or escapes the world directory.";
        const auto size = fs::file_size(cellPath, failure);
        std::ifstream cell(cellPath, std::ios::binary);
        if (failure || size > 4 * 1024 * 1024 || !cell)
            return "Missing or oversized cell file.";
        text.assign(std::istreambuf_iterator<char>(cell), std::istreambuf_iterator<char>());
        return "";
    }, path);
}

Result World::loadWorldFiles(const std::map<std::string, std::string>& files, const std::string& label)
{
    const auto manifest = files.find("world.ratw");
    if (manifest == files.end())
        return {false, "The build has no world.ratw manifest.", label};
    if (manifest->second.size() > 32 * 1024 * 1024)
        return {false, "Authored world manifest is too large.", label};
    std::set<std::string> seen;
    std::istringstream input(manifest->second);
    return loadWorld(input, [&](const std::string& relative, std::string& text) -> std::string {
        const auto found = files.find(relative);
        if (found == files.end() || !seen.insert(relative).second)
            return "Cell file is missing or reused.";
        text = found->second;
        return "";
    }, label);
}

Result World::loadWorld(std::istream& input, const CellReader& readCell, const std::string& path)
{

    // Everything is parsed and validated off to the side. A rejection never
    // mutates the running world's maps, entities, remembered views, or clock.
    World candidate;
    candidate.cells_.clear();
    candidate.factions_.clear();
    candidate.chapters_.clear();
    candidate.entities_.clear();
    candidate.index_.dirty = true;
    candidate.doors_.clear();
    candidate.blockingFixtures_.clear();
    candidate.spawnCell_.clear();
    candidate.customWorld_ = true;
    candidate.source_ = source_;
    candidate.tiered_ = tiered_;                    // A tier setting made before loading still holds.
    candidate.tieredSet_ = tieredSet_;
    candidate.society_.reset(false);
    candidate.herbCell_.clear();
    AuthoredRoster roster;
    std::set<std::string> residentIds;             // roster.residents by ID, to refuse a duplicate at once.
    bool economy = false, herbs = false;
    std::vector<std::pair<std::string, Spot>> places;
    std::size_t cellFiles = 0;
    std::map<std::string, std::tuple<std::string, std::string, std::vector<std::string>>> territories;
    std::map<std::string, std::vector<std::string>> liveClaims;
    std::size_t totalTiles = 0, lineNumber = 0;
    bool header = false, spawn = false;
    std::string line;
    auto reject = [&](const std::string& reason) -> Result {
        return {false, "World manifest line " + std::to_string(lineNumber) + ": " + reason, path};
    };
    while (std::getline(input, line))
    {
        ++lineNumber;
        if (line.size() > 16384)
            return reject("Line is too long.");
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;
        for (unsigned char ch : line)
            if ((ch < 32 && ch != '\t') || ch == 127)
                return reject("Control character in manifest.");
        std::istringstream fields(line);
        std::string command;
        fields >> command;
        if (!header)
        {
            int version = 0;
            fields >> version;
            if (command != "RATW_WORLD" || version < 1 || version > 3 || !end(fields))
                return reject("Expected RATW_WORLD 1, 2 or 3.");
            header = true;
            continue;
        }
        if (command == "faction")
        {
            FactionDefinition faction;
            if (!quoted(fields, faction.id) || !quoted(fields, faction.name) || !quoted(fields, faction.color) ||
                !end(fields) || !identifier(faction.id, 48) || !displayName(faction.name) ||
                faction.color.size() != 7 || faction.color.front() != '#' ||
                !std::all_of(faction.color.begin() + 1, faction.color.end(), [](char c) {
                    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }) ||
                candidate.factions_.size() >= 64 || candidate.factions_.count(faction.id))
                return reject("Invalid or duplicate faction definition.");
            candidate.factions_.emplace(faction.id, faction);
        }
        else if (command == "chapter")
        {
            ChapterDefinition chapter;
            if (!quoted(fields, chapter.id) || !quoted(fields, chapter.name) || !end(fields) ||
                !identifier(chapter.id, 48) || !displayName(chapter.name) ||
                candidate.chapters_.size() >= 128 || candidate.chapters_.count(chapter.id))
                return reject("Invalid or duplicate Chapter definition.");
            candidate.chapters_.emplace(chapter.id, chapter);
        }
        else if (command == "territory")
        {
            std::string id, region, chapter;
            int count = -1;
            if (!quoted(fields, id) || !quoted(fields, region) || !quoted(fields, chapter))
                return reject("Malformed territory record.");
            fields >> count;
            if (!fields || count < 0 || count > 64 || !identifier(id, 48) || !identifier(region, 48) ||
                (chapter != "-" && !identifier(chapter, 48)) || territories.count(id) || territories.size() >= MaxAreas)
                return reject("Invalid or duplicate territory record.");
            std::vector<std::string> claims;
            for (int index = 0; index < count; ++index)
            {
                std::string claim;
                if (!quoted(fields, claim) || !identifier(claim, 48) ||
                    std::find(claims.begin(), claims.end(), claim) != claims.end())
                    return reject("Invalid or duplicate faction claim.");
                claims.push_back(claim);
            }
            if (!end(fields)) return reject("Unexpected territory fields.");
            std::sort(claims.begin(), claims.end());
            territories[id] = {region, chapter == "-" ? "" : chapter, claims};
        }
        else if (command == "claims")
        {
            // A place's faction claims as the live tables have them; they replace the territory record's claims.
            std::string id;
            int count = -1;
            if (!quoted(fields, id))
                return reject("Malformed claims record.");
            fields >> count;
            if (!fields || count < 0 || count > 64 || !identifier(id, 48) || liveClaims.count(id) || liveClaims.size() >= MaxAreas)
                return reject("Invalid or duplicate claims record.");
            std::vector<std::string> claims;
            for (int index = 0; index < count; ++index)
            {
                std::string claim;
                if (!quoted(fields, claim) || !identifier(claim, 48) ||
                    std::find(claims.begin(), claims.end(), claim) != claims.end())
                    return reject("Invalid or duplicate faction claim.");
                claims.push_back(claim);
            }
            if (!end(fields)) return reject("Unexpected claims fields.");
            std::sort(claims.begin(), claims.end());
            liveClaims[id] = claims;
        }
        else if (command == "cell")
        {
            std::string id, relative;
            if (!quoted(fields, id) || !quoted(fields, relative) || !end(fields) || !identifier(id, 48) ||
                candidate.cells_.count(id) || candidate.cells_.size() >= MaxCells || relative.size() > 512)
                return reject("Invalid, duplicate, or excessive cell record.");
            const fs::path relativePath(relative);
            if (relative.empty() || relativePath.is_absolute() || relativePath.has_root_path() ||
                relative.find('\\') != std::string::npos || relative.find(':') != std::string::npos)
                return reject("Cell paths must be relative within the exported world directory.");
            for (const auto& segment : relativePath)
                if (segment == ".." || segment == ".")
                    return reject("Cell paths cannot traverse directories.");
            std::string text;
            const auto problem = readCell(relative, text);
            if (!problem.empty())
                return reject(problem);
            const auto loaded = candidate.loadCellText(text, relative);
            if (!loaded.ok)
                return reject(loaded.message);
            const auto* loadedCell = candidate.cell(id);
            if (!loadedCell || candidate.cells_.size() != ++cellFiles)
                return reject("Manifest and cell file identities differ.");
            if (loadedCell->width < 4 || loadedCell->height < 4 || loadedCell->width > 256 || loadedCell->height > 256)
                return reject("Exported cells must be 4 to 256 tiles on each axis.");
            totalTiles += loadedCell->tiles.size();
            if (totalTiles > MaxTiles)
                return reject("World exceeds the 262144 tile import limit.");
        }
        else if (command == "spawn")
        {
            if (spawn || !quoted(fields, candidate.spawnCell_))
                return reject("Duplicate or invalid spawn.");
            fields >> candidate.spawnPosition_.x >> candidate.spawnPosition_.y;
            if (!end(fields))
                return reject("Invalid spawn coordinates.");
            spawn = true;
        }
        else if (command == "door")
        {
            Door door;
            if (!parseDoor(fields, door) || candidate.doors_.count(door.id) || candidate.doors_.size() >= MaxDoors)
                return reject("Invalid, duplicate, or excessive fixture record.");
            candidate.doors_.emplace(door.id, std::move(door));
        }
        else if (command == "area")
        {
            // A streamed cell: its header now, its tiles and seams when someone needs them.
            std::string id, areaHeader;
            if (!quoted(fields, id) || !end(fields) || !identifier(id, 48) || candidate.cells_.count(id) ||
                candidate.cells_.size() >= MaxAreas || !candidate.source_.header || !candidate.source_.load)
                return reject("Invalid, duplicate, or unsourced area record.");
            const auto problem = candidate.source_.header(id, areaHeader);
            if (!problem.empty())
                return reject(problem);
            Cell area;
            std::istringstream text(areaHeader);
            const auto read = candidate.parseCell(text, id, area, true);
            if (!read.ok || area.id != id)
                return reject(read.ok ? "Area header names another cell: " + id : read.message);
            candidate.cells_.emplace(id, std::move(area));
        }
        else if (command == "exits")
        {
            // Which cells a streamed cell's seams lead to, so routes can be planned without loading it.
            std::string id;
            int count = -1;
            if (!quoted(fields, id))
                return reject("Malformed exits record.");
            fields >> count;
            if (!fields || count < 1 || count > 1024 || !identifier(id, 48) || candidate.exits_.count(id))
                return reject("Invalid or duplicate exits record.");
            auto& to = candidate.exits_[id];
            for (int index = 0; index < count; ++index)
            {
                std::string next;
                if (!quoted(fields, next) || !identifier(next, 48) || next == id || !to.insert(next).second)
                    return reject("Invalid exits entry.");
            }
            if (!end(fields)) return reject("Unexpected exits fields.");
        }
        else if (command == "economy")
        {
            auto& e = roster.economy;
            fields >> e.treasury >> e.storeHerbs >> e.storeMeals >> e.dailyHerbs >> e.dailyMeals;
            if (economy || !end(fields) || e.treasury < 0 || e.treasury > 1000000 || e.storeHerbs < 0 ||
                e.storeHerbs > 10000 || e.storeMeals < 0 || e.storeMeals > 10000 || e.dailyHerbs < 0 ||
                e.dailyHerbs > 1000 || e.dailyMeals < 0 || e.dailyMeals > 1000)
                return reject("Invalid or duplicate economy record.");
            economy = true;
        }
        else if (command == "herbs")
        {
            Spot spot;
            if (herbs || !quoted(fields, spot.cell))
                return reject("Invalid or duplicate herb patch.");
            fields >> spot.x >> spot.y;
            if (!end(fields))
                return reject("Invalid herb patch coordinates.");
            places.push_back({"herb patch", spot});
            candidate.herbCell_ = spot.cell;
            candidate.herbPatch_ = {spot.x, spot.y};
            herbs = true;
        }
        else if (command == "route")
        {
            PatrolRoute route;
            int count = -1;
            if (!quoted(fields, route.id) || !identifier(route.id, 48) || roster.routes.count(route.id) ||
                roster.routes.size() >= 128)
                return reject("Invalid or duplicate patrol route.");
            fields >> count;
            if (!fields || count < 1 || count > 64)
                return reject("Patrol routes need 1 to 64 posts.");
            for (int index = 0; index < count; ++index)
            {
                Spot post;
                if (!quoted(fields, post.cell))
                    return reject("Malformed patrol post.");
                fields >> post.x >> post.y;
                places.push_back({"route " + route.id, post});
                route.posts.push_back(post);
            }
            if (!end(fields))
                return reject("Unexpected patrol route fields.");
            roster.routes[route.id] = route;
        }
        else if (command == "story")
        {
            std::string id, personality, backstory;
            if (!quoted(fields, id) || !quoted(fields, personality) || !quoted(fields, backstory) || !end(fields) ||
                personality.size() > 4000 || backstory.size() > 12000)
                return reject("Malformed story record.");
            const auto owner = std::find_if(roster.residents.begin(), roster.residents.end(),
                                            [&](const ResidentSpec& r) { return r.id == id; });
            if (owner == roster.residents.end() || !owner->personality.empty() || !owner->backstory.empty())
                return reject("Story must follow its resident, once: " + id);
            owner->personality = personality;
            owner->backstory = backstory;
        }
        else if (command == "joinable")
        {
            // A resident who may travel with a party (Docs/Design/32, 2.3): follows its resident, once.
            std::string id;
            if (!quoted(fields, id) || !end(fields))
                return reject("Malformed joinable record.");
            const auto owner = std::find_if(roster.residents.begin(), roster.residents.end(),
                                            [&](const ResidentSpec& r) { return r.id == id; });
            if (owner == roster.residents.end() || owner->joinable)
                return reject("Joinable must follow its resident, once: " + id);
            owner->joinable = true;
        }
        else if (command == "let")
        {
            // A place to let (Docs/Design/32, 5.2): let "cell" "hall|warehouse" "landlord" rent level.
            Letting l;
            if (!quoted(fields, l.cell) || !quoted(fields, l.kind) || !quoted(fields, l.landlord))
                return reject("Malformed let record.");
            fields >> l.rent >> l.level;
            if (!fields || !end(fields) || !identifier(l.cell, 48) || (l.kind != "hall" && l.kind != "warehouse") ||
                (l.landlord != "treasury" && !identifier(l.landlord, 64)) || l.rent < 1 || l.rent > 100000 || l.level < 2 ||
                l.level > 5 || candidate.lettings_.count(l.cell))
                return reject("Invalid or duplicate let record.");
            candidate.lettings_[l.cell] = l;
        }
        else if (command == "wander")
        {
            std::string id;
            int count = -1;
            if (!quoted(fields, id))
                return reject("Malformed wander record.");
            fields >> count;
            auto resident = std::find_if(roster.residents.begin(), roster.residents.end(),
                                         [&](const ResidentSpec& r) { return r.id == id; });
            if (resident == roster.residents.end() || !fields || count < 1 || count > 4096 || !resident->wander.empty())
                return reject("Wander area for an unknown resident, or a malformed one: " + id);
            for (int index = 0; index < count; ++index)
            {
                Spot spot;
                if (!quoted(fields, spot.cell))
                    return reject("Malformed wander tile.");
                fields >> spot.x >> spot.y;
                resident->wander.push_back(spot);
            }
            if (!end(fields))
                return reject("Unexpected wander fields.");
        }
        else if (command == "resident")
        {
            ResidentSpec r;
            int paid = -1;
            std::string route;
            auto& a = r.appearance;
            if (!quoted(fields, r.id) || !quoted(fields, r.name) || !quoted(fields, r.role) ||
                !quoted(fields, r.workLabel) || !quoted(fields, r.description) || !quoted(fields, r.greeting))
                return reject("Malformed resident text fields.");
            fields >> r.age;
            if (!quoted(fields, a.species) || !quoted(fields, a.sex) || !quoted(fields, a.stature) ||
                !quoted(fields, a.pattern))
                return reject("Malformed resident appearance.");
            fields >> a.baseColor >> a.gradientColor >> a.markingColor >> r.speakingColor >> paid >> r.startHour >>
                r.endHour;
            if (!quoted(fields, route))
                return reject("Malformed resident route.");
            fields >> r.purse >> r.herbs >> r.meals;
            for (Spot* spot : {&r.home, &r.work, &r.evening})
            {
                if (!quoted(fields, spot->cell))
                    return reject("Malformed resident place.");
                fields >> spot->x >> spot->y;
            }
            r.route = route == "-" ? "" : route;
            r.paid = paid == 1;
            if (!end(fields) || !identifier(r.id, 48) || r.id == "treasury" || r.id.rfind("wolf-", 0) == 0 ||
                r.id.rfind("player-", 0) == 0 || !displayName(r.name) ||
                (r.role != "merchant" && r.role != "guard" && r.role != "civilian") || r.workLabel.empty() ||
                r.workLabel.size() > 40 || r.description.size() > 4096 || r.greeting.size() > 1024 || r.age < 0 ||
                r.age > 200 || !validAppearance(a) || r.speakingColor < 0 || r.speakingColor > 31 || !flag(paid) ||
                !std::isfinite(r.startHour) || !std::isfinite(r.endHour) || r.startHour < 0 || r.startHour >= 24 ||
                r.endHour < 0 || r.endHour >= 24 || r.startHour == r.endHour || r.purse < 0 || r.purse > 100000 ||
                r.herbs < 0 || r.herbs > 10000 || r.meals < 0 || r.meals > 10000 ||
                (!r.route.empty() && (r.role == "merchant" || !identifier(r.route, 48))) ||
                roster.residents.size() >= MaxResidents || residentIds.count(r.id))
                return reject("Invalid or duplicate resident: " + r.id);
            residentIds.insert(r.id);
            for (const auto* spot : {&r.home, &r.work, &r.evening})
                places.push_back({"resident " + r.id, *spot});
            roster.residents.push_back(std::move(r));
        }
        else
            return reject("Unknown manifest record.");
    }
    if (!input.eof() || !header || !spawn || candidate.cells_.empty())
        return reject("World must contain cells and exactly one spawn.");
    for (const auto& [id, to] : candidate.exits_)
        for (const auto& next : to)
            if (!candidate.cells_.count(id) || !candidate.cells_.count(next))
                return reject("Exits refer to an unknown cell: " + id);
    if (candidate.streamed())
    {
        // Validation reads tiles: load just the cells it looks at. Everything else streams in later.
        std::set<std::string> needed{candidate.spawnCell_};
        for (const auto& record : candidate.doors_)
            needed.insert({record.second.cellId, record.second.targetCell});
        for (const auto& [owner, spot] : places)
            needed.insert(spot.cell);
        for (const auto& r : roster.residents)
            for (const auto& spot : r.wander)
                needed.insert(spot.cell);
        for (const auto& id : needed)
            if (candidate.cells_.count(id))
            {
                const auto loaded = candidate.ensureLoaded(id);
                if (!loaded.ok)
                    return reject(loaded.message);
            }
    }
    for (const auto& territory : territories)
    {
        auto* c = candidate.cell(territory.first);
        const auto& [region, chapter, claims] = territory.second;
        if (!c || (!chapter.empty() && !candidate.chapters_.count(chapter)))
            return reject("Territory references an unknown cell or Chapter.");
        for (const auto& claim : claims) if (!candidate.factions_.count(claim))
            return reject("Territory references an unknown faction.");
        c->region = region; c->chapter = chapter; c->factionClaims = claims;
    }
    for (const auto& [cell, l] : candidate.lettings_)
        if (!candidate.cell(cell) || (l.landlord != "treasury" && !residentIds.count(l.landlord)))
            return reject("A place to let names an unknown place or landlord: " + cell);
    for (const auto& [id, claims] : liveClaims)
    {
        auto* c = candidate.cell(id);
        if (!c)
            return reject("Claims reference an unknown cell or interior: " + id);
        for (const auto& claim : claims) if (!candidate.factions_.count(claim))
            return reject("Claims reference an unknown faction: " + claim);
        c->factionClaims = claims;
    }

    using Endpoint = std::tuple<std::string, int, int>;
    std::map<Endpoint, std::set<char>> endpoints;
    for (const auto& record : candidate.doors_)
    {
        const auto& door = record.second;
        if (candidate.streamed() && door.passage && door.boundary)
            continue;                       // A streamed cell's seams were checked as it loaded.
        const auto* source = candidate.cell(door.cellId);
        const auto* target = candidate.cell(door.targetCell);
        const auto* paired = candidate.door(door.linkedDoor);
        if (!source || !target || !paired || source == target || !center(*source, door.position) ||
            !center(*target, door.arrival) || !center(*target, paired->position) || !center(*source, paired->arrival))
            return reject("Fixture has a dangling target, self link, or invalid tile-center anchor: " + door.id);
        const auto* tile = source->tile(int(door.position.x), int(door.position.y));
        const auto* arrivalTile = target->tile(int(door.arrival.x), int(door.arrival.y));
        if (!tile || tile->solid || !arrivalTile || arrivalTile->solid)
            return reject("Fixture or arrival is on solid terrain: " + door.id);
        if (paired->linkedDoor != door.id || paired->cellId != door.targetCell || paired->targetCell != door.cellId ||
            paired->open != door.open || paired->locked != door.locked || paired->boundary != door.boundary ||
            paired->passage != door.passage)
            return reject("Fixture pairing or flags are not reciprocal: " + door.id);
        const bool stairs = door.id.rfind("stairs_", 0) == 0;
        if ((stairs || door.passage) && (!door.open || door.locked))
            return reject("Stairs and passages must remain unlocked and open: " + door.id);
        if (stairs != (paired->id.rfind("stairs_", 0) == 0) || (stairs && tile->glyph != '^'))
            return reject("Stair fixtures must be paired steps: " + door.id);
        if (!stairs && !door.passage && tile->glyph != '+')
            return reject("Door fixture requires a + terrain endpoint: " + door.id);
        if (door.boundary != (door.edge != '-'))
            return reject("Boundary fixtures require an explicit edge; interior fixtures cannot set one: " + door.id);
        const Endpoint endpoint{door.cellId, int(door.position.x), int(door.position.y)};
        auto& occupied = endpoints[endpoint];
        if (!occupied.empty() && (!(door.passage && door.boundary) || occupied.count('-')))
            return reject("Fixture endpoint is reused: " + door.id);
        const char occupancy = door.passage && door.boundary ? door.edge : '-';
        if (!occupied.insert(occupancy).second)
            return reject("Fixture edge is reused: " + door.id);

        if (door.passage && door.boundary)
        {
            if (!onEdge(*source, door) || !onEdge(*target, *paired) || opposite(door.edge) != paired->edge ||
                !same(door.arrival, paired->position) || !same(paired->arrival, door.position) ||
                !near(source->worldZ, target->worldZ))
                return reject("Automatic seam anchors, edges or elevation layers disagree: " + door.id);
            const double dx = target->worldX + paired->position.x - source->worldX - door.position.x;
            const double dy = target->worldY + paired->position.y - source->worldY - door.position.y;
            const double expectedX = door.edge == 'E' ? 1 : door.edge == 'W' ? -1 : 0;
            const double expectedY = door.edge == 'S' ? 1 : door.edge == 'N' ? -1 : 0;
            if (!near(dx, expectedX) || !near(dy, expectedY) || std::abs(tile->height - arrivalTile->height) > .55)
                return reject("Automatic seam cells are not adjacent or their height step is too large: " + door.id);
        }
        else
        {
            if (std::abs(door.arrival.x - paired->position.x) + std::abs(door.arrival.y - paired->position.y) != 1.0)
                return reject("Portal arrival must be beside its paired endpoint: " + door.id);
            if (door.boundary && (!onEdge(*source, door) || door.passage))
                return reject("Boundary portal is not on its specified edge: " + door.id);
            const auto* pairedTile = target->tile(int(paired->position.x), int(paired->position.y));
            if (!pairedTile || std::abs(arrivalTile->height - pairedTile->height) > .75)
                return reject("Portal arrival would strand an actor beyond a height step: " + door.id);
        }
    }
    for (const auto& record : candidate.doors_)
    {
        const auto& door = record.second;
        const auto arrivalEndpoint = endpoints.find({door.targetCell, int(door.arrival.x), int(door.arrival.y)});
        if (!(door.passage && door.boundary) && arrivalEndpoint != endpoints.end() &&
            arrivalEndpoint->second.count('-'))
            return reject("Portal arrival collides with another fixture endpoint: " + door.id);
    }
    const auto* spawnCell = candidate.cell(candidate.spawnCell_);
    if (!spawnCell || !center(*spawnCell, candidate.spawnPosition_))
        return reject("Spawn refers to a missing cell or invalid tile center.");
    const auto* spawnTile = spawnCell->tile(int(candidate.spawnPosition_.x), int(candidate.spawnPosition_.y));
    const auto spawnEndpoint =
        endpoints.find({candidate.spawnCell_, int(candidate.spawnPosition_.x), int(candidate.spawnPosition_.y)});
    if (!spawnTile || spawnTile->solid || (spawnEndpoint != endpoints.end() && spawnEndpoint->second.count('-')))
        return reject("Spawn is blocked or coincides with a fixture endpoint.");

    // Authored places must be open tile centers in real cells.
    for (const auto& [owner, spot] : places)
    {
        const auto* c = candidate.cell(spot.cell);
        const auto* t = c && center(*c, {spot.x, spot.y}) ? c->tile(int(spot.x), int(spot.y)) : nullptr;
        if (!t || t->solid)
            return reject("Place for " + owner + " is missing, off-center or on solid terrain.");
    }
    // Painted wander areas may brush walls or furniture: keep only open tile centers in real cells.
    for (auto& r : roster.residents)
    {
        std::vector<Spot> open;
        for (const auto& spot : r.wander)
        {
            const auto* c = candidate.cell(spot.cell);
            const auto* t = c && center(*c, {spot.x, spot.y}) ? c->tile(int(spot.x), int(spot.y)) : nullptr;
            if (t && !t->solid)
                open.push_back(spot);
        }
        r.wander = std::move(open);
    }
    for (auto& r : roster.residents)
    {
        if (!r.route.empty() && !roster.routes.count(r.route))
            return reject("Resident " + r.id + " walks an unknown patrol route.");
        if (r.role != "merchant")
            continue;
        // Customers stand on the first open neighbor of the merchant's counter place.
        const auto* c = candidate.cell(r.work.cell);
        bool found = false;
        for (const auto& [dx, dy] : {std::pair{0, 1}, {0, -1}, {1, 0}, {-1, 0}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}})
        {
            const auto* t = c->tile(int(r.work.x) + dx, int(r.work.y) + dy);
            if (!found && t && !t->solid)
            {
                r.serve = {r.work.cell, r.work.x + dx, r.work.y + dy};
                found = true;
            }
        }
        if (!found)
            return reject("Merchant " + r.id + " has no open tile beside the counter for customers.");
    }
    for (const auto& r : roster.residents)
    {
        Entity e;
        e.id = r.id;
        e.name = r.name;
        e.npc = true;
        e.age = r.age;
        e.lastBirthdayDay = candidate.calendarDays_;
        e.description = r.description;
        e.activity = r.workLabel;
        e.speakingColor = r.speakingColor;
        e.appearance = r.appearance;
        // The calendar begins at noon: residents on duty then start at work.
        const bool working = r.startHour <= r.endHour ? r.startHour <= 12 && 12 < r.endHour
                                                      : 12 >= r.startHour || 12 < r.endHour;
        const Spot& start = working ? r.work : r.home;
        e.cellId = start.cell;
        e.position = {start.x, start.y};
        candidate.entities_[e.id] = e;
    }
    if (!roster.residents.empty())
        candidate.society_.configure(roster);
    candidate.rebuildFixtureIndex();
    *this = std::move(candidate);
    furnishHomes();                                 // Every home's stores, from the start (doc 36).
    return {true, "Authored world loaded atomically.", path};
}
} // namespace ratw
