#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "RatwAppearance.h"
#include "RatwCalendar.h"
#include "RatwSociety.h"

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

struct Wind
{
    // Airflow heading, not meteorological "from": east == 0, south == pi/2.
    double direction = 0.0, strength = 0.0;
    bool variable = false;
};

// Whole-cell authored interior light, independent of whether a roof shelters
// the cell. Values are normalized, and outdoor cells ignore this profile.
struct Lighting
{
    double artificial = 1.0, daylightAccess = 1.0;
    std::string tone = "warm";
};

// Shared authoritative conditions, not a client-only weather overlay. Factors
// multiply otherwise healthy senses/base movement; sight includes illumination.
// Shelter is currently authored per whole cell, not per roof tile.
struct Environment
{
    double hour = 12.0, daylight = 1.0, illumination = 1.0;
    double sight = 1.0, hearing = 1.0, scent = 1.0, movement = 1.0;
    std::string phase = "day";
    double artificialLight = 0.0, daylightAccess = 1.0, glowStrength = 0.0;
    std::string lightingTone = "neutral", lightSource = "daylight";
    calendar::Calendar date;
};

struct Cell
{
    std::string id, name, description;
    int width = 0, height = 0;
    double worldX = 0.0, worldY = 0.0, worldZ = 0.0;
    bool outdoors = false;
    Weather weather = Weather::Clear;
    Wind wind; // Base wind; windAt() adds deterministic outdoor variation.
    Lighting lighting;
    bool seasonalWeather = true;
    std::string region = "unassigned", chapter;
    std::vector<std::string> factionClaims;
    std::vector<Tile> tiles;
    const Tile* tile(int x, int y) const;
    Tile* tile(int x, int y);
};

struct FactionDefinition { std::string id, name, color; };
struct ChapterDefinition { std::string id, name; };

struct Entity
{
    std::string id, name, cellId;
    Vec2 position, velocity;
    double facing = 0.0;
    // Stationary turns are authoritative, shortest-arc, and rate limited.
    double turnTarget = 0.0;
    bool turning = false;
    bool npc = false;
    double hearing = 1.0, vision = 1.0;
    double earHealth = 1.0, eyeHealth = 1.0;
    double sneakSkill = 0.0, hearingSkill = 0.0; // Skill values in [0, 100].
    double smell = 1.0, noseHealth = 1.0, scentSkill = 0.0;
    double dexterity = 50.0, stamina = 100.0; // Server-owned, [0,100].
    int pace = 0;                             // Requested travel pace, 0 (walk) .. 10 (sprint).
    bool exhausted = false;                   // Recovery hysteresis; never a client speed override.
    double staminaRate = 0.0;                 // Last simulation step's net change per second.
    int age = 18;
    Appearance appearance;
    double strength = 50.0, wisdom = 30.0;
    double lastBirthdayDay = -1.0; // Legacy/new records anchor on first observation of the shared calendar.
    int ageNoticePending = 0;
    std::string posture = "standing", state, description, activity, leaderId;
    // A rise never translates the actor; retained movement resumes afterward.
    double postureRemaining = 0.0;
    std::string postureTarget;
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
    bool passage = false; // Permanently open; boundary passages are invisible transport seams.
    char edge = '-';      // Explicit boundary orientation; '-' retains legacy door behavior.
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

struct ScentCue
{
    // Eight broad bearings: E, SE, S, SW, W, NW, N, NE. No source metadata.
    int sector = 0, strength = 1; // Strength category 1..3, never distance.
    bool windborne = false;
};

struct Snapshot
{
    double time = 0;
    Environment environment;
    Entity self;
    Cell cell; // Hidden tiles have blank glyphs and no terrain properties.
    std::vector<bool> visibleTiles, rememberedTiles;
    std::vector<Entity> entities;  // Includes self; never hidden actors.
    std::vector<Door> doors;       // Currently visible fixtures only.
    std::vector<MapCell> worldMap; // Current plus first-degree known neighbors.
    bool isometric = false;
    std::vector<ScentCue> scentCues; // Aggregated unseen wolves, no identities.
    bool movementHeard = false;      // Anonymous unseen player movement only.
};

struct Result
{
    bool ok = false;
    std::string message, targetId;
};
struct TravelState
{
    bool active = false, paused = false;
    std::string destination, status, nextDoor;
    std::vector<std::string> route; // Visited cells only; no hidden map geometry.
};
struct SensoryResult
{
    double hearing = 0.0, vision = 0.0;
    bool identifiable = false;
    double scent = 0.0; // Scent alone never establishes identity.
};

struct PersistedWorld
{
    double time = 0;
    double clockOffsetHours = 12.0; // Legacy saves start from the noon epoch.
    double calendarDays = -1.0; // Absent legacy calendar preserves old phase; no retroactive age rewards.
    std::map<std::string, bool> seasonalWeather;
    bool hasSociety = false;
    SocietyState society;
    std::vector<Entity> players;
    std::vector<Entity> npcs;
    std::map<std::string, bool> doorStates;
    std::map<std::string, Weather> weather;
    std::map<std::string, Wind> winds;
    std::map<std::string, Lighting> lighting;
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
    double calendarDays() const { return calendarDays_; }
    Society& society() { return society_; }
    const Society& society() const { return society_; }
    const std::map<std::string, FactionDefinition>& factions() const { return factions_; }
    const std::map<std::string, ChapterDefinition>& chapters() const { return chapters_; }
    Result relocateResident(const std::string& npc, const std::string& destination, double x, double y);
    Result advanceCalendar(double days); // Explicit developer/test jump, never a client-authorized normal action.
    Result useSeasonalWeather(const std::string& cellId);
    Result trade(const std::string& player, const std::string& merchant, const std::string& item, int quantity, bool buy);
    Result gather(const std::string& player);
    Result eat(const std::string& player);

    void tick(double dt);
    Result move(const std::string& id, double dx, double dy);
    Result moveTo(const std::string& id, double x, double y);
    Result face(const std::string& id, double x, double y);
    Result setPosture(const std::string& id, const std::string& posture);
    Result stop(const std::string& id);
    Result setPace(const std::string& id, int pace);
    Result travelTo(const std::string& id, const std::string& destination);
    Result cancelTravel(const std::string& id);
    TravelState travelState(const std::string& id) const;
    std::vector<MapCell> travelMap(const std::string& id) const;
    Result interact(const std::string& id, const std::string& target, const std::string& verb);
    std::vector<std::string> actions(const std::string& id, const std::string& target) const;
    Snapshot snapshot(const std::string& observerId);
    SensoryResult perceive(const std::string& observerId, const std::string& sourceId,
                           Voice voice = Voice::Speak) const;
    bool lineOfSight(const std::string& cellId, Vec2 from, Vec2 to) const;
    double hearingClarity(const std::string& observerId, const std::string& sourceId, Voice voice = Voice::Speak) const;
    double visionClarity(const std::string& observerId, const std::string& sourceId) const;
    // Movement sounds only: this never reduces deliberate spoken voice volume.
    double movementAudibility(const std::string& observerId, const std::string& sourceId) const;
    double scentClarity(const std::string& observerId, const std::string& sourceId) const;
    std::vector<ScentCue> scentCues(const std::string& observerId) const;
    Wind windAt(const std::string& cellId) const;
    Environment environmentAt(const std::string& cellId) const;
    Result setTimeOfDay(double hour); // Finite [0,24); simulation clock only.
    Result setLighting(const std::string& cellId, double artificial, double daylightAccess, const std::string& tone);
    Result setWind(const std::string& cellId, double direction, double strength, bool variable = false);
    void setWeather(const std::string& cellId, Weather weather);
    void observe(const std::string& observerId);
    const std::map<std::string, CellMemory>& memories(const std::string& observerId) const;
    PersistedWorld save() const;
    Result restore(const PersistedWorld& state);
    // Optional authored map file, format documented in Data/Cells/README.md.
    Result loadCellFile(const std::string& path);
    // Startup-only, atomic replacement from an exported Atlas manifest.
    Result loadWorldFile(const std::string& path);

  private:
    std::map<std::string, Cell> cells_;
    std::map<std::string, FactionDefinition> factions_;
    std::map<std::string, ChapterDefinition> chapters_;
    std::map<std::string, Entity> entities_;
    std::map<std::string, Door> doors_;
    // Only closable fixtures can occlude/collide. Hundreds of open authoring
    // seams must not turn each sight-ray sample into a full-world scan.
    std::map<std::string, std::map<std::pair<int, int>, std::vector<std::string>>> blockingFixtures_;
    std::map<std::string, std::map<std::string, CellMemory>> memories_;
    // Explicit portal actions wait for posture changes, but are never saved.
    std::map<std::string, std::string> pendingPortals_;
    std::map<std::string, TravelState> travels_;
    // Transient route execution bookkeeping; never restored from a save.
    std::map<std::string, std::string> travelLegCells_;
    std::map<std::string, double> travelRetryAt_;
    struct TravelProgress
    {
        Vec2 position;
        double checkedAt = 0;
        int stagnant = 0, transitions = 0;
    };
    std::map<std::string, TravelProgress> travelProgress_;
    bool issuingTravel_ = false;
    double time_ = 0.0;
    double clockOffsetHours_ = 12.0;
    double calendarDays_ = .5;
    std::int64_t climateSlot_ = -1;
    Society society_;
    double scheduleAccumulator_ = 0.0;
    std::string spawnCell_ = "tavern";
    Vec2 spawnPosition_{16.5, 12.5};
    bool customWorld_ = false;
    bool passable(const std::string& cellId, Vec2 p, double fromHeight = 0.0) const;
    bool visiblePoint(const Entity& observer, Vec2 point) const;
    bool visiblePortal(const Entity& observer, const Door& door) const;
    double sightRange(const Entity& observer) const;
    std::vector<Vec2> findPath(const Entity& actor, Vec2 goal, bool allowClosed = false) const;
    void integrate(Entity& actor, double dt);
    void updateStamina(Entity& actor, double dt, double movedTime);
    void updateTravel(Entity& actor);
    void prepareMovement(Entity& actor);
    void transition(Entity& actor, const Door& door);
    void updateSchedules();
    void separate(double dt);
    void createDemo();
    void rebuildFixtureIndex();
    bool blockedByDoor(const std::string& cellId, Vec2 point) const;
};

const char* weatherName(Weather value);
const char* knowledgeName(Knowledge value);
const char* voiceName(Voice value);
const char* paceName(int pace);
double paceSpeed(const Entity& actor); // Flat-terrain speed before weather/posture.
int effectivePace(const Entity& actor);
double ageVisionFactor(const Entity& actor);
double ageHearingFactor(const Entity& actor);
double effectiveDexterity(const Entity& actor);
int advanceAge(Entity& actor, double absoluteDay); // Idempotent annual rewards and pending birthday notification.

} // namespace ratw
