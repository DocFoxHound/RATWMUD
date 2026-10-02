#pragma once

#include <cstdint>
#include <functional>
#include <iosfwd>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>
#include "RatwAppearance.h"
#include "RatwBonds.h"
#include "RatwCrime.h"
#include "RatwSchedules.h"
#include "RatwAmbient.h"
#include "RatwRoads.h"
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
    Stairs,
    Slope,
    Cliff,
    Feature // Trees, furniture, statues and the like: the catalog gives their rules.
};
// Saved by number: append new kinds, never reorder.
enum class Weather
{
    Clear,
    Rain,
    Fog,
    Snow,
    Overcast,
    Storm,
    Sandstorm
};
constexpr int WeatherKinds = 7;
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
    double stature = 0.0; // How far the tile stands above its ground, for sight: a tree, a shelf, a boulder.
    double movementCost = 1.0;
};

// One entry of the terrain catalog (Data/Terrain/terrain.json). Tiles are stored as `code` and drawn
// as the Unicode `glyph`, or as `ascii` where Unicode is unavailable.
struct TerrainInfo
{
    char code;
    Terrain kind;
    bool solid, opaque;
    double height, stature, cost;
    bool ramp;
    char16_t glyph;
    char ascii;
    std::uint32_t fg, bg;
    const char* name;
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
    // False while a streamed cell's tiles and seams are not in memory; its header (everything above) always is.
    bool loaded = true;
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
    // Dead characters lie where they fell and cannot move or act until brought back (Dungeon Master).
    bool dead = false;
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
    // How badly hurt (0..100): at 100 a character is beaten down and lies there until it heals below 50 (Phase 7).
    double hurt = 0.0;
    bool typing = false;
    double speakingUntil = 0.0;
    std::vector<Vec2> path;
    Vec2 input;
    // Set by an accepted transition; UI must release/reissue held movement.
    bool transitioned = false;
    // Simulation tier (see World::setTiered), never saved: an NPC far from every player lives its day in timed
    // steps from one known-good place to the next instead of walking every tile.
    bool offstage = false;
    // Folk of the road (a caravan's wagon, bandits), made from the roads' state as needed and never saved themselves.
    bool transient = false;
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

// Something that happened, for the world's event log (game.events): what, who, to whom, where and when. It says
// that a conversation took place, never what was said.
struct WorldEvent
{
    std::string kind, actor, target, cell;
    double time = 0, day = 0;                     // World seconds and calendar days.
    std::string item;
    int quantity = 0;
    std::int64_t coins = 0;
    std::string detail;
};

// Events as the JSON array game.record_events takes (migration 0021): text cut to the table's limits on whole
// UTF-8 characters, anything that isn't valid UTF-8 dropped, so no event can make a save fail.
std::string eventsJson(const std::vector<WorldEvent>& events);

// A promise made in conversation (Docs/Design/26-living-npcs.md), kept track of by the world: kept if the one who
// made it trades with, pays, gives to or helps the other before it is due; broken if it falls due first.
struct Promise
{
    std::string by, to, what;
    double made = 0, due = 0;       // Calendar days.
    std::string status = "open";    // "open", "kept" or "broken".
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
    std::vector<SavedBond> bonds;
    std::vector<Promise> promises;
    RoadsState roads;
    CrimeState crime;
    std::vector<CalledFestival> festivals;          // Called by the Dungeon Master (Phase 9).
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
    // The doors, stairs and seams in one cell, in ID order (an index over doors(); rebuilt when doors change).
    const std::vector<Door*>& doorsIn(const std::string& cellId) const;
    // Cells one step away through any portal (seams, doors, stairs), locked or not.
    std::set<std::string> neighbors(const std::string& cellId) const;
    // Streamed worlds: the cells not in memory next to ones that are, the ones someone may need next. A host can
    // fetch their files ahead of time so that loading one (ensureLoaded) waits on nothing.
    std::vector<std::string> cellsSoonNeeded() const;
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
    // Simulation tiers. With them on, an NPC is simulated in full only on the stage (a cell with a player in it, or
    // next to one). Offstage, residents keep their needs, work, trade and schedules exactly as before, but travel as
    // timed hops between known places (their goals, doors and cell crossings) without walking, colliding or needing
    // the cell in memory, so a world's cost follows its players rather than its population. On by default for
    // streamed worlds (the live server); off for small authored worlds, which are always wholly in memory.
    void setTiered(bool on) { tieredSet_ = true; tiered_ = on; }
    bool tiered() const { return tieredSet_ ? tiered_ : streamed(); }
    std::size_t offstageCount() const;
    // The event log. The world records deaths, revivals, relocations and every economy ledger entry; a host adds
    // its own (arrivals, conversations, spawns...) with recordEvent. takeEvents() hands over everything since the
    // last call, oldest first. Uncollected, at most EventsKept are kept (the oldest are dropped and counted).
    static constexpr std::size_t EventsKept = 50000;
    void recordEvent(WorldEvent event);
    // How everyone regards everyone else (see RatwBonds.h). The world moves them by rule, from the events it records
    // and from time spent together; a host may add a conversation's small, clamped nudge.
    const Bonds& bonds() const { return bonds_; }
    Bonds& bonds() { return bonds_; }
    // A player asks the NPC to take them on as an apprentice in the NPC's trade (see Society::apprentice).
    Result apprentice(const std::string& player, const std::string& master);
    // A promise made in conversation, due in `days`. Keeping it builds the other's trust; breaking it costs more.
    void promise(const std::string& by, const std::string& to, const std::string& what, double days = 3);
    const std::vector<Promise>& promises() const { return promises_; }
    // Open promises between two characters, in words for a conversation (empty if none).
    std::string promisesBetween(const std::string& npc, const std::string& other, const std::string& otherName) const;
    // Residents the society wants (a stranger for an empty post, a child), each with a free place to sleep found near
    // the home it asked for. The host makes them and then calls welcomeResident with their new ID.
    std::vector<ResidentRequest> takeResidentRequests();
    Result welcomeResident(const ResidentRequest& request, const std::string& id);
    // The roads (RatwRoads.h): towns, caravans, bandit camps, contracts and rumours. Active when the world has two
    // or more towns; a single settlement keeps its one store (the treasury) as before.
    const std::vector<Town>& towns() const { return towns_; }
    const RoadsState& roads() const { return roads_; }
    RoadsState& roads() { return roads_; }                 // For operators (clear a camp, say) and tests.
    const Town* townOf(const std::string& cellId) const;
    // Contracts a player could take where they stand, taking one, and marking one done (a bounty, by whoever ended
    // the camp; the Dungeon Master, for now). Couriers, escorts and supply runs are finished by doing them.
    std::vector<const Contract*> contractsNear(const std::string& player) const;
    Result takeContract(const std::string& player, const std::string& contractId);
    Result completeContract(const std::string& contractId, const std::string& by);
    // Posts work, the reward set aside from the poster's purse at once (no reward if they can't pay).
    Contract& postContract(const std::string& kind, const std::string& poster, const std::string& town,
                           const std::string& target, std::int64_t reward, double days, const std::string& detail);
    // What someone has heard: a claim about a subject, from a source, this sure (0..1). Rumours spread along bonds
    // each day, losing confidence, and between towns with the caravans.
    void believe(const std::string& holder, const std::string& subject, const std::string& claim,
                 const std::string& source, double confidence, const std::string& incident = {});
    const std::vector<Belief>* beliefsOf(const std::string& holder) const;
    // What an NPC has heard about someone, in words for a conversation (empty if nothing).
    std::string rumoursAbout(const std::string& npc, const std::string& subject, const std::string& subjectName) const;
    // Fights (a first, placeholder version; only bandits can be fought). A player swings at a bandit within reach;
    // stamina is what a fight wears down, and one beaten to the ground is robbed, not killed.
    Result attack(const std::string& attacker, const std::string& target);
    // Crime and law (RatwCrime.h, Phase 7). A theft from or an assault on a resident is an incident, known only to
    // those who perceived it; they tell the watch, which wants the offender once what it has heard is enough.
    // Players can't steal from or attack each other.
    Result steal(const std::string& thief, const std::string& victim);
    Result report(const std::string& player, const std::string& guard);   // What they saw, told to the watch.
    Result payFine(const std::string& person, const std::string& guard);  // What the watch asks, to a guard.
    const CrimeState& crime() const { return crime_; }
    CrimeState& crime() { return crime_; }                  // For operators and tests.
    const Warrant* warrantFor(const std::string& person) const;
    const Custody* custodyOf(const std::string& person) const;
    std::int64_t owedBy(const Warrant& w) const;             // Restitution and the fine, in pennies.
    bool guardOnDuty(const std::string& id) const;
    std::string lawTown(const std::string& cellId) const;   // Whose Watch keeps the law here ("" for nobody's).
    // Schedules (Phase 9). A cell's community is its town (a region, where there are no town records); "" for none.
    std::string communityOf(const std::string& cellId) const { return lawTown(cellId); }
    DayPlan dayPlan(const std::string& community);          // Today's, as residents live it.
    std::string dayLabel(const std::string& cellId);        // "Marketday", "Restday", "Stoneday · Harvest Home"...
    std::string festivalName(const std::string& community, int season) const;
    // A festival for a community, today (from noon) or some days ahead; `name` "" for the season's own.
    Result callFestival(const std::string& community, const std::string& name, int inDays = 0);
    const std::vector<CalledFestival>& calledFestivals() const { return festivals_; }
    // The ambient director (Phase 10). The exchanges worth voicing now where these players can hear, best first (one
    // a cell), leaving out anyone in `busy` (talking with a player, say) and whoever talked lately.
    std::vector<AmbientPick> ambientPicks(const std::vector<std::string>& listeners, const std::set<std::string>& busy = {});
    // An exchange was voiced: it counts against both and the place for a while, and does what talk does.
    void ambientSpoken(const AmbientPick& pick);
    std::string describeRegard(const std::string& holder, const std::string& other) const;
    std::vector<std::string> guardsOf(const std::string& town) const;
    // Paying off the bandits who have stopped this player (whichever of them `bandit` is).
    Result payBandits(const std::string& player, const std::string& bandit);
    bool hostile(const std::string& id) const;               // A bandit, still standing.
    std::int64_t banditDemand(const std::string& player) const; // What bandits are asking of them now (0: nothing).
    // Things that happened to a player that no action of theirs answered (a bandit's blow, say), since the last
    // call: (player, words), oldest first. The host shows them to the player.
    std::vector<std::pair<std::string, std::string>> takeNotices();
    std::vector<WorldEvent> takeEvents();
    std::size_t droppedEvents() const { return droppedEvents_; }
    // Route searches answered from the path cache, and searched (see findPath).
    std::pair<std::size_t, std::size_t> pathCacheStats() const { return {pathHits_, pathMisses_}; }
    // Brings these players' views up to date together, on several threads at once (each view is thousands of sight
    // rays, and one player's doesn't depend on another's). observe() and snapshot() then find them ready. Changes
    // nothing they would have seen; call it from the thread that owns the world, between ticks.
    void prepareViews(const std::vector<std::string>& observerIds) const;
    // Where tick() spends its time, in milliseconds, since the profile was last reset: the total and the worst single
    // tick for each part. For benchmarks and server logs; it changes nothing the simulation does.
    struct TickProfile
    {
        struct Part
        {
            double total = 0, worst = 0;
        };
        Part streaming, schedules, movement, separation, views;
        std::size_t ticks = 0, routeSearches = 0;
        std::size_t routeNodes = 0, largestRoute = 0;   // Nodes the route searches expanded, and the most in one.
        double slowestRoute = 0;                        // The longest one route search took (ms), and where.
        std::string slowestRouteCell;
        std::size_t slowestRouteNodes = 0, slowestRouteWaypoints = 0;
    };
    const TickProfile& tickProfile() const { return profile_; }
    void resetTickProfile() { profile_ = {}; }
    Result move(const std::string& id, double dx, double dy);
    Result moveTo(const std::string& id, double x, double y);
    Result face(const std::string& id, double x, double y);
    Result setPosture(const std::string& id, const std::string& posture);
    Result stop(const std::string& id);
    // Kills (true) or brings back (false) a character or NPC: stops them and lays them down, or stands them up.
    Result setDead(const std::string& id, bool dead);
    // Live changes (Dungeon Master): makes NPC `id` exactly as `candidate` (a fully validated world loaded from the
    // same build and the current live rows) has them: adds, updates or removes their body and their place in society.
    Result adoptResident(const World& candidate, const std::string& id);
    // Takes patrol routes and wander areas from `candidate`, re-planning every resident.
    void adoptLayers(const World& candidate);
    // Takes the factions and every place's faction claims from `candidate`.
    void adoptFactions(const World& candidate);
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
    // The same from text already in memory; `label` names it in messages.
    Result loadCellText(const std::string& text, const std::string& label);
    // Startup-only, atomic replacement from an exported Atlas manifest.
    Result loadWorldFile(const std::string& path);
    // The same from a build held in memory (e.g. read from the world database):
    // "world.ratw" plus the cell files it names, by relative path.
    Result loadWorldFiles(const std::map<std::string, std::string>& files, const std::string& label);
    // --- Streamed worlds ---------------------------------------------------------------------------------
    // A world too large to hold at once keeps every cell's header (name, size, position, weather...) but a
    // cell's tiles and seams only while someone is in it or next to it. The source gives them by cell ID.
    struct CellSource
    {
        // The cell file up to "grid:", which then says "size: W H"; returns a problem, or "".
        std::function<std::string(const std::string& id, std::string& header)> header;
        // The whole cell file, and its seams as manifest "door" records; returns a problem, or "".
        std::function<std::string(const std::string& id, std::string& cell, std::string& seams)> load;
    };
    // Set before loading a world whose manifest names cells with "area" records (RATW_WORLD 3).
    void setCellSource(CellSource source) { source_ = std::move(source); }
    bool streamed() const { return static_cast<bool>(source_.load); }
    // Brings a streamed cell's tiles and seams into memory (a no-op for a loaded cell).
    Result ensureLoaded(const std::string& cellId);
    // Loads every cell a character is in or next to; unloads the rest once unneeded for `idle` game seconds.
    void stream(double idle = 120.0);
    std::size_t loadedCells() const;
    const std::string& herbPatchCell() const { return herbCell_; }
    Vec2 herbPatchPosition() const { return herbPatch_; }

  private:
    // Reads one cell file named by the manifest into `text`; returns a problem, or "" on success.
    using CellReader = std::function<std::string(const std::string& relative, std::string& text)>;
    Result loadWorld(std::istream& input, const CellReader& readCell, const std::string& path);
    Result loadCell(std::istream& file, const std::string& path);
    // Reads a cell file into `out` (headerOnly: up to "grid:", for a streamed cell's header).
    Result parseCell(std::istream& file, const std::string& path, Cell& out, bool headerOnly) const;
    void unload(const std::string& cellId);
    CellSource source_;
    // A streamed world's cell graph through seams (explicit doors are always in doors_).
    std::map<std::string, std::set<std::string>> exits_;
    std::map<std::string, double> needed_;          // Game time each loaded cell was last in use.
    double streamCheck_ = 0.0;
    std::set<std::string> streamOccupied_;          // The cells characters stood in when stream() last looked.
    // Each cell's neighbours (through portals and seams), built on first use and dropped when doors change: a
    // city cell has well over a thousand seam and door records, far too many to walk every tick.
    mutable std::map<std::string, std::vector<std::string>> neighborCache_;
    const std::vector<std::string>& neighborList(const std::string& cellId) const;
    // firstSteps() for each starting cell, kept until doors change: residents ask it every half second.
    mutable std::map<std::string, std::map<std::string, std::string>> stepsCache_;
    // Connections known to lead nowhere: every way from the body of one cell into the next lands in a pocket of it
    // (found by residents, when both are in memory). Routes leave them out until either cell's ground changes.
    mutable std::set<std::pair<std::string, std::string>> deadEnds_;
    const std::map<std::string, std::string>& cachedSteps(const std::string& from) const;
    std::map<std::string, Cell> cells_;
    std::map<std::string, FactionDefinition> factions_;
    std::map<std::string, ChapterDefinition> chapters_;
    std::map<std::string, Entity> entities_;
    TickProfile profile_;
    std::vector<WorldEvent> events_;
    Bonds bonds_;
    std::vector<Promise> promises_;
    RoadsState roads_;
    std::vector<Town> towns_;
    std::map<std::string, std::string> townOfCell_;
    bool townsReady_ = false;
    std::map<std::string, std::vector<Belief>> beliefs_;   // By holder.
    CrimeState crime_;
    // The ambient director (RatwAmbient.cpp): when each resident and place last had an exchange (world seconds), and
    // what has happened in residents' lives lately (a few items each, for two game days).
    std::map<std::string, double> ambientLast_;
    std::map<std::string, std::vector<AmbientNews>> news_;
    std::map<std::string, std::vector<TownNews>> townNews_;      // By town (or region).
    struct PriceSeen
    {
        double factor = 1, day = -100;
        int dir = 0;                    // The last change worth talking of: +1 dearer, -1 cheaper.
    };
    std::map<std::string, std::map<std::string, PriceSeen>> priceSeen_;   // Town -> item.
    void noteNews(const WorldEvent& event);
    std::string townIdOf(const std::string& cellId) const;
    AmbientTopic ambientTopic(const std::string& a, const std::string& b);
  public:
    // The written scenes' conditions and blanks for where `who` stands, now (doc 30).
    void sceneMoment(const std::string& who, std::map<std::string, std::string>& tags, std::map<std::string, std::string>& blanks);
  private:
    void sceneTopics(const std::string& a, const std::string& b, const std::function<void(AmbientTopic)>& consider);
    // Schedules (RatwSchedules.cpp).
    std::vector<CalledFestival> festivals_;
    struct MarketSquare
    {
        bool found = false;
        Spot at;
        std::vector<Spot> stalls, crowd;
    };
    std::map<std::string, MarketSquare> squares_;                 // By community, worked out once a day.
    std::int64_t squaresDay_ = -1, plannedAt_ = -1;
    std::uint64_t festivalsChanged_ = 0, plannedFor_ = 0;
    std::set<std::string> festivalsBegun_;                  // "community|day": the festival's event was recorded.
    const MarketSquare& square(const std::string& community);
    int skyOf(const std::string& cellId) const;            // -1 indoors, 0 fair, 1 wet, 2 harsh.
    void planDays();
    std::int64_t crimeHour_ = -1;
    std::map<std::string, double> stealReady_;              // When each may try again.
    std::map<std::string, std::pair<std::string, double>> fights_;   // "attacker|target": the incident, the last blow.
    std::map<std::string, std::string> pursuits_;           // Guard: whom they're going to stop.
    struct Confrontation
    {
        std::string guard;
        double deadline = 0;
    };
    std::map<std::string, Confrontation> confrontations_;   // Players told to pay or come along.
    std::map<std::string, std::string> marks_;              // A hungry resident: whom they mean to take a meal from.
    std::map<std::string, std::int64_t> tried_;             // The day each last tried.
    Incident& openIncident(const std::string& kind, const std::string& offender, const std::string& victim);
    Incident* incident(const std::string& id);
    void witness(Incident& inc, double sleight);
    void weigh(Incident& inc);
    void reportTo(Incident& inc, Witness& w, const std::string& guard);
    bool willReport(const Witness& w, const Incident& inc) const;
    Result assault(const std::string& attacker, const std::string& target);
    void settleWarrant(const Warrant& w, const std::string& guard);
    bool gaolSpot(const std::string& town, std::string& cellId, double& x, double& y);
    void takeIntoCustody(const std::string& person, const std::string& town, const std::string& guard);
    void confront(const std::string& guard, const std::string& person);
    bool crimeErrand(const std::string& resident, std::string& task, std::string& reason, std::string& goalCell, Vec2& goal) const;
    void tendCrime();
    void crimeDaily();
    void chooseMarks();
    void gossip();
    void setupTowns();
    void tendRoads();
    void roadsDaily();
    std::vector<std::string> routeBetween(const std::string& from, const std::string& to) const;
    void rumoursFromEvent(const WorldEvent& event);
    void contractsFromEvent(const WorldEvent& event);
    void settleContract(Contract& c, const std::string& status, const std::string& paidTo);
    // Road folk: each one's part, keyed by entity ID ("road:...").
    struct RoadFolk
    {
        std::string kind, of;               // "caravan" or "bandit"; the caravan's or camp's ID.
        double hp = 16, share = 1, nextSwing = 0, nextPath = 0;
        std::string lastCell;
        double enteredAt = 0;               // When it came into its cell (a wagon stuck that long is moved on).
    };
    std::map<std::string, RoadFolk> folk_;
    // Bandits who have stopped a player: asking (a demand) until they pay, get clear, or it comes to blows.
    struct Encounter
    {
        std::string camp, player;
        std::int64_t demand = 0;
        double since = 0;
        bool fighting = false;
    };
    std::vector<Encounter> encounters_;
    std::map<std::string, double> spared_;          // Players bandits leave alone until then (world seconds).
    std::map<std::string, double> swingReady_;      // When each player may swing again.
    std::vector<std::pair<std::string, std::string>> notices_;
    std::vector<std::vector<std::string>> roadRoutes_;   // Every road between two towns (cells), for bandits.
    std::int64_t priceHour_ = -1;
    struct RouteBudget;
    void tendRoadFolk();
    void tendCaravan(Caravan& c, Entity& wagon, RoadFolk& f, RouteBudget& budget);
    void tendCamp(BanditCamp& camp, const std::set<std::string>& stage);
    const Town* town(const std::string& id) const;
    void caravanEntered(Caravan& c, const Entity& wagon);
    void caravanArrived(Caravan& c);
    Entity& addRoadFolk(const std::string& id, const std::string& name, const std::string& description,
                        const std::string& cellId, Vec2 at, const std::string& kind, const std::string& of);
    void removeRoadFolk(const std::string& id);
    bool withCaravan(const std::string& who, const Entity& wagon) const;
    Encounter* encounterWith(const std::string& player);
    void endEncounter(const std::string& camp, double spareFor);
    void clearCamp(BanditCamp& camp, const std::string& by);
    void beaten(Entity& player, BanditCamp& camp);
    BanditCamp* campOf(const std::string& banditId);
    void notice(const std::string& player, std::string words) { notices_.push_back({player, std::move(words)}); }
    void tendPrices();
    void residentsTakeWork();
    void tradeBetweenTowns();
    Caravan* sendCaravan(const Town& from, const Town& to, const std::map<std::string, int>& load);
    // Work on the road a resident has taken (a letter to carry, a caravan to guard): where it takes them now,
    // in place of the day's plan. False if they have none, or it waits (for the morning, say).
    bool errand(const std::string& resident, const ResidentLife& life, std::string& task, std::string& reason,
                std::string& goalCell, Vec2& goal) const;
    // Walking (onstage) or hopping (offstage) toward a goal, through the cells in between: what residents do to
    // follow their schedules, and what the road folk do.
    struct RouteBudget
    {
        int searches = 0;
        std::size_t expandedBefore = 0;
        int maxSearches = 6;                    // RouteSearchesPerUpdate
        std::size_t maxNodes = 100000;          // RouteNodesPerUpdate
        bool defer = false;                     // Residents denied a search wait in routeWanted_ for a later tick.
    };
    // Residents waiting to plan a route (Docs/Design/30, phase 5): planned a few a tick rather than all at once in a
    // schedule update, so a town setting off never stalls one tick.
    struct RouteWant
    {
        std::string task, goalCell;
        Vec2 target;
    };
    std::map<std::string, RouteWant> routeWanted_;
    std::string routeCursor_;
    void planWantedRoutes();
    void headFor(Entity& e, const std::string& task, const std::string& goalCell, Vec2 target, RouteBudget& budget);
    std::int64_t marriageWeek_ = -1;
    void tendPromises();
    void tendMarriages();
    // An open, walkable tile near (x, y) in a cell that no resident calls home, for someone new to sleep.
    bool freeSpotNear(const std::string& cellId, double& x, double& y);
    std::int64_t bondHour_ = -1, bondDay_ = -1;     // The last game hour and day bonds were tended.
    void bondsFromEvent(const WorldEvent& event);
    // The society's ledger entries since last time, as events (and so as bonds) now rather than at the next save.
    void absorbJournal();
    void tendBonds();
    CareerWorld careerWorld() const;
    void careerNotes(const std::vector<CareerNote>& notes);
    std::size_t droppedEvents_ = 0;
    bool tiered_ = false, tieredSet_ = false;
    // An offstage NPC's current hop: from where it stands to `to` (in its cell), done at `arriveAt`; a crossing then
    // puts it at `arrival` in `intoCell`. Kept only while the goal it was planned for stays the same.
    struct OffstageLeg
    {
        std::string goalCell;
        Vec2 goal;
        bool crossing = false;
        std::string intoCell;
        Vec2 to, arrival;
        double arriveAt = 0;
    };
    std::map<std::string, OffstageLeg> legs_;
    // Each streamed cell's seams (its side of every crossing into a neighbour), kept from the first time it loaded
    // so that offstage NPCs can cross it later without it being in memory.
    std::map<std::string, std::vector<Door>> seamAnchors_;
    std::set<std::string> stageCells() const;
    // Where an entity is, the cells across its open edges, and those behind doors near it: what it could see,
    // hear or step into soon. The stage is this around every player; streaming keeps it around everyone onstage.
    void nearCells(const Entity& e, std::set<std::string>& out) const;
    std::set<std::string> tierWanted_;              // Tiers: the cells to keep in memory, as of the last update.
    // Each cell's portals other than open-edge seams (doors, stairs): a city cell has thousands of seam records but
    // a few dozen of these. Built on first use; seams come and go with loading but these don't (rebuildFixtureIndex
    // drops them).
    mutable std::map<std::string, std::vector<const Door*>> portalsIn_;
    // Each cell's portals (seams included) by 2-tile square, for "is anyone at a doorway" (separate()); rebuilt when
    // the cell's door list changes.
    mutable std::map<std::string, std::map<std::pair<long, long>, std::vector<const Door*>>> portalGrid_;
    bool nearPortal(const std::string& cellId, Vec2 p, double within) const;
    void placeOnStage(Entity& e, const std::set<std::string>& stage);
    void moveOffstage(Entity& e, const std::string& task, const std::string& goalCell, Vec2 goal);
    std::vector<Entity*> separateScratch_;          // separate()'s grouping, kept to avoid reallocating each step.
    std::map<std::string, Door> doors_;
    // Only closable fixtures can occlude/collide. Hundreds of open authoring
    // seams must not turn each sight-ray sample into a full-world scan.
    using FixtureTiles = std::map<std::pair<int, int>, std::vector<std::string>>;
    std::map<std::string, FixtureTiles> blockingFixtures_;
    // lineOfSight() with the cell and its fixtures already found: a view casts thousands of rays in one cell.
    // fixtureMask, when given, marks the cell's fixture tiles in place of looking each one up in fixtures.
    bool lineOfSight(const Cell& cell, const FixtureTiles* fixtures, const std::vector<char>* fixtureMask, Vec2 from,
                     Vec2 to) const;
    // doors_ by the cell they stand in (pointers into doors_; map nodes never move).
    std::map<std::string, std::vector<Door*>> doorsIn_;
    // Adds a just-loaded cell's seams (already in doors_) to the index without rebuilding it (see ensureLoaded()).
    void indexSeams(const std::string& cellId, const std::vector<std::string>& seamIds);
    // For every cell, the first cell to step into on the way to each reachable cell, through unlocked portals.
    std::map<std::string, std::string> firstSteps(const std::string& from) const;
    std::map<std::string, std::map<std::string, CellMemory>> memories_;
    // Where tick() last took each player's view for their map memory (see tick()).
    struct ObservedTile
    {
        std::string cellId;
        int x = 0, y = 0;
    };
    std::map<std::string, ObservedTile> lastObserved_;
    // Explicit portal actions wait for posture changes, but are never saved.
    std::map<std::string, std::string> pendingPortals_;
    std::map<std::string, TravelState> travels_;
    // Transient route execution bookkeeping; never restored from a save.
    std::map<std::string, std::string> travelLegCells_;
    std::map<std::string, double> travelRetryAt_;
    std::map<std::string, double> pathRetryAt_;     // A resident whose path search failed waits before searching again.
    // Reused path-search buffers: a 256-tile cell's navigation grid has a million nodes, too many to allocate per
    // search. An entry counts only when its stamp is the current search's.
    struct NavScratch
    {
        std::vector<double> g;
        std::vector<int> previous;
        std::vector<std::uint32_t> seen, closed;
        // What a node's footprint stands on, worked out once per search (see findPath): the search in which it
        // was, whether any of it is blocked, whether any of it is a ramp, and its lowest and highest ground.
        std::vector<std::uint32_t> footprintSearch;
        std::vector<std::uint8_t> footprintFlags;
        std::vector<double> footprintLow, footprintHigh;
        std::uint32_t search = 0;
    };
    mutable NavScratch nav_;
    mutable std::size_t searchExpanded_ = 0;       // Nodes the path searches have expanded (a running count).
    // Which tiles of a cell can reach which (by the step rules, doors taken as open): a region number per tile,
    // kept per cell and rebuilt whenever the cell's ground has changed (a checksum of its tiles says so).
    struct Regions
    {
        std::uint64_t checksum = 0;
        std::vector<int> region;
        int main = -1;                              // The largest region: the body of the cell, beside its pockets.
        std::uint64_t checkedTick = 0;              // Ground doesn't change inside a tick: checksummed once a tick.
    };
    mutable std::map<std::string, Regions> regions_;
    std::uint64_t ticks_ = 0;                       // Ticks begun; `ticking_` while one runs.
    bool ticking_ = false;
    // The cell's region map, validated once a tick (outside a tick, on every call: it checksums every tile); null for
    // a cell without tiles.
    const std::vector<int>* regionMap(const Cell& cell) const;
    int regionAt(const Cell& cell, Vec2 point) const;
    int mainRegion(const Cell& cell) const;         // -1 without tiles.
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
    std::string herbCell_ = "exterior";
    Vec2 herbPatch_{17.5, 7.5};
    // Movement from `from` (nullptr: level ground at height 0). Heights are half-tile steps: a
    // half step is free, a full step needs a slope or stairs on either side, anything more is a ledge.
    bool passable(const std::string& cellId, Vec2 p, const Tile* from = nullptr) const;
    bool visiblePoint(const Entity& observer, Vec2 point) const;
    bool visiblePortal(const Entity& observer, const Door& door) const;
    // visiblePoint() for every tile of the observer's cell at once (1 = visible): the sight range is worked out once
    // and only tiles within it are traced, where testing each tile alone recomputes the light for every one.
    std::vector<char> visibleTileMask(const Entity& observer, const Cell& cell, double range) const;
    // Each player's view, kept until something that shapes it changes: the cell, where they stand, how far they can
    // see, or a door opening or closing there. `fresh` says whether it was just recomputed.
    struct View
    {
        std::string cellId;
        long long x = 0, y = 0, range = 0;
        std::uint64_t doors = 0, tiles = 0;
        std::vector<char> visible;
        bool remembered = false;            // Whether observe() has put this view into the map memory yet.
    };
    mutable std::map<std::string, View> views_;
    const std::vector<char>& viewOf(const Entity& observer, const Cell& cell, bool* fresh = nullptr) const;
    // The cache key viewOf() compares (everything but the tiles seen).
    View viewKey(const Entity& observer, const Cell& cell, double range) const;
    double sightRange(const Entity& observer) const;
    std::vector<Vec2> findPath(const Entity& actor, Vec2 goal, bool allowClosed = false) const;
    std::vector<Vec2> searchPath(const Entity& actor, Vec2 goal, bool allowClosed) const;   // findPath, uncached.
    // Paths already found, by everything a search depends on: the cell, its ground (the region checksum), which of
    // its doors are closed, the exact start and goal, and whether closed doors may be passed. Residents walk the same
    // ways every day (home, work, the shop); a repeat is a lookup. The same inputs always give the same path.
    struct PathKey
    {
        std::string cellId;
        double startX, startY, goalX, goalY;
        bool allowClosed;
        std::uint64_t ground, closed;
        bool operator<(const PathKey& o) const
        {
            return std::tie(cellId, startX, startY, goalX, goalY, allowClosed, ground, closed) <
                   std::tie(o.cellId, o.startX, o.startY, o.goalX, o.goalY, o.allowClosed, o.ground, o.closed);
        }
    };
    static constexpr std::size_t PathsKept = 16384;
    mutable std::map<PathKey, std::vector<Vec2>> pathCache_;
    mutable std::size_t pathHits_ = 0, pathMisses_ = 0;
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

Tile tileFromGlyph(char glyph); // Terrain rules shared by built-in and authored cells.
const TerrainInfo* terrainInfo(char code); // Null for a code the catalog does not know.
const std::vector<TerrainInfo>& terrainCatalog();
const char* weatherName(Weather value);
bool parseWeather(const std::string& name, Weather& out); // False for an unknown name; `out` is then unchanged.
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
