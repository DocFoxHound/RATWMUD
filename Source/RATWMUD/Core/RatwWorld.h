#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

// Engine-independent, deterministic authoritative simulation. Positions are in
// terrain-tile units, with x right and y down. Facing is radians, east == zero.
namespace ratw
{

struct Vec2
{
    double x = 0.0, y = 0.0;
};
enum class Terrain
{
    Floor,
    Wall,
    Grass,
    Water,
    Table,
    Counter,
    Stairs
};
enum class Weather
{
    Clear,
    Rain,
    Fog,
    Snow
};
enum class Voice
{
    Whisper,
    Speak,
    Yell
};
enum class Knowledge
{
    Unknown,
    Glimpsed,
    Visited
};

struct Tile
{
    char glyph = '.';
    Terrain terrain = Terrain::Floor;
    bool solid = false;
    bool opaque = false;
    double height = 0.0;
    double movementCost = 1.0;
};

struct Cell
{
    std::string id, name, description;
    int width = 0, height = 0;
    double worldX = 0.0, worldY = 0.0, worldZ = 0.0;
    bool outdoors = false;
    Weather weather = Weather::Clear;
    std::vector<Tile> tiles;
    const Tile* tile(int x, int y) const;
    Tile* tile(int x, int y);
};

struct Entity
{
    std::string id, name, cellId;
    Vec2 position, velocity;
    double facing = 0.0;
    bool npc = false;
    double hearing = 1.0, vision = 1.0;
    double earHealth = 1.0, eyeHealth = 1.0;
    std::string posture = "standing", state, description, activity, leaderId;
    int speakingColor = 0;
    bool typing = false;
    double speakingUntil = 0.0;
    std::vector<Vec2> path;
    Vec2 input;
    // Set by an accepted transition; UI must release/reissue held movement.
    bool transitioned = false;
};

struct Door
{
    std::string id, name, cellId;
    Vec2 position;
    bool open = false;
    bool locked = false;
    bool portal = false;
    bool boundary = false;
    std::string targetCell, linkedDoor;
    Vec2 arrival;
    double reach = 1.7;
};

struct CellMemory
{
    Knowledge knowledge = Knowledge::Unknown;
    std::string cellId, name;
    int width = 0, height = 0;
    double worldX = 0, worldY = 0, worldZ = 0;
    // Only individually observed tiles are retained, including in visited cells.
    std::vector<char> glyphs;
    std::vector<bool> observed;
};

struct MapCell
{
    std::string id, name;
    int width = 0, height = 0;
    double worldX = 0, worldY = 0, worldZ = 0;
    Knowledge knowledge = Knowledge::Unknown;
    bool current = false, visible = false;
    // Empty for coarse glimpses. Remembered detail contains no entity state.
    std::vector<char> rememberedGlyphs;
};

struct Snapshot
{
    double time = 0;
    Entity self;
    Cell cell; // Hidden tiles have blank glyphs and no terrain properties.
    std::vector<bool> visibleTiles, rememberedTiles;
    std::vector<Entity> entities;  // Includes self; never hidden actors.
    std::vector<Door> doors;       // Currently visible fixtures only.
    std::vector<MapCell> worldMap; // Current plus first-degree known neighbors.
    bool isometric = false;
};

struct Result
{
    bool ok = false;
    std::string message, targetId;
};
struct SensoryResult
{
    double hearing = 0.0, vision = 0.0;
    bool identifiable = false;
};

struct PersistedWorld
{
    double time = 0;
    std::vector<Entity> players;
    std::vector<Entity> npcs;
    std::map<std::string, bool> doorStates;
    std::map<std::string, Weather> weather;
    std::map<std::string, std::map<std::string, CellMemory>> memories;
};

class World
{
  public:
    World();
    Entity& addPlayer(const std::string& id, const std::string& name);
    bool removePlayer(const std::string& id);
    Entity* entity(const std::string& id);
    const Entity* entity(const std::string& id) const;
    const Cell* cell(const std::string& id) const;
    Cell* cell(const std::string& id);
    const std::map<std::string, Cell>& cells() const
    {
        return cells_;
    }
    const std::map<std::string, Entity>& entities() const
    {
        return entities_;
    }
    const std::map<std::string, Door>& doors() const
    {
        return doors_;
    }
    const Door* door(const std::string& id) const;
    double time() const
    {
        return time_;
    }

    void tick(double dt);
    Result move(const std::string& id, double dx, double dy);
    Result moveTo(const std::string& id, double x, double y);
    Result face(const std::string& id, double x, double y);
    Result stop(const std::string& id);
    Result interact(const std::string& id, const std::string& target, const std::string& verb);
    std::vector<std::string> actions(const std::string& id, const std::string& target) const;
    Snapshot snapshot(const std::string& observerId);
    SensoryResult perceive(const std::string& observerId, const std::string& sourceId,
                           Voice voice = Voice::Speak) const;
    bool lineOfSight(const std::string& cellId, Vec2 from, Vec2 to) const;
    double hearingClarity(const std::string& observerId, const std::string& sourceId, Voice voice = Voice::Speak) const;
    double visionClarity(const std::string& observerId, const std::string& sourceId) const;
    void setWeather(const std::string& cellId, Weather weather);
    void observe(const std::string& observerId);
    const std::map<std::string, CellMemory>& memories(const std::string& observerId) const;
    PersistedWorld save() const;
    Result restore(const PersistedWorld& state);
    // Optional authored map file, format documented in Data/Cells/README.md.
    Result loadCellFile(const std::string& path);

  private:
    std::map<std::string, Cell> cells_;
    std::map<std::string, Entity> entities_;
    std::map<std::string, Door> doors_;
    std::map<std::string, std::map<std::string, CellMemory>> memories_;
    double time_ = 0.0;
    double scheduleAccumulator_ = 0.0;
    bool passable(const std::string& cellId, Vec2 p, double fromHeight = 0.0) const;
    bool visiblePoint(const Entity& observer, Vec2 point) const;
    bool visiblePortal(const Entity& observer, const Door& door) const;
    double sightRange(const Entity& observer) const;
    std::vector<Vec2> findPath(const Entity& actor, Vec2 goal, bool allowClosed = false) const;
    void integrate(Entity& actor, double dt);
    void transition(Entity& actor, const Door& door);
    void updateSchedules();
    void separate(double dt);
    void createDemo();
};

const char* weatherName(Weather value);
const char* knowledgeName(Knowledge value);
const char* voiceName(Voice value);

} // namespace ratw
