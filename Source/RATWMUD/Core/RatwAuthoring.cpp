#include "RatwWorld.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <tuple>

namespace ratw
{
namespace
{
namespace fs = std::filesystem;
constexpr std::size_t MaxCells = 256, MaxDoors = 65536, MaxTiles = 262144;
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
bool contained(const fs::path& root, const fs::path& path)
{
    auto filePart = path.begin();
    for (auto rootPart = root.begin(); rootPart != root.end(); ++rootPart, ++filePart)
        if (filePart == path.end() || *rootPart != *filePart)
            return false;
    return filePart != path.end();
}
} // namespace

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

    // Everything is parsed and validated off to the side. A rejection never
    // mutates the running world's maps, entities, remembered views, or clock.
    World candidate;
    candidate.cells_.clear();
    candidate.factions_.clear();
    candidate.chapters_.clear();
    candidate.entities_.clear();
    candidate.doors_.clear();
    candidate.blockingFixtures_.clear();
    candidate.spawnCell_.clear();
    candidate.customWorld_ = true;
    candidate.society_.reset(false);
    std::set<fs::path> files;
    std::map<std::string, std::tuple<std::string, std::string, std::vector<std::string>>> territories;
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
            if (command != "RATW_WORLD" || version != 1 || !end(fields))
                return reject("Expected RATW_WORLD 1.");
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
                (chapter != "-" && !identifier(chapter, 48)) || territories.count(id) || territories.size() >= MaxCells)
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
            const fs::path cellPath = fs::canonical(root / relativePath, error);
            if (error || !contained(root, cellPath) || !fs::is_regular_file(cellPath, error) || error ||
                !files.insert(cellPath).second)
                return reject("Cell file is missing, reused, or escapes the world directory.");
            const auto loaded = candidate.loadCellFile(cellPath.string());
            if (!loaded.ok)
                return reject(loaded.message);
            const auto* loadedCell = candidate.cell(id);
            if (!loadedCell || candidate.cells_.size() != files.size())
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
            int open = -1, locked = -1, boundary = -1, passage = -1;
            std::string edge;
            if (!quoted(fields, door.id) || !quoted(fields, door.name) || !quoted(fields, door.cellId))
                return reject("Malformed quoted fixture fields.");
            fields >> door.position.x >> door.position.y;
            if (!quoted(fields, door.targetCell))
                return reject("Malformed fixture target.");
            fields >> door.arrival.x >> door.arrival.y;
            if (!quoted(fields, door.linkedDoor))
                return reject("Malformed paired fixture ID.");
            fields >> open >> locked >> boundary >> passage;
            if (!quoted(fields, edge) || !end(fields) || !identifier(door.id, 160) || !identifier(door.cellId, 48) ||
                !identifier(door.targetCell, 48) || !identifier(door.linkedDoor, 160) || door.name.empty() ||
                door.name.size() > 512 || !flag(open) || !flag(locked) || !flag(boundary) || !flag(passage) ||
                edge.size() != 1 || std::string("NESW-").find(edge[0]) == std::string::npos ||
                candidate.doors_.count(door.id) || candidate.doors_.size() >= MaxDoors)
                return reject("Invalid, duplicate, or excessive fixture record.");
            door.open = open == 1;
            door.locked = locked == 1;
            door.boundary = boundary == 1;
            door.passage = passage == 1;
            door.edge = edge[0];
            door.portal = true;
            candidate.doors_.emplace(door.id, std::move(door));
        }
        else
            return reject("Unknown manifest record.");
    }
    if (!input.eof() || !header || !spawn || candidate.cells_.empty())
        return reject("World must contain cells and exactly one spawn.");
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

    using Endpoint = std::tuple<std::string, int, int>;
    std::map<Endpoint, std::set<char>> endpoints;
    for (const auto& record : candidate.doors_)
    {
        const auto& door = record.second;
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

    candidate.rebuildFixtureIndex();
    *this = std::move(candidate);
    return {true, "Authored world loaded atomically.", path};
}
} // namespace ratw
