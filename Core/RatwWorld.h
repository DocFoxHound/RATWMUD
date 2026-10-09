#pragma once

#include <cstdint>
#include "RatwTogether.h"

#include <functional>
#include <iosfwd>
#include <limits>
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include "RatwAppearance.h"
#include "RatwBattle.h"
#include "RatwInjury.h"
#include "RatwCold.h"
#include "RatwPractice.h"
#include "RatwBonds.h"
#include "RatwCrime.h"
#include "RatwSchedules.h"
#include "RatwAmbient.h"
#include "RatwRoads.h"
#include "RatwCalendar.h"
#include "RatwSociety.h"

// Engine-independent, deterministic authoritative simulation. Positions are in
// terrain-tile units, with x right and y down. Facing is radians, east == zero.
namespace ratw::items
{
struct Item;
}
namespace ratw::wild
{
struct Species;
}

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
// The regional weather field (Docs/Design/29-client-polish.md, phase 7): weather systems that drift across the world
// on the prevailing wind, strongest at their middle and fading toward their edge, born and dying over hours.
struct WeatherSystem
{
    std::string id;                     // "<anchor cell>:<slot>" for the world's own; "front-<n>" for one called up.
    Weather kind = Weather::Rain;
    double x = 0, y = 0;                // Where its middle was when it was born (world tiles).
    double radius = 300, peak = 1;      // Reach in tiles; strength at its middle, 0..1.
    double vx = 0, vy = 0;              // Drift, tiles a game day.
    double born = 0, life = .5;         // Calendar days.
};
// The weather at a point: the strongest kind there and how strong (0..1), and the next strongest (where two meet).
struct WeatherSample
{
    Weather kind = Weather::Clear;
    double intensity = 0;
    Weather second = Weather::Clear;
    double secondIntensity = 0;
};

struct Environment
{
    double hour = 12.0, daylight = 1.0, illumination = 1.0;
    double sight = 1.0, hearing = 1.0, scent = 1.0, movement = 1.0;
    std::string phase = "day";
    double artificialLight = 0.0, daylightAccess = 1.0, glowStrength = 0.0;
    std::string lightingTone = "neutral", lightSource = "daylight";
    calendar::Calendar date;
    Weather weather = Weather::Clear;   // Where it was asked for: the kind, and how strong (0 indoors).
    double intensity = 0;
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
    // In the header so it inlines: sight and walking ask for tiles millions of times a second (doc 31, Phase 4).
    const Tile* tile(int x, int y) const
    {
        if (x < 0 || y < 0 || x >= width || y >= height)
            return nullptr;
        const auto index = static_cast<std::size_t>(y * width + x);
        return index < tiles.size() ? &tiles[index] : nullptr;
    }
    Tile* tile(int x, int y) { return const_cast<Tile*>(static_cast<const Cell&>(*this).tile(x, y)); }
};

struct FactionDefinition { std::string id, name, color; };
struct ChapterDefinition { std::string id, name; };
// A place to let, as Atlas authors it (Docs/Design/32, 5.2): kind "hall" or "warehouse", a landlord (a resident or
// "treasury"), the rent in pennies a game week, and the Chapter level it needs.
struct Letting
{
    std::string cell, kind, landlord;
    std::int64_t rent = 0;
    int level = 2;
};

// Who made or gave a player's goods (Docs/Design/55-letters-gifts-favours.md, 3 and 4): beside the stack, not in the
// item's id. `count` of `item` came from `maker` (made `madeDay`) and/or `giver` (given `givenDay`); "" for none.
// An occasion residents host (doc 55, 6): a wedding, a funeral; where and when, its hosts, the players invited, who
// said they'd come, and who stood witness. Hosts are sent there by errand (World::errand).
struct Occasion
{
    std::string id, kind, community, cell;
    double x = 0, y = 0, start = 0, end = 0;        // Calendar days.
    std::vector<std::string> hosts;
    std::set<std::string> invited, coming, witnesses;
    bool invitesSent = false;
};

struct ScentRecord
{
    std::string item, maker, giver;
    int count = 0;
    double madeDay = -1, givenDay = -1;
};

struct Entity
{
    // (First, together: what every tick's passes over all entities look at, so a pass over a world of residents who
    // are all offstage reads one cache line of each. Each is described where it was: below, by its neighbours.)
    bool npc = false;
    bool dead = false;                            // Dead characters lie where they fell and cannot move or act until
                                                  // brought back (Dungeon Master).
    // Simulation tier (see World::setTiered), never saved: an NPC far from every player lives its day in timed
    // steps from one known-good place to the next instead of walking every tile.
    bool offstage = false;
    bool transient = false;                       // Folk of the road (a caravan's wagon, bandits), made from the roads'
                                                  // state as needed and never saved themselves.
    bool lingering = false;                       // (With the fights, below.)
    bool clientWalks = false;                     // (With free movement, below.)
    double hurt = 0.0, downedLeft = 0.0;          // (Described below, where `hurt` was.)
    double struggleUntil = 0.0, tendUntil = 0.0;  // Getting up, and tending someone, out of a fight (not saved).
    std::string gift;                             // (With `mouth`, below.)
    std::string id, name, cellId;
    Vec2 position, velocity;
    double facing = 0.0;
    // Stationary turns are authoritative, shortest-arc, and rate limited.
    double turnTarget = 0.0;
    bool turning = false;
    double hearing = 1.0, vision = 1.0;
    double earHealth = 1.0, eyeHealth = 1.0;
    double sneakSkill = 0.0, hearingSkill = 0.0; // Skill values in [0, 100].
    double smell = 1.0, noseHealth = 1.0, scentSkill = 0.0;
    double dexterity = 50.0, stamina = 100.0; // Server-owned, [0,100].
    int pace = 0;                             // Requested travel pace, 0 (walk) .. 10 (sprint).
    bool exhausted = false;                   // Recovery hysteresis; never a client speed override.
    double staminaRate = 0.0;                 // Last simulation step's net change per second.
    int age = 18;
    Cold<Appearance> appearance;              // (Out of line: RatwCold.h. `->` to read a part.)
    double strength = 50.0, wisdom = 30.0;
    // What carrying asks of a player (doc 35, 1.2): the fastest pace its load allows and how much faster running tires
    // it. Worked out from its purse every tick (World::refreshLoad); not saved. Residents carry freely.
    int loadPace = 10;
    double loadDrain = 1.0;
    // Acute and lasting injuries (Docs/Design/38-injuries.md, phases 3 and 4): players' only, saved with them. What they
    // do is worked out from the list where it matters (injury::effects).
    std::vector<Injury> injuries;
    double lastBirthdayDay = -1.0; // Legacy/new records anchor on first observation of the shared calendar.
    int ageNoticePending = 0;
    std::string posture = "standing", state, description, activity, leaderId;
    // A rise never translates the actor; retained movement resumes afterward.
    double postureRemaining = 0.0;
    std::string postureTarget;
    int speakingColor = 0;
    // `hurt` (at the top): how badly hurt (0..100): health is 100 − hurt. At 100 a character is Downed
    // (Docs/Design/33-combat.md): lying, with `downedLeft` seconds until a player gets up (doc 38), or an NPC dies,
    // unless they struggle up (once a game day) or someone tends them.
    double recoveryUsed = -1.0;               // The game day (whole) the self-recovery was last used; -1 never.
    // Rest (doc 38): a player's downings since their last full rest, which stretch the next one; the rest they have had
    // without a break, anywhere (`restRun`) and lying in a bed (`bedRun`), in game hours; the calendar day of their last
    // full rest. `awaySince`: the calendar day they left the world, while gone (-1 while here), and whether they left
    // lying in a bed.
    int downsSinceRest = 0;
    double restRun = 0.0, bedRun = 0.0, fullRestDay = -1.0, awaySince = -1.0;
    bool awayInBed = false;
    double leftAt = 0;                              // When its player last left the world (Unix seconds; doc 56, 10). Saved.
    std::string tending;
    // Fights (doc 33): what is held in the mouth ("" or "sword"); a Gift ("" or "fire"), Quickened or not, and its mana;
    // fighting skill (0..100: NPCs' comes from their trade, a player's grows by fighting).
    // All saved. `lingering`: the player has gone but their body stays in a fight a while.
    std::string mouth;
    double wardenAttention = 0;     // Quickened magic others saw (doc 43): for the Wardens, later. Saved.
    // Keeping the secret (doc 53, 4): Quickened wolves whose magic this wolf saw as a partner (the wolf to the day last
    // seen), those it has told the Wardens of (once each), and the day it last vouched for one. Saved; IDs only.
    std::map<std::string, double> witnessed;
    std::set<std::string> toldWardens;
    double vouchedDay = -1e9;
    // Vouching's risk (doc 53, 4; doc 52's shape): whom it last vouched for, and its standing with the Wardens, which
    // falls if that wolf draws their attention again within the month. At -2 they no longer take its word. Saved.
    std::string vouchedFor;
    double wardenStanding = 0;
    // Where its letters wait (doc 55, 2): the town it last left the world in or had a full rest in. Saved.
    std::string postTown;
    // Whose scent its goods carry (doc 55, 3 and 4): at most 60 records, newest last; players only. Saved.
    std::vector<ScentRecord> scents;
    // Grooming (doc 55, 7), in calendar days: Well-groomed until (24 game hours, or the next full rest), whether at half
    // (self-groomed: 2 game hours), its lesser scent until (2 game hours), by whom; the day it last groomed another and
    // itself. Saved.
    double groomedUntil = -1, groomScentUntil = -1, groomedOtherDay = -1e9, groomedSelfDay = -1e9;
    bool groomHalf = false;
    std::string groomedBy;
    // Meals (doc 55, 6), in calendar days: Fed until (2 game hours, 4 for a shared meal), and when it last ate. Saved.
    double fedUntil = -1, ateAt = -1;
    // Away at an inn (doc 54, 1): it left the world in an inn's common room or a bed it has there; rested practice
    // builds half again as fast for that absence. Saved.
    bool awayAtInn = false;
    // Work Gifts in use (doc 43), until these world seconds: Lighten Load (carries half again), Carry (speech carries
    // as a yell). Not saved: they are short.
    double lightLoadUntil = 0, carryVoiceUntil = 0;
    // What is worn (doc 35, 1.1): a wear slot ("head", "neck", "chest_left"...) to a catalog item, and each piece of
    // jewellery with the fur spot it is clipped at. Saved. The goods stay in the purse; wearing marks them.
    std::map<std::string, std::string> worn;
    // Which sword is in the jaws when `mouth` is "sword" (doc 35, Part 4: "sword~fine"; "" for the common one), and how
    // much use each kind of gear in service has had (by item id: wear and tear, doc 35 phase 9). Saved.
    std::string swordKind;
    std::map<std::string, double> wear;
    std::map<std::string, double> gameSkills;  // Tavern games (doc 54, 5): 0..100 a game, grown by playing. Saved.
    // The journal (doc 54, 7): lore fragments read at archives, in order; species brought down (first day, count);
    // forage goods found (first day, "season, ground"); the cells it has been in. Saved.
    std::vector<std::string> lore;
    std::map<std::string, std::pair<double, int>> bestiary;
    std::map<std::string, std::pair<double, std::string>> herbarium;
    std::set<std::string> places;
    double scentMaskedUntil = 0;    // Masking oil (doc 35): its scent, and what it carries, hidden until then (world seconds). Saved.
    bool noPvp = false;             // Auto-decline fights with players (doc 40's fight start): no one may challenge them. Saved.
    // Doc 53: "Allow hunting partners" and "Allow work partners" off (on by default). Saved.
    bool noHuntPartners = false, noWorkPartners = false;
    std::vector<std::pair<std::string, std::string>> jewellery;     // (spot, item)
    bool quickened = false;
    bool dungeonMaster = false;                   // A player a Dungeon Master marked as one: they have the Dev Console.
    double mana = 0.0, fightingSkill = 50.0;
    bool typing = false;
    double speakingUntil = 0.0;
    std::vector<Vec2> path;
    Vec2 input;
    // Set by an accepted transition; UI must release/reissue held movement.
    bool transitioned = false;
    // Free movement (Docs/Design/31-responsiveness.md, Phase 3), never saved: `clientWalks` (at the top), the player's
    // client walks this wolf and says where it is (World::placeByClient), checked against walking's rules; the world no
    // longer walks it.
    double clientMoved = 0;                       // Distance accepted since the last tick (for stamina).
    double poseBudget = 0, poseRefilled = -1;     // How far poses may still go, refilled at walking speed.
    double lastPoseAt = -1;                       // When the last pose was accepted.
    int poseStrikes = 0;                          // Poses refused since the last one accepted.
    std::uint32_t poseSeq = 0;                    // The last pose accepted.
    std::uint32_t inputSeq = 0;                   // Held movement: the last input applied.
    // Practice (Docs/Design/49-characters-and-earned-gifts.md): a player's grades (an attribute to "weak" or "strong";
    // plain when absent) and specialty, chosen at creation (its Phase 4); trade skills by family ("craft", "labour"...);
    // stamina the attribute (`endurance`, which Phase 4 puts to work); which of the plan's migrations it has had; and
    // what practice keeps (RatwPractice.h). Saved, but for the parts PracticeState says aren't.
    std::map<std::string, std::string> grades;
    std::string specialty;
    std::map<std::string, double> skills;
    double endurance = 50.0;
    int progressVersion = 0;
    Cold<PracticeState> practice;             // (Out of line: RatwCold.h.)
};

// The world's entities by ID: a std::map that also keeps a list of them in ID order, for the passes over all of them
// every tick (World::tick; Docs/Design/31-responsiveness.md, "Fast-forward"). Walking a list of pointers is many times
// quicker than walking the tree. The list is made again after anything is added or taken away, so a pass that adds or
// takes away entities walks the map itself.
class EntityMap : public std::map<std::string, Entity>
{
    using Base = std::map<std::string, Entity>;

  public:
    EntityMap() = default;
    EntityMap(const EntityMap& other) : Base(other) {}
    EntityMap(EntityMap&& other) noexcept : Base(std::move(other)) { other.stale_ = true; }
    EntityMap& operator=(const EntityMap& other)
    {
        Base::operator=(other);
        stale_ = true;
        return *this;
    }
    EntityMap& operator=(EntityMap&& other) noexcept
    {
        Base::operator=(std::move(other));
        stale_ = other.stale_ = true;
        return *this;
    }
    Entity& operator[](const std::string& id)
    {
        const auto before = size();
        auto& e = Base::operator[](id);
        stale_ = stale_ || size() != before;
        return e;
    }
    Entity& operator[](std::string&& id)
    {
        const auto before = size();
        auto& e = Base::operator[](std::move(id));
        stale_ = stale_ || size() != before;
        return e;
    }
    template <class... Args>
    auto emplace(Args&&... args)
    {
        stale_ = true;
        return Base::emplace(std::forward<Args>(args)...);
    }
    template <class... Args>
    auto try_emplace(Args&&... args)
    {
        stale_ = true;
        return Base::try_emplace(std::forward<Args>(args)...);
    }
    template <class... Args>
    auto insert(Args&&... args)
    {
        stale_ = true;
        return Base::insert(std::forward<Args>(args)...);
    }
    template <class... Args>
    auto erase(Args&&... args)
    {
        stale_ = true;
        return Base::erase(std::forward<Args>(args)...);
    }
    void clear() noexcept
    {
        Base::clear();
        stale_ = true;
    }
    // Every entity in ID order, to walk like the map (`for (auto& [id, e] : entities_.inOrder())`), by a pass that
    // neither adds nor takes away any.
    struct InOrder
    {
        struct It
        {
            value_type* const* at;
            value_type& operator*() const { return **at; }
            It& operator++()
            {
                ++at;
                return *this;
            }
            bool operator!=(const It& other) const { return at != other.at; }
        };
        value_type* const* first;
        value_type* const* last;
        It begin() const { return {first}; }
        It end() const { return {last}; }
    };
    InOrder inOrder() const
    {
        if (stale_)
        {
            order_.clear();
            order_.reserve(size());
            for (auto& entry : const_cast<EntityMap&>(*this))
                order_.push_back(&entry);
            stale_ = false;
        }
        return {order_.data(), order_.data() + order_.size()};
    }

  private:
    mutable std::vector<value_type*> order_;
    mutable bool stale_ = true;
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
    // A snapshot taken without tiles (World::snapshot's `withTiles` false: doc 31, Phase 4) leaves `cell` its header
    // and points at the live cell and the observer's memory of it instead, for the caller to read at once.
    const Cell* source = nullptr;
    const CellMemory* memory = nullptr;
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
    std::vector<WeatherSystem> fronts;  // Weather called up by the DM or a developer (the world's own isn't saved).
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
    // A new player placed at a given cell and spot instead of the spawn (doc 52's start towns).
    Entity& addPlayer(const std::string& id, const std::string& name, const std::string& cell, Vec2 position);
    // Where a new character arrives in a town (doc 52, 1): the spawn for the town that holds it; else beside the town's
    // market merchant; else `cell` and `position` (the data file's) where someone could stand there. False if none
    // will do, or there is no such town: then the spawn.
    bool arrivalIn(const std::string& townId, std::string& cellOut, Vec2& positionOut, const std::string& cell = {},
                   Vec2 position = {});
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
    // The entities in a cell (doc 31, Phase 4): an index rebuilt each tick, and whenever one is added, erased or goes
    // through a door, so a view or a motion frame looks at its own cell, not the whole world. A caller still skips one
    // whose cellId is no longer that cell (moved by something the index didn't hear of, until the next rebuild).
    const std::vector<const Entity*>& entitiesIn(const std::string& cellId) const;
    // Builds the lazy indexes views read (entitiesIn's, the doors in each cell), so views built on several threads at once
    // only read them (doc 31, Phase 4). Call it after the world last changed and before the views.
    void prepareReading() const;
    // How work is spread over threads (the game's pool: doc 31, Phase 4): run(count, job) calls job(i) for every i
    // below count and returns when all are done. Unset, the world makes its own threads where it uses them.
    using Parallel = std::function<void(std::size_t, const std::function<void(std::size_t)>&)>;
    void setParallel(Parallel run)
    {
        parallel_ = std::move(run);
        society_.setParallel(parallel_);            // (The residents decide on it too.)
        society_.setOrchestratorThread(bool(parallel_));   // (And the economy orchestrator plans on a thread of its own.)
    }
    // observe() for several observers at once (on the parallel runner): each writes only its own memory and view.
    void observeAll(const std::vector<std::string>& observerIds);
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
    const std::map<std::string, Letting>& lettings() const { return lettings_; }
    Result relocateResident(const std::string& npc, const std::string& destination, double x, double y);
    // A Dungeon Master's move (doc 34, the LIVE map): someone in the world put straight onto an open tile of any place,
    // where they carry on. Not someone in a fight, or a tile they couldn't stand on.
    Result teleport(const std::string& id, const std::string& cellId, double x, double y);
    // Temporary folk a Dungeon Master brings in from the LIVE map (doc 34): a stranger standing where put, who leaves
    // when their time is up (1 minute to a day of world time). Never saved; looked at, not talked to (yet: briefs are
    // doc 34 Part 6). `look` is how they look, often a copy of a resident's.
    Result addVisitor(const std::string& id, const std::string& name, const std::string& description, const Appearance& look,
                      const std::string& cellId, double x, double y, double minutes);
    Result sendVisitorAway(const std::string& id);
    // A visitor a storyteller brought on stage (doc 58, 5): fights, theft, trade and recruiting refuse it; the Mind never
    // answers it; Look marks it. (A DM's visitors share the refusals.)
    void markStoryVisitor(const std::string& id) { storyVisitors_.insert(id); }
    bool isStoryVisitor(const std::string& id) const { return storyVisitors_.count(id) > 0; }
    // When a temporary visitor leaves (world seconds), or a negative number for anyone else.
    double visitorLeaves(const std::string& id) const { const auto v = visitors_.find(id); return v == visitors_.end() ? -1 : v->second; }
    double worldTime() const { return time_; }
    // Whether a resident could walk from one place to another, through unlocked portals.
    bool canWalkBetween(const std::string& from, const std::string& to) const { return from == to || cachedSteps(from).count(to) > 0; }
    Result advanceCalendar(double days); // Explicit developer/test jump, never a client-authorized normal action.
    Result useSeasonalWeather(const std::string& cellId);
    Result trade(const std::string& player, const std::string& merchant, const std::string& item, int quantity, bool buy);
    Result gather(const std::string& player);
    // Hunting and foraging (Docs/Design/41-hunting-and-foraging.md; RatwHunt.cpp). Out in the wild a wolf may forage
    // the ground beside it, or set out to hunt: a fight in an arena whose other side is animals, found by the ground
    // there, how hard it has been hunted lately and how many are playing. Only a hunter's party or Chapter may join.
    struct WildHere
    {
        bool hunt = false, forage = false;
        std::string huntWhy, forageWhat;            // Why not; what is to be foraged here ("the trees, the grass").
    };
    WildHere wildAround(const std::string& player) const;
    // `only`: the game to find (species ids), for the Dev Console's /hunt and tests; empty, what the ground gives.
    Result startHunt(const std::string& player, const std::vector<std::string>& only = {});
    // Hunting together (Docs/Design/53-hunting-and-working-together.md, Phase 2). Anyone may join a hunt while its
    // starter allows hunting partners (the user: only the starter's setting counts); its party, Chapter and companions
    // always; never one blocked by a hunter. A closed hunt may be asked into (30 s for a hunter to let them in), and a
    // hunter may invite a wolf within 20 tiles. A kill is shared equally among those taking part.
    struct HuntAsk
    {
        std::string battle, from;
        double until = 0;
    };
    struct HuntShare                                // One's share of one's last hunt, for Give my share (the end card).
    {
        std::string battle;
        std::map<std::string, int> goods;
        std::vector<std::string> hunters;           // The others who took part, to give it to.
        double until = 0;
    };
    bool mayJoinHunt(const Battle& b, const std::string& id) const;
    bool mayAskHunt(const Battle& b, const std::string& id) const;
    Result askToJoinHunt(const std::string& id, const std::string& battleId);
    Result answerHuntAsk(const std::string& hunter, const std::string& asker, bool letIn);
    Result inviteToHunt(const std::string& hunter, const std::string& target);
    Result giveHuntShare(const std::string& from, const std::string& to);
    Result setPartners(const std::string& id, const std::string& kind, bool on);
    void setBlocked(std::function<bool(const std::string& a, const std::string& b)> blocked) { blocked_ = std::move(blocked); }
    // Party mates (doc 32), wired by Game: partners who see a Quickened wolf's magic keep its secret (doc 53, 4).
    void setPartnered(std::function<bool(const std::string& a, const std::string& b)> partnered) { partnered_ = std::move(partnered); }
    bool partners(const std::string& a, const std::string& b) const;
    // At a resident of the Warden Order (Game checks who): tell the Wardens of a Quickened wolf one saw (+3 attention, once
    // a witness and wolf), or vouch for it (-1, once a game month a witness).
    Result tellWardens(const std::string& witness, const std::string& wolf);
    Result vouchToWardens(const std::string& witness, const std::string& wolf);
    // A Quickened wolf's Warden attention rose: whoever vouched for it within the month loses standing with them.
    void attentionRose(const std::string& wolf);
    const std::vector<HuntAsk>& huntAsks() const { return huntAsks_; }
    std::vector<std::string> huntNearby(const std::string& hunter) const;
    std::string huntStarterOf(const std::string& battleId) const;
    int huntTaken(const Battle& b) const;
    const HuntShare* huntShareOf(const std::string& id) const;
    bool huntInvited(const std::string& battleId, const std::string& id) const;
    // Working together (doc 53, 2; RatwTogether.cpp): a wolf at work (foraging lately, or in a joint) may be lent a paw
    // by one within 6 tiles while its Allow work partners is on, or when it asked them; never between a blocked pair.
    // Each member works at the joint's rate (together::rate) and the goods are shared exactly.
    Result lendAPaw(const std::string& helper, const std::string& worker, const std::string& role = {});
    Result askToLend(const std::string& worker, const std::string& target);
    Result leaveWork(const std::string& id);
    bool atWork(const std::string& id) const;
    bool mayLend(const std::string& helper, const std::string& worker) const;
    bool askedToLend(const std::string& worker, const std::string& helper) const;
    const together::JointWork* jointOf(const std::string& id) const;
    double workRate(const std::string& id) const;
    double workLabour(const std::string& jointId) const;
    const std::map<std::string, together::JointWork>& joints() const { return joints_; }
    // Farm work (doc 53, 2.6): the work a resident farmer is at in its post this season (the harvest in autumn,
    // threshing in winter), or null; a player near starts or joins it beside the farmer; each beat (a spell) a player
    // who was there brings in its rate times a share of a spell's yield for the farm and is paid the town's hand wage a
    // spell times its rate, from the farm's till. No work when the till can't pay or the barn is full.
    const together::Activity* residentWorkAt(const std::string& farmer) const;
    Result helpAtWork(const std::string& player, const std::string& farmer);
    // A player did something (moved, spoke, any command): farm work, paid by the beat, needs a hand who is there.
    void noteActive(const std::string& player);
    // Gifted and Quickened at work (doc 53, 4): a work Gift the joint's activity takes (Winnow and Dry at threshing)
    // lifts it for a beat, on an angle of its own; Keep watch, in a joint in the wild, is an angle any wolf may take
    // (a Quickened wolf's): a creeping bandit must get past the watcher too, and it can't be taken unawares.
    bool jointTakesGift(const std::string& id, const std::string& ability) const;
    Result giftOnJoint(const std::string& id, const std::string& ability);
    Result keepWatch(const std::string& id, bool on);
    bool keepingWatch(const std::string& id) const;
    // Talker and doer (doc 53, 4): a resident a wolf talks to faces it until `seconds` after the last line, standing
    // still; so a partner behind the resident goes unseen (a theft's watchfulness, doc 40's cone).
    void faceTalker(const std::string& resident, const std::string& talker, double seconds = 20);
    std::string facingTalker(const std::string& resident) const;
    void tendTalkers();
    // The wolves keeping watch over `id` (in its joint), itself first if it is one.
    std::vector<std::string> watchersOver(const std::string& id) const;
    Result forage(const std::string& player);
    Result leaveHunt(const std::string& player);     // Gives up the hunt, wherever one stands in it.
    // Wear and tear (doc 35; RatwDurability.cpp): how much use a good takes (0: it doesn't wear), how much is left of
    // the one in service (1 new, 0 worn out), which sword is in the jaws ("" for none), and mending at a shop.
    // The nose (RatwMarks.cpp): how keen one is (the physical `smell`, its health and the skill; it sharpens with use),
    // whether a wolf's scent is masked, masking it, and the masterworks a nose makes out on those near by.
    static double noseAcuity(const Entity& e);
    bool scentMasked(const Entity& e) const;
    // Grooming's effects (doc 55, 7): 1 groomed by another, 0.5 self-groomed, 0 not; and the wolf's scent scale (0
    // masked, 0.6 groomed by another, 0.8 self-groomed, else 1), which multiplies how far it is smelt.
    double groomedFactor(const Entity& e) const;
    // A scent record on a player's goods (doc 55, 3 and 4): `quantity` of `item` made by `maker` and/or given by
    // `giver`; at most 60, oldest dropped. Residents keep none.
    void addScent(const std::string& who, const std::string& item, int quantity, const std::string& maker, const std::string& giver);
    // Meals (doc 55, 6): Fed (stamina back a tenth faster, rest healing 5% faster); shared with a wolf within 2 tiles who
    // ate in the last 10 game minutes, it lasts twice as long, and the two grow closer once a game day.
    double fedFactor(const Entity& e) const { return e.fedUntil > calendarDays_ ? 1. : 0.; }
    void fed(Entity& e);
    // Occasions (doc 55, 6; RatwOccasions.cpp): planned from a marriage (the next Restday, 11:00 at the church) or a death
    // (the next morning, 10:00); invitations are Game's; the invited present at the hour stand witness (the hosts warm
    // to them by 5).
    const std::vector<Occasion>& occasions() const { return occasions_; }
    Occasion* occasion(const std::string& id);
    void planOccasion(const std::string& kind, const std::vector<std::string>& hosts, const std::string& about);
    void tendOccasions(double dt);
    double scentScale(const Entity& e) const;
    // A grooming done: `groomer` groomed `groomed` (or itself). Well-groomed, the lesser scent, a severe acute injury
    // licked clean, and the bond for both (once a pair a game day).
    void applyGrooming(const std::string& groomer, const std::string& groomed);
    void trainNose(const std::string& id);
    Result maskScent(const std::string& player);
    struct MarkSmelt
    {
        std::string holder, item;                   // Who carries it, and the marked good ("sword~masterwork@sorrel").
    };
    std::vector<MarkSmelt> marksSmelt(const std::string& player) const;
    static int durabilityOf(const std::string& item);
    double conditionOf(const Entity& e, const std::string& item) const;
    static std::string swordHeld(const Entity& e);
    // The blade in the jaws, as the catalog has it (doc 47: its blow, what a swing costs), or null with none held.
    static const items::Item* bladeHeld(const Entity& e);
    // The watch's issue (doc 47): a guard on the job wears the Professional kit, and in a fight holds an iron sword; one
    // no longer of the watch gives it back. Not theirs: never in the purse, so never sold, looted or left on the ground.
    void kitOut(Entity& e, bool fighting);
    bool canRepair(const std::string& merchant, const std::string& item) const;
    std::int64_t repairCost(const Entity& e, const std::string& item) const;
    Result repairGear(const std::string& player, const std::string& merchant, const std::string& item);
    // Tracks (doc 41): smelling the ground out in the wild finds game trails near by, as faint marks on the map, for a
    // while. A hunt begun near a trail is likelier to find what made it (and, a fresh one, sure to).
    struct Track
    {
        std::string cell, species;
        std::vector<std::pair<int, int>> tiles;
        bool fresh = false;
        double until = 0;                           // World seconds.
    };
    Result smellTracks(const std::string& player);
    std::vector<Track> tracksOf(const std::string& player) const;   // In the wolf's own cell, still to be seen.
    // An animal in a hunt: its species ("" for anyone else), and whether it has yet to notice a hunter (doc 40's seam).
    std::string animalOf(const std::string& id) const;
    bool animalUnaware(const std::string& id) const;
    // Doc 53: an animal's state ("grazing", "watching", "fleeing", "calming"; "" for none), whom it watches, and the
    // chance it dodges a bite from this wolf (or −1 for one that doesn't run: the fight's own odds), with why in words.
    std::string animalState(const std::string& id) const;
    std::string animalWatching(const std::string& id) const;
    double huntDodge(const BattleFighter& biter, const BattleFighter& animal, std::string* why = nullptr) const;
    bool huntWaiting(const std::string& hunter) const;
    // Who counts as a hunter's friend, to join their hunt (the game knows parties and Chapters).
    void setFriends(std::function<bool(const std::string& a, const std::string& b)> friends) { friends_ = std::move(friends); }
    // How hard a cell has been hunted lately: kills, fading over the days (doc 41).
    double huntPressure(const std::string& cellId) const;
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
    // A host's watcher, told of every event as it is recorded (doc 55: residents' letters come from deeds, not scans).
    void setEventWatcher(std::function<void(const WorldEvent&)> watcher) { eventWatcher_ = std::move(watcher); }
    // Names (doc 56, 6): the name `knower` holds for a player character, or "" (the game's: the world doesn't keep
    // names). Talk never speaks a player's true name unless the speaker was given it.
    void setNamer(std::function<std::string(const std::string& knower, const std::string& subject)> namer) { namer_ = std::move(namer); }
    // Deeds in talk (doc 56, 5): for a deed claim ("deed:<id>") a teller holds about `subject`: {how the teller names
    // them (a name or a look), the deed's phrase, a nickname or ""}; an empty phrase when there is nothing to tell.
    struct DeedWords
    {
        std::string subject, phrase, nickname;
        bool byName = false;
    };
    void setDeedWords(std::function<DeedWords(const std::string& teller, const std::string& claim, const std::string& subject)> words)
    {
        deedWords_ = std::move(words);
    }
    // How everyone regards everyone else (see RatwBonds.h). The world moves them by rule, from the events it records
    // and from time spent together; a host may add a conversation's small, clamped nudge.
    const Bonds& bonds() const { return bonds_; }
    Bonds& bonds() { return bonds_; }
    // A player asks the NPC to take them on as an apprentice in the NPC's trade (see Society::apprentice).
    Result apprentice(const std::string& player, const std::string& master);
    // A player's help with a resident's trouble (doc 57, 3): a vacant post given to a resident now, a youth apprenticed now
    // to a position's holder (Society::appoint, apprenticeTo), each logged as the careers' own notes are. ok, or why not.
    Result appointResident(const std::string& positionId, const std::string& resident);
    // Town projects standing (doc 57, Phase 4; the game hands them over when they change): what each does where, at its
    // strength (1; 0.5 worn under half; none at 0). A watch post counts +2 guards a strength against a raid in its cell
    // and the next, and no bandit camp gathers within two cells of it; a market cover keeps its town's stalls out in foul
    // weather (worn, every other day); within a cell of a waystation caravans press on through a storm.
    struct StandingWorks
    {
        std::map<std::string, double> watchposts;   // Cell -> strength.
        std::map<std::string, double> covers;       // Community -> strength.
        std::map<std::string, double> waystations;  // Cell -> strength.
        bool operator==(const StandingWorks& o) const { return watchposts == o.watchposts && covers == o.covers && waystations == o.waystations; }
    };
    void setStandingWorks(StandingWorks works) { works_ = std::move(works); }
    const StandingWorks& standingWorks() const { return works_; }
    // What a town's projects are proposed from (doc 57, Phase 4): the market days this season its stalls stayed in for foul
    // weather, and the days caravans bound for it waited out storms, by the cell they waited in.
    int foulMarketDays(const std::string& community) const;
    // A camp's odds of robbing a caravan with so many guards in a cell (a watch post's two guards counted), and whether a
    // camp may gather in a cell (none within two cells of a watch post).
    double raidOdds(double strength, double hunger, int guards, const std::string& cellId) const;
    // Trade that moves prices (doc 57, 5): goods a player sells or hands in where the town is short of them (its store
    // under two days' meals or herbs; its shops under a shop's keeping of anything else) are noted, at most 32 a town;
    // when that good's going price there falls a tenth or more within a week, the note becomes the cause the town talks
    // of. A wolf who brings a day's food for a tenth of a town in "empty shelves" has fed the town ("fed the town").
    struct TradeNote
    {
        std::string item, who;
        int quantity = 0;
        double day = 0, price = 0;                  // When, and the going price then.
    };
    struct PriceCause
    {
        std::string item, who;
        double day = 0;
    };
    void noteTrade(const std::string& town, const std::string& item, const std::string& who, int quantity);
    bool townShortOf(const std::string& town, const std::string& item) const;
    double goingPrice(const std::string& town, const std::string& item) const;
    const std::map<std::string, std::vector<PriceCause>>& priceCauses() const { return priceCauses_; }
    // How a knower names a wolf (doc 57: "as the town knows them"), and whether by a name: the game's.
    struct Known
    {
        std::string words;
        bool byName = false;
    };
    void setKnower(std::function<Known(const std::string& knower, const std::string& subject)> knower) { knower_ = std::move(knower); }
    // Protected residents (doc 57, 6; the game decides who): players can't attack them, and a theft from them takes a meal
    // or herbs but never coin.
    void setProtected(std::function<bool(const std::string& id)> isProtected) { protected_ = std::move(isProtected); }
    bool isProtected(const std::string& id) const { return protected_ && protected_(id); }
    bool campMayGather(const std::string& cellId) const { return watchedNear(cellId, 2) < 1; }
    std::map<std::string, int> stormWaits(const std::string& community) const;
    Result apprenticeResident(const std::string& positionId, const std::string& youth);
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
    Result takeOfferedContract(const std::string& player, const std::string& contractId);
    Result completeContract(const std::string& contractId, const std::string& by);
    // A contract for goods (doc 35, Part 7; RatwProcure.cpp): the town's buyers' requests posted as contracts, and a
    // taker delivering what they carry of it to a merchant of that town, paid by the piece.
    Result deliverContract(const std::string& player, const std::string& contractId);
    std::vector<const Contract*> deliverable(const std::string& player) const;   // Taken by them, for here, and carried.
    // Posts work, the reward set aside from the poster's purse at once (no reward if they can't pay).
    // A poster's contract still open or taken, withdrawn (doc 57: a cancelled project's): the reward left in escrow goes
    // back to the poster. False if there is none.
    bool withdrawContract(const std::string& id);
    Contract& postContract(const std::string& kind, const std::string& poster, const std::string& town,
                           const std::string& target, std::int64_t reward, double days, const std::string& detail);
    // What someone has heard: a claim about a subject, from a source, this sure (0..1). Rumours spread along bonds
    // each day, losing confidence, and between towns with the caravans.
    void believe(const std::string& holder, const std::string& subject, const std::string& claim,
                 const std::string& source, double confidence, const std::string& incident = {}, const std::string& as = {});
    // Every belief of this claim dropped (a deed revoked: doc 56).
    void forgetClaim(const std::string& claim);
    const std::vector<Belief>* beliefsOf(const std::string& holder) const;
    // What an NPC has heard about someone, in words for a conversation (empty if nothing).
    std::string rumoursAbout(const std::string& npc, const std::string& subject, const std::string& subjectName) const;
    // Fights (Docs/Design/33-combat.md; RatwBattle.cpp). An attack starts a turn-based fight in an arena cut from the
    // cell, or joins the fight the target is already in. A player attacking a player challenges them instead: the
    // fight starts when they accept.
    Result attack(const std::string& attacker, const std::string& target, const std::string& terms = "");
    // The Dev Console (a player marked Dungeon Master): a fight to try things out where they stand, against one weak
    // bandit set on the far side of the arena with a clear way to them; and their fight ended now, as a draw. A test
    // bandit's camp ("camp_dmtest_") goes when the fight does, with no robbery, bounty or camp cleared.
    Result testFight(const std::string& player);
    // The Dev Console's /fight-test-team-1: the player and two allies (passers-by, made up for it) against three weak
    // bandits. Allies and bandits alike go with the fight when it ends.
    Result testFightTeam(const std::string& player);
    Result endFightInDraw(const std::string& player);
    static bool testCamp(const std::string& camp) { return camp.rfind("camp_dmtest_", 0) == 0; }
    // A challenge between players, on its terms ("blood", "yield" or "death"; "" for yield).
    Result challenge(const std::string& from, const std::string& to, const std::string& terms = "");
    // Training grounds (doc 53, 5; Data/Together/training.json): the Warden Training Grounds and the barracks yards.
    // There a resident trainer (a guard on duty, or one whose post trains) spars with a player who asks (no assault),
    // and a wolf with no partner practises at the post.
    bool trainingGround(const std::string& cellId) const;
    bool trainer(const std::string& id) const;
    Result sparWithTrainer(const std::string& player, const std::string& trainerId);
    // A festival tourney's bout (doc 54, 6): the two step into the ring (`at` and the tile beside it) and spar to a
    // yield. Entering the tourney agreed to it, so a wolf who declines challenges still fights its bouts.
    Result tourneyBout(const std::string& a, const std::string& b, const Spot& at);
    Result practiseAtPost(const std::string& player);
    Result answerChallenge(const std::string& player, bool accept);
    const Challenge* challengeTo(const std::string& player) const;
    const std::vector<Battle>& battles() const { return battles_; }
    const Battle* battle(const std::string& battleId) const;
    // The fight a character is in as a fighter (fighting or Downed in it; not fled), or watching; null if none.
    const Battle* battleOf(const std::string& id) const;
    const Battle* watching(const std::string& id) const;
    bool inBattle(const std::string& id) const { return battleOf(id) != nullptr; }
    // A Chapter's structures (Docs/Design/32, 5.7), set by the game: the tiles they block, and the tiles they shelter.
    std::map<std::string, std::set<std::pair<int, int>>> obstacles, shelters;
    // Who may go through a door into a place (Docs/Design/32, 5.2: a Chapter's rented rooms); unset, anyone may.
    std::function<bool(const std::string& who, const std::string& cell)> mayEnter;
    // A fighter's turn: move to a tile (crawl one, when Downed), and act: "bite", "tend", "struggle", "flee", "wait".
    Result battleMove(const std::string& id, int x, int y);
    Result battleAct(const std::string& id, const std::string& action, const std::string& target = {});
    // Gifts in a fight (Docs/Design/43-gifts.md, RatwMagic.cpp). Using one of one's family's fight abilities on one's
    // turn: `target` a fighter's id, a tile "x,y", a way "x,y" (toward that tile), a fighter and a tile "id@x,y", or
    // painted tiles "x,y;x,y;..."; arming a reaction (Slip, Interpose) or not; letting a channelled one go.
    Result useGift(const std::string& id, const std::string& ability, const std::string& target);
    Result armReaction(const std::string& id, const std::string& ability, bool on);
    Result letGo(const std::string& id);
    // What a fighter's Gift offers it now, for the fight screen: each fight ability, how it is aimed, and whether it can
    // be used now (and why not).
    struct GiftOption
    {
        std::string id, name, kind, target, why;
        int range = 0, tiles = 0, cooldown = 0;
        double mana = 0, perTurn = 0, perTile = 0;
        bool ready = false, on = false;
    };
    std::vector<GiftOption> giftOptions(const std::string& id) const;
    // A Trance (doc 45): a Quickened wolf's, at any time in a fight; its level (foes standing for each of its side's, 2 to
    // 5); a Gift's mana pool (less, Trance-fatigued).
    Result enterTrance(const std::string& id);
    double tranceLevel(const Battle& b, const BattleFighter& f) const;
    double manaPool(const Entity& e) const;
    // A Gift knocking a fighter's bar back: once between its own turns (doc 45). False if it was already shaken.
    bool knockBar(BattleFighter& t, double amount, bool weight = false);
    double tranceGain(const BattleFighter& f, double perLevel) const;   // A Trance's part, by its level and the family.
    // A work ability used out of a fight (Mend, Shortcut, Lighten Load...).
    Result useWorkGift(const std::string& id, const std::string& ability, const std::string& target);
    // The effects on a fighter now, for its card: a short name, what it does, and turns left (-1: the fight).
    struct GiftEffect
    {
        std::string id, name, does;
        int turns = -1;
    };
    std::vector<GiftEffect> giftEffects(const Battle& b, const BattleFighter& f) const;
    // How fast a fighter's bar fills in this fight (DEX, armour, injuries, Gifts), with the fight's haste.
    double meterRate(const Battle& b, const BattleFighter& f, double haste) const;
    // Whether a fighter's side can't be flanked (Heat Sense, Stone Armor, Water Screen, Critical Sight: doc 43).
    bool unflankable(const BattleFighter& f) const;
    // Practice (doc 49): the one way an attribute or skill grows, from a source in Data/Progression/skills.json
    // ("nose.use", "sneak.ambush"...). `partner` is the wolf practised with or against (an ID) and `partnerKind`
    // ("player", "resident", "animal", "fierce", "post") with `partnerValue` its skill if known; `occasion` names one
    // occasion (a fight's ID; empty: what comes within a minute of the last is one); `teacher` a teacher by kind
    // ("mentor", "trainer", "master"). Players only. A growth line goes to the player as a notice.
    using PracticeContext = practice::Context;
    void practise(const std::string& who, const std::string& source, const PracticeContext& context = {});
    // A wolf's value of an attribute or skill, and its cap (nullptr/0 for one it can't have).
    static double* practiceSlot(Entity& e, const practice::Skill& s);
    static double practiceValue(const Entity& e, const practice::Skill& s);
    static double practiceCap(const Entity& e, const practice::Skill& s);
    // A new wolf's build (doc 49, Phase 4): its grades' starting attributes and its specialty's starting skill.
    static void applyBuild(Entity& e, const practice::Build& build);
    // Development and tests: set a skill's value and, if `today` >= 0, what it has gained today.
    Result setPractice(const std::string& who, const std::string& skill, double value, double today = -1);
    // Real seconds, for practice's rolling day and rested time (set by the game; the world's own time without it), and
    // the account a character belongs to (set by the game; "" for none), so one account's wolves never teach each other.
    std::function<double()> realClock;
    // Off, nothing grows: simulations that measure the rules (Tests/level_sim.cpp) keep their wolves as made.
    bool practising = true;
    std::function<std::string(const std::string& id)> accountOf;
    // Whether a player is at the keys now (set by the game): passive practice (an apprentice's hours) needs it.
    std::function<bool(const std::string& id)> playerActive;
    // Whether a fighter could stand on an arena tile now: open ground no one stands on or walks to (but `except`).
    bool arenaOpen(const Battle& b, int x, int y, const std::string& except = {}) const;
    // The tiles a fighter may move to now (none when it isn't their turn, or they have moved).
    std::vector<std::pair<int, int>> battleReach(const std::string& id) const;
    // Planning ahead while one's bar fills (doc 37): the tiles one's next turn could reach from here; a tile to go to
    // and an action to take ("bite", "sword", "flame" (target "x,y"), "tend", "roll", "rest", "hold", "stow", "pickup",
    // "flee"), each played as the turn begins; and taking back the move ("move"), the action ("act") or both ("").
    std::vector<std::pair<int, int>> planReach(const std::string& id) const;
    Result planMove(const std::string& id, int x, int y);
    Result planAct(const std::string& id, const std::string& action, const std::string& target);
    Result unplan(const std::string& id, const std::string& part);
    // Sneaking (doc 40): how much one fighter notices another now (0..1: sight in its field of view, cover, a stalker's
    // crouch, the noise of a move, scent down the wind, skill); what an observer has noticed of a target (0 unaware,
    // battle::AwareSuspicious, battle::AwareAlert); whether a blow from `f` takes `t` unawares (an ambush); stalking.
    double arenaNotice(const Battle& b, const BattleFighter& observer, const BattleFighter& target, bool moving) const;
    battle::Senses arenaSenses(const Battle& b, const BattleFighter& observer, const BattleFighter& target, bool moving) const;
    // Out of a fight (doc 40, §2): a resident's awareness of a player near them (kept for near pairs, checked a few times
    // a second); a fresh check of it now (for a theft or a crime's witnesses); a voice heard giving a sneak away.
    double residentAwareness(const std::string& resident, const std::string& player) const;
    double senseInWorld(const std::string& resident, const std::string& player);
    void heardVoice(const std::string& listener, const std::string& speaker);
    // The same by sense, from any two places: for the arena and the open world alike.
    battle::Senses noticeSenses(const Entity& observer, Vec2 at, double facing, const Entity& target, Vec2 targetAt, bool stalking, bool moving,
                                int pace, const std::string& cellId, bool smoked = false) const;
    double awareness(const Battle& b, const std::string& observer, const std::string& target) const;
    bool ambushing(const Battle& b, const BattleFighter& f, const BattleFighter& t) const;
    Result battleStalk(const std::string& id, bool on);
    // Doc 40's fight start: the positioning phase (a wolf placing itself on its side's half, and ready), whether a tile
    // may be placed on by a fighter, and the open ground's connected parts.
    Result placeFighter(const std::string& id, int x, int y);
    Result readyToFight(const std::string& id, bool ready);
    bool placeable(const Battle& b, const BattleFighter& f, int x, int y) const;
    int halfOf(const Battle& b, int x, int y) const;
    bool reachesFoe(const Battle& b, const BattleFighter& f) const;    // On the same open ground as a foe still standing.
    // Auto-decline fights with players: on, no player may challenge this one (fights with residents, bandits, game go on).
    Result setNoPvp(const std::string& id, bool on);
    // How fast the fight's bars fill now: battle::Haste with no player taking a turn and no fire gathering, else 1.
    double meterHaste(const Battle& b) const;
    // The chance a blow from `f` lands on `t` from where they stand now (hit or graze): dexterity, fighting skill and
    // the side or back it comes from (doc 33). The same number the blow is rolled against, for the page's preview.
    double strikeChance(const BattleFighter& f, const BattleFighter& t) const;
    // A fighter hurt (by a blow, fire or burning): down, yielding or bloodied as the fight's terms say (doc 37).
    void hurtFighter(Battle& b, BattleFighter& t, double damage, double downedBase, const std::string& by, bool interrupt);
    // Coming into a fight from the edge of its square: as a fighter on a side, or to watch it.
    Result joinBattle(const std::string& id, const std::string& battleId, int side);
    Result observeBattle(const std::string& id, const std::string& battleId);
    Result leaveObserving(const std::string& id);
    // Out of a fight: struggling up from Downed (the once-a-day self-recovery), and tending someone Downed.
    Result struggleUp(const std::string& id);
    Result tendWounds(const std::string& id, const std::string& target);
    bool downed(const std::string& id) const;
    bool recoveryAvailable(const Entity& e) const;
    battle::Temperament temperamentOf(const Entity& e) const;
    // Turning to face another way, on one's own turn: free (eighths of a turn from east).
    Result battleFace(const std::string& id, int dir);
    // A truce offered, and agreed to (or refused): every fighter still standing must agree.
    Result offerTruce(const std::string& id);
    // Yielding (doc 37): offered at any time; a player on the other side accepts or refuses (an NPC side accepts).
    Result offerYield(const std::string& id);
    Result answerYield(const std::string& id, bool accept);
    Result answerTruce(const std::string& id, bool agree);
    // The mouth slot: holding a sword (one has one), putting it away; taking something lying on the ground.
    Result holdItem(const std::string& id, const std::string& item);
    Result stowItem(const std::string& id);
    Result takeItem(const std::string& id, const std::string& groundId);
    // Wearing (doc 35, Phase 4): putting on a wearable one owns, at a wear slot or (jewellery) a fur spot (`where`
    // empty: the first free place it fits), and taking it off again (`item` picks a piece at a fur spot). Not in a
    // fight. How many of an item one wears (or holds), and letting go of what the purse no longer has.
    Result wear(const std::string& id, const std::string& item, const std::string& where);
    Result takeOff(const std::string& id, const std::string& where, const std::string& item);
    static int wornCount(const Entity& e, const std::string& item);
    // Carrying (doc 35, 1.2): everything in the purse weighs, worn or not. Comfortable up to 12 + STR/4 lb; up to twice
    // that is heavy (the top pace falls a notch a quarter over, and running tires more); beyond, overloaded: a walk,
    // and no starting or joining a fight.
    struct Load
    {
        double carried = 0, comfortable = 0;
        std::string state = "comfortable";            // "comfortable", "heavy" or "overloaded".
        int pace = 10;                                // The fastest pace it allows.
        double drain = 1.0;                           // Running's stamina cost, times this.
    };
    Load loadOf(const Entity& e) const;
    void refreshLoad(Entity& e) const;
    // Why a player may not start or join a fight for what they carry ("" if they may).
    std::string tooLoadedToFight(const std::string& id) const;
    // Injuries that outlast a fight (doc 38, phases 3 and 4; RatwInjury.cpp). From a blow that lands, from going down, and
    // at a fight's end; fighting on hurt sets healing back; rest heals. Players only.
    void injureOnBlow(Battle& b, BattleFighter& t, double damage, double downedBase, const std::string& by);
    void injureOnDown(Battle& b, BattleFighter& f, double overkill, double downedBase, const std::string& by);
    void injureAtEnd(Battle& b);
    void strainOnEntering(const std::string& id);
    void healInjuries(Entity& e, double restHours);
    // A Dungeon Master's correction or storyline (doc 38, phase 5): add an injury of a kind (acute at a severity), or
    // take one away by its id.
    Result addInjury(const std::string& id, const std::string& type, int severity, const std::string& side, const std::string& from);
    Result removeInjury(const std::string& id, const std::string& injuryId);
    // What others see one wearing, in a sentence or two ("" for nothing).
    static std::string wornWords(const Entity& e);
    void fitWorn(const std::string& id);
    const std::vector<GroundItem>& groundItems() const { return ground_; }
    // A Gift, given (the Dungeon Master or a developer: who has one is the setting's to decide).
    Result giveGift(const std::string& id, const std::string& gift, bool quickened);
    // A player gone from the world while fighting: the body stays, away, until the fight ends or a minute passes.
    void linger(const std::string& id);
    // ...and back again in time: their turns are theirs once more, not skipped as away.
    void stopLingering(const std::string& id);
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
    // Bandits called up near someone (the Dungeon Master: doc 33): a small camp of `count` (1..6) five to ten tiles away,
    // on open ground away from doors; they come out at once, as any camp does when someone is near.
    Result callBandits(const std::string& near, int count);
    std::int64_t banditDemand(const std::string& player) const; // What bandits are asking of them now (0: nothing).
    // Things that happened to a player that no action of theirs answered (a bandit's blow, say), since the last
    // call: (player, words), oldest first. The host shows them to the player.
    std::vector<std::pair<std::string, std::string>> takeNotices();
    std::vector<WorldEvent> takeEvents();
    std::size_t droppedEvents() const { return droppedEvents_; }
    // Each home's stores (doc 36) placed so far: home cell -> kind -> where it stands.
    const std::map<std::string, std::map<std::string, Spot>>& homeStoreSpots() const { return homeStoreSpots_; }
    const std::map<std::string, Spot>& beds() const { return beds_; }
    // Route searches answered from the path cache, and searched (see findPath).
    std::pair<std::size_t, std::size_t> pathCacheStats() const { return {pathHits_, pathMisses_}; }
    // Residents' route searches on a thread of their own (doc 31, Phase 5): a resident whose route isn't in the path
    // cache waits a tick or two while it is searched, rather than the whole tick waiting on a long search. Off, the
    // search runs at once on the caller (tests, tools). A copy of the world starts with it off.
    void setRoutesOffThread(bool on);
    // Work done by counts alone, never by the clock (for long headless runs): each half-hour's schedules finish in the
    // step that begins them, and route planning stops at its count of searches and nodes, not at RouteBudgetMs. The same
    // world then runs the same way on any machine, however loaded. Off (the server), slow work spreads over steps.
    void setDeterministic(bool on) { deterministic_ = on; }
    // The cells a traveller passes through from one to another (the first and the last among them), by the way routes
    // go (firstSteps: along the roads where there are any); empty with no way.
    std::vector<std::string> routeBetween(const std::string& from, const std::string& to) const;
    bool deterministic() const { return deterministic_; }
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
        double last[5] = {};                            // The last tick's: streaming, schedules, movement, separation, views.
        // The schedules by stage (ms in all): society, bonds, roads, crime, errands, streaming's wants, route planning.
        double stages[7] = {};
        std::size_t ticks = 0, routeSearches = 0;
        std::size_t routeNodes = 0, largestRoute = 0;   // Nodes the route searches expanded, and the most in one.
        double slowestRoute = 0;                        // The longest one route search took (ms), and where.
        std::string slowestRouteCell;
        std::size_t slowestRouteNodes = 0, slowestRouteWaypoints = 0;
    };
    const TickProfile& tickProfile() const { return profile_; }
    void resetTickProfile() { profile_ = {}; }
    Result move(const std::string& id, double dx, double dy);
    // Free movement (doc 31, Phase 3). Whether a client walks this wolf itself; and a pose it sends, checked from where
    // the wolf stands: no farther than its speed allows (with a margin, from a budget refilled at that speed, so a
    // burst of poses gains nothing), only where it may stand, and never through a wall or a closed door. `ix, iy` is
    // the way it is heading, so pushing into a door or a cell's edge still takes it through (only the server crosses).
    void setClientWalks(const std::string& id, bool on);
    struct PoseCheck
    {
        bool accepted = false, crossed = false;
        std::string reason;                       // Why it was refused.
    };
    PoseCheck placeByClient(const std::string& id, std::uint32_t seq, double x, double y, double facing, double ix, double iy);
    static constexpr double PoseMargin = 1.15, PoseSlack = .1;
    // The moments around a fight, when the server walks a player itself (held movement): pursued by a guard, in a
    // scuffle lately, an offence lately.
    bool pursued(const std::string& id) const;
    bool foughtWithin(const std::string& id, double seconds) const;
    bool offendedWithin(const std::string& id, double seconds) const;
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
    // `withTiles` false: the cell's header only, with `source` and `memory` to read its tiles from (copying a city
    // cell's 65,000 tiles for every view was a large part of a snapshot's cost).
    // `observeFirst` false: the observer's memory was brought up to date already (observe), as views built in parallel
    // need (doc 31, Phase 4: a snapshot then only reads the world).
    Snapshot snapshot(const std::string& observerId, bool withTiles = true, bool observeFirst = true);
    SensoryResult perceive(const std::string& observerId, const std::string& sourceId,
                           Voice voice = Voice::Speak) const;
    bool lineOfSight(const std::string& cellId, Vec2 from, Vec2 to) const;
    double hearingClarity(const std::string& observerId, const std::string& sourceId, Voice voice = Voice::Speak) const;
    // How keen a wolf's hearing is (1 a healthy adult's): its hearing, age, ears, skill and injuries (the factor
    // hearingClarity scales its range by; doc 51's howl uses it too).
    double hearingSensitivity(const Entity& e) const;
    double visionClarity(const std::string& observerId, const std::string& sourceId) const;
    // The same with the observer's sight range already known (sightRange): for a view or a motion frame, which ask it
    // of every wolf in the cell (doc 31, Phase 4: the range was worked out three times a wolf).
    double visionClarity(const Entity& observer, const Entity& source, double range) const;
    double sightRange(const Entity& observer) const;
    // Movement sounds only: this never reduces deliberate spoken voice volume.
    double movementAudibility(const std::string& observerId, const std::string& sourceId) const;
    double scentClarity(const std::string& observerId, const std::string& sourceId) const;
    std::vector<ScentCue> scentCues(const std::string& observerId) const;
    Wind windAt(const std::string& cellId) const;
    Environment environmentAt(const std::string& cellId) const;
    // At a point in the cell: the weather there, as strong as it is there (doc 29, phase 7).
    Environment environmentAt(const std::string& cellId, Vec2 at) const;
    WeatherSample weatherAt(const std::string& cellId, Vec2 at) const;
    // A sample of the field over a cell for the client: one each `step` tiles, row by row.
    std::vector<WeatherSample> weatherGrid(const std::string& cellId, int step, int& cols, int& rows) const;
    const std::vector<WeatherSystem>& weatherSystems() const { return systems_; }
    // Calls up a weather system: its kind, its middle (world tiles), reach, heading (radians, east 0) and hours.
    // `grown`: already at full strength (a developer's front, to see at once), rather than gathering over its first hours.
    Result spawnFront(Weather kind, double x, double y, double radius, double heading, double hours, bool grown = false);
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
    StandingWorks works_;                           // Town projects standing (doc 57).
    std::map<std::string, std::vector<TradeNote>> tradeNotes_;    // Town -> its latest notes (doc 57, 5).
    std::map<std::string, std::vector<PriceCause>> priceCauses_;  // Town -> prices lately lowered, and by whom.
    std::map<std::string, double> fedToday_;        // "town|wolf|day" -> nourishment brought in that day.
    std::map<std::string, std::map<std::string, double>> storeFactor_;   // Town -> meal and herbs -> its price factor now.
    std::function<Known(const std::string&, const std::string&)> knower_;
    std::function<bool(const std::string&)> protected_;
    void tendTradeNotes();
    std::map<std::string, std::set<std::int64_t>> foulMarkets_;   // Community -> market days kept in by the weather.
    std::map<std::string, std::map<std::string, std::set<std::int64_t>>> stormWaits_;   // Town -> cell -> days.
    double watchedNear(const std::string& cellId, int within) const;   // The strongest watch post this many cells off.
    // Roads between places (the user, 2026-10-05: travel between towns keeps to the roads, unless they are blocked).
    // The seams that cross on a road (a dirt road, a street, flagstones) on both sides, and each cell's neighbours a
    // road leads to. Worked out once at load from every cell's tiles (indexRoads), so a route never depends on which
    // cells happen to be in memory. Routes count a step along a road as 1 and one overland as OverlandCost.
    std::set<std::string> roadSeams_;
    std::map<std::string, std::set<std::string>> roadExits_;
    static constexpr int OverlandCost = 3;
    void indexRoads();
    bool roadLink(const std::string& from, const std::string& to) const
    {
        const auto found = roadExits_.find(from);
        return found != roadExits_.end() && found->second.count(to) > 0;
    }
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
    std::map<std::string, Letting> lettings_;
    EntityMap entities_;
    TickProfile profile_;
    std::vector<WorldEvent> events_;
    Bonds bonds_;
    std::vector<Promise> promises_;
    RoadsState roads_;
    std::vector<Town> towns_;
    std::map<std::string, std::string> townOfCell_;
    std::unordered_map<std::string, std::size_t> townIndex_;   // Cell -> its town in towns_ (townOf, asked very often).
    std::unordered_set<std::string> standing_;     // Cells anyone on the stage stands in, for the errands (continueSchedules).
    bool unseenErrands_ = true;                     // Whether this pass's errands include the unseen's (Society::UnseenStep).
    bool errandsOnstage_ = true;                    // Whether any resident is on the stage, this pass.
    // Those the passes over everyone each tick may have something to do for (movement, bodies apart, the Downed, rest):
    // anyone dead, onstage, a player, with a Gift, tending someone or Downed. Not a resident going about its day
    // offstage. Gathered after the schedules (the last to change any of it), in ID order (gatherAwake).
    std::vector<EntityMap::value_type*> awake_;
    void gatherAwake();
    EntityMap::InOrder awake() const { return {awake_.data(), awake_.data() + awake_.size()}; }
    std::unordered_set<std::string> following_;     // Residents keeping up with something that moves, this pass.
    bool townsReady_ = false;
    // Home storage (Docs/Design/36-home-storage.md): every household's stores opened and stocked once (homesReady_),
    // and where each home's stand, placed when its interior is first in memory.
    bool homesReady_ = false;
    std::map<std::string, std::map<std::string, Spot>> homeStoreSpots_;
    std::map<std::string, Spot> beds_;              // Each resident's place on a bed at home: up to four to a bed.
    void placeBeds(const std::string& cellId);
    void furnishHomes();
    void placeHomeStores(const std::string& cellId);
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
    std::map<std::string, Spot> boards_;            // By community: its notice board (doc 54, 2), placed once.

  public:
    // A town's notice board (doc 54, 2): on open ground 2 to 4 tiles from its square's market point, off its stall spots,
    // the same place every time; null for a community with no square.
    const Spot* boardSpot(const std::string& community);
    // Market stalls (doc 54, 3): a square's built stall spots, and those let to players today ("cell|x|y", by whole
    // tiles), which merchants leave to them: Marketday's plan sends merchants only to the others.
    const std::vector<Spot>& stallSpots(const std::string& community) { return square(community).stalls; }
    // A square's middle (null for a community with none) and where a crowd stands around it (doc 54, 6's festivals).
    const Spot* marketSpot(const std::string& community)
    {
        const auto& sq = square(community);
        return sq.found ? &sq.at : nullptr;
    }
    const std::vector<Spot>& crowdSpots(const std::string& community) { return square(community).crowd; }
    void setLetStalls(std::set<std::string> keys);
    static std::string stallKey(const Spot& s);
    // Tavern games (doc 54, 5): a resident seated at a table stays at its seat until it is let go (an errand before its
    // day's plan; a hungry resident still goes to eat).
    struct Seat
    {
        std::string cell;
        double x = 0, y = 0;
        std::string reason;
    };
    void seatResident(const std::string& id, Seat seat) { seated_[id] = std::move(seat); }
    void unseatResident(const std::string& id) { seated_.erase(id); }
    bool seatedResident(const std::string& id) const { return seated_.count(id) > 0; }
    // Kills on hunts since last asked (doc 54, 6: a festival's hunting contest reads them): who, what, how cleanly.
    struct HuntedNote
    {
        std::string killer, species, grade;
        double day = 0;
    };
    void noteFound(const std::string& player, const std::string& item, const std::string& ground);   // (The herbarium.)
    // The events recorded since they were last taken for saving (the chronicle's in-memory part: doc 56).
    const std::vector<WorldEvent>& recentEvents() const { return events_; }
    std::vector<HuntedNote> takeHunted()
    {
        auto out = std::move(hunted_);
        hunted_.clear();
        return out;
    }

  private:
    std::set<std::string> letStalls_;
    std::map<std::string, Seat> seated_;
    std::vector<HuntedNote> hunted_;
    // A community's church (doc 42, Phase 6): its clergy's place of work, its seats and its pulpit; worked out each day.
    struct Chapel
    {
        bool found = false;
        Spot pulpit;
        std::vector<Spot> pews;
    };
    std::map<std::string, Chapel> chapels_;
    std::int64_t chapelsDay_ = -1;
    const Chapel& chapel(const std::string& community);
    int skyOf(const std::string& cellId) const;            // -1 indoors, 0 fair, 1 wet, 2 harsh.
    void planDays();
    std::int64_t crimeHour_ = -1;
    std::map<std::string, double> stealReady_;              // When each may try again.
    std::map<std::string, std::pair<std::string, double>> fights_;   // "attacker|target": the incident, the last blow.
    std::map<std::string, std::string> pursuits_;           // Guard: whom they're going to stop.
    // Sneaking (doc 40, §2): residents' awareness of players near them; when it was last checked; guards gone to look
    // into something they half noticed (where, and until when).
    std::map<std::pair<std::string, std::string>, double> worldAware_;
    double awarenessAt_ = -1;
    struct Looking
    {
        std::string cell;
        Vec2 at;
        double until = 0;
        std::string task = "looking into a noise", reason = "something stirred";   // (Or backing away from a prowler.)
    };
    std::map<std::string, Looking> lookings_;
    void tendAwareness();
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
    void rumoursFromEvent(const WorldEvent& event);
    void contractsFromEvent(const WorldEvent& event);
    void settleContract(Contract& c, const std::string& status, const std::string& paidTo);
    // Road folk: each one's part, keyed by entity ID ("road:...").
    struct RoadFolk
    {
        std::string kind, of;               // "caravan" or "bandit"; the caravan's or camp's ID.
        double skill = -1;                  // A fighting skill of their own (a test bandit's), else their kind's.
        double hp = 16, share = 1, nextSwing = 0, nextPath = 0;
        std::string lastCell;
        double enteredAt = 0;               // When it came into its cell (a wagon stuck that long is moved on).
    };
    std::map<std::string, RoadFolk> folk_;
    std::map<std::string, double> visitors_;   // Temporary folk (addVisitor): when each leaves, in world seconds.
    std::set<std::string> storyVisitors_;      // Those a storyteller brought (doc 58).
    // Bandits who have stopped a player: asking (a demand) until they pay, get clear, or it comes to blows.
    struct Encounter
    {
        std::string camp, player;
        std::int64_t demand = 0;
        double since = 0;
        bool fighting = false;
        // Creeping up instead of stepping out (doc 40): unseen yet; how much the traveller has noticed of them; whether
        // they have heard something.
        bool creeping = false;
        double spotted = 0;
        bool rustled = false;
    };
    std::vector<Encounter> encounters_;
    std::map<std::string, double> spared_;          // Players bandits leave alone until then (world seconds).
    // Fights (RatwBattle.cpp).
    std::vector<Battle> battles_;
    std::vector<Challenge> challenges_;
    std::map<std::string, double> settleUntil_;     // No new fight for these until then (just back from one).
    std::uint64_t nextBattle_ = 0;
    std::vector<GroundItem> ground_;
    std::uint64_t nextGround_ = 0;
    std::map<std::string, std::set<std::string>> heardFights_;   // Who has been told of a fight they could only hear.
    // Hunting and foraging (RatwHunt.cpp). Animals live only in their hunt: nothing of them is saved.
    struct HuntAnimal
    {
        std::string species, battle;
        bool alert = false;
        double best = 0, fire = 0, total = 0;       // The hardest blow, and the damage done in all and by fire.
        std::string lastBy;                         // Who last struck it (for a kill by its burns).
        bool struckUnaware = false;                 // Its hardest blow fell before it knew (doc 40's ambush): masterwork.
        // Doc 53, Phase 1: "grazing", "watching" (a wolf it noticed, further than its flight distance), "fleeing" (from
        // one that came too close, or bit at it), "calming"; whom it watches and flees; its turns calming; every wolf it
        // has been alert to (masterwork only from one it never saw).
        std::string state = "grazing", watching, from;
        int calm = 0;
        std::set<std::string> saw;
        int noiseX = -1, noiseY = -1;               // Fleeing a noise (Throw Voice, doc 53, 4), not a wolf: from this tile.
    };
    // A hunter in a hunt (doc 53): its last turn that did something (moved, stalked, bit), and whether it lies in wait
    // (it ended its turn crouched without biting: it springs on an animal stepping beside it).
    struct HuntHunter
    {
        int lastActive = -1;
        bool waiting = false;
    };
    std::map<std::string, HuntHunter> huntHunters_;
    // Working together (doc 53, 2): the joints at work and each member's, who each worker has asked to lend a paw, and
    // when each wolf last worked (foraged); never saved.
    std::map<std::string, together::JointWork> joints_;
    std::map<std::string, std::string> jointOf_;
    std::map<std::string, std::set<std::string>> lendAsked_;
    std::map<std::string, double> lastWorked_;
    double jointsAccumulator_ = 0;
    std::uint64_t jointNext_ = 1;
    void tendJoints(double dt);
    std::string joinJoint(const std::string& jointId, const std::string& helper, const std::string& role);
    void workBeat(together::JointWork& joint);
    void endJoint(const std::string& id);
    void dropFromJoint(const std::string& id);
    // Hunting together (doc 53, Phase 2): each hunt's starter, who was let in or invited, asks waiting, each player's
    // share of its last hunt, the parts the hunters played, the pairs who shared a kill.
    std::map<std::string, std::string> huntStarter_;
    std::map<std::string, std::set<std::string>> huntInvited_;
    std::vector<HuntAsk> huntAsks_;
    std::map<std::string, HuntShare> huntShares_;
    std::map<std::string, std::map<std::string, std::set<std::string>>> huntRoles_;
    std::map<std::string, std::set<std::pair<std::string, std::string>>> huntPairs_;
    std::function<bool(const std::string& a, const std::string& b)> blocked_, partnered_;
    std::function<void(const WorldEvent&)> eventWatcher_;
    std::vector<WorldEvent> pendingEvents_;         // Events an event gave rise to, recorded after it (a promise kept).
    std::function<std::string(const std::string&, const std::string&)> namer_;
    std::function<DeedWords(const std::string&, const std::string&, const std::string&)> deedWords_;
    std::function<bool(const std::string& id, const std::string& cell)> bedRight_;
    std::map<std::string, std::pair<std::string, double>> talkFacing_;   // Resident -> the wolf it faces, until when.
    std::map<std::string, double> groomBondDay_;    // "a|b" -> the game day their grooming last warmed them (doc 55).
    std::map<std::string, double> mealBondDay_;     // "a|b" -> the game day a shared meal last warmed them (doc 55).
    std::vector<Occasion> occasions_;
    std::uint64_t nextOccasion_ = 1;
    double occasionsAccumulator_ = 0;
    bool huntHelperTurn(Battle& b, BattleFighter& f);
    void huntJoined(Battle& b, const std::string& id);
    std::vector<std::string> huntSharers(const Battle& b, const std::string& killer) const;
    std::string pickHuntSpecies(const Battle& b, const std::string& salt) const;
    std::map<std::string, HuntAnimal> animals_;
    std::map<std::string, std::vector<double>> huntKills_;      // Cell -> the game days of its kills.
    std::map<std::string, std::pair<double, double>> huntArrivals_;   // Hunt -> when another may wander in, and how many it expects.
    mutable std::map<std::string, std::map<std::string, double>> groundCache_;   // Cell and block -> its ground (it doesn't change).
    std::map<std::string, std::pair<int, double>> forage_;      // Patch -> pickings taken, the day they were counted.
    // Working out of town (doc 42, Phase 3b; RatwOutwork.cpp): the ground near each town, found once; each hunting
    // ground's mix of country; a spell of work's yield; one picking from a forage patch (shared with players).
    std::shared_ptr<const std::map<std::string, std::vector<WorkGround>>> workGrounds_;
    std::map<std::string, std::map<std::string, double>> huntGround_;
    void findWorkGrounds();
    std::vector<std::pair<std::string, int>> harvestAt(const std::string& who, const WorkGround& at, int season);
    bool takeFromPatch(const std::string& cellId, int x, int y, int extra = 0);   // extra: pickings more (doc 53).
    std::map<std::string, double> forageNext_;                  // Wolf -> when it may forage again (world seconds).
    std::function<bool(const std::string&, const std::string&)> friends_;
    std::uint64_t nextAnimal_ = 0;
    std::map<std::string, std::vector<Track>> tracks_;            // Wolf -> the trails it has found (not saved).
    double lastWearDay_ = -1;                                     // When clothes last wore with the days.
    double nextMarkCheck_ = 0;                                    // When noses near thieves are next tried.
    void postProcurements();
    std::vector<std::string> noteReckonings(); // The month's reckoning in the event log (doc 42): each town's line.
    // Residents filling contracts for goods (doc 42, Phase 4; RatwProcure.cpp): the shop with goods to spare for one
    // (and how many), taking them on, and the carriers' fetching and delivering.
    std::pair<std::string, int> sourceFor(const Contract& k) const;
    // Trade between towns (doc 42, Phase 7; RatwTrade.cpp): a town's market (what its shops and producers have to
    // spare and who holds it; what its makers and suppliers are short of and who), its trading house, the day's trade
    // caravans, and a trade caravan's arrival and homecoming.
    // Food between towns (tradeCaravans): a town whose shops hold under FoodDaysKept days' food for its people sends for
    // the foods other towns have most to spare (FoodsSent of them), FoodKept of each for each of its food shops.
    static constexpr int FoodDaysKept = 2, FoodKept = 12, FoodsSent = 3;
    struct Market
    {
        std::map<std::string, int> spare, want;
        std::map<std::string, std::vector<std::pair<std::string, int>>> holders, wanters;   // Account, how many.
        int people = 0, food = 0;                   // Who lives there; the food on its shops' shelves (meals' worth).
        std::vector<std::string> foodShops;         // Its food shops' tills.
    };
    std::string residentTown(const std::string& id) const;
    Market marketOf(const std::string& town) const;
    std::string traderOf(const Town& t) const;
    void tradeCaravans();
    // Standing orders (RatwTrade.cpp): the price a piece agreed with a town now, those of a road due a delivery, and the
    // porters sent to renegotiate them (chosen once a day among those free to go; done on arriving at the market).
    std::int64_t orderPrice(const Town& from, const std::string& item) const;
    void residentsRenegotiate(std::set<std::string>& busy);
    void tendNegotiators();
    // Food for the road for one going from `town` to `toCell` and back, paid by `payer` up to `budget`: a meal for every
    // ten places of the way, two at least, bought at its town's shops; what can't be bought, the money for it. Returns
    // what it cost. Never made.
    std::int64_t provisionTraveller(const std::string& traveller, const std::string& payer, const std::string& town,
                                    const std::string& toCell, std::int64_t budget);
    void recoverHoard(const BanditCamp& camp, const std::string& to, const std::string& kind);
    std::map<std::string, std::int64_t> churchCared_;   // Player -> the day the church last tended them.
    void tradeCaravanArrived(Caravan& c);
    void tradeCaravanHome(Caravan& c);
    void residentsFillContracts(std::set<std::string>& busy);
    // Food for the road for a carrier going to another town for a contract's goods (RatwProcure.cpp).
    void provisionCarrier(Contract& k, int quantity);
    void tendContractCarriers();
  public:
    // The month's reckoning at once (the Dev Console's /reckon): what each town took in, as a line each.
    std::string reckonNow();
    // The church's care (doc 42, Phase 5): a priest or chapel keeper tends a poor wolf's healing wounds with the
    // church's bandages, free, once a day: a quarter of each wound's rest. Poor: under PoorPurse.
    static constexpr std::int64_t PoorPurse = 20;
    bool churchCares(const std::string& player, const std::string& clergy) const;
    Result churchCare(const std::string& player, const std::string& clergy);
    // The storehouse of a cleric's town's church (Society::churchStore): its bandages, bread and candles.
    std::string churchStoreOf(const std::string& clergy) const;
  private:
    void tendMarks();
    void wearGear(Entity& e, const std::string& item, double amount);
    void wearArmourAt(Entity& e, const std::string& zone, double taken);
    void tendWear();
    bool animalNotices(const Battle& b, const BattleFighter& animal) const;
    bool animalTurn(Battle& b, BattleFighter& f);
    // Doc 53, Phase 1: what an animal that runs makes of the wolves it notices (watching, bolting, calming); a dodge
    // that sends it off; the long run of one bolting; one lying in wait springing on it as it steps past.
    void huntReact(Battle& b, BattleFighter& animal, const BattleFighter* after);
    bool fleeingTurn(Battle& b, BattleFighter& f, HuntAnimal& a, const wild::Species& s, const std::vector<const BattleFighter*>& hunters);
    void huntBolt(Battle& b, BattleFighter& animal, HuntAnimal& a, const std::string& from);
    // A noise at a tile in a hunt (Throw Voice drives, doc 53, 4): game within its flight distance of it flees from the
    // tile; further off within `reach`, it watches that way. How many it moved.
    int huntNoise(Battle& b, int x, int y, int reach);
    void huntAfterTurn(Battle& b, BattleFighter& f);
    void huntMissed(Battle& b, const BattleFighter& biter, BattleFighter& animal);
    void huntStep(Battle& b, BattleFighter& animal);
    int huntReach(const Battle& b, const BattleFighter& f) const;
    double huntBlow(Battle& b, BattleFighter& t, double damage, double downedBase, const std::string& by);
    // Spars (doc 53, 5; RatwTraining.cpp): a blade's blow blunted; what a blow teaches, by partner and ground.
    double sparBlow(const Battle& b, double damage, const std::string& by) const;
    double sparPractice(const Battle& b, const BattleFighter* foe) const;
    std::map<std::string, double> postNext_;       // When each wolf may next practise at the post.
    bool huntKill(Battle& b, BattleFighter& f, const std::string& by);
    void huntBanner(Battle& b);
    void endHunt(Battle& b);
    std::string huntJoinRefusal(const Battle& b, const std::string& id) const;
    void tendHunts();
    std::map<std::string, double> groundIn(const std::string& cellId, int x0, int y0, int w, int h) const;
    double huntExpected(const std::string& cellId, const std::map<std::string, double>& ground) const;
    bool addAnimal(Battle& b, const std::string& species, bool arriving);
    int playersOnline() const;
    Result swordStrike(Battle& b, BattleFighter& f, const std::string& target);
    Result shove(Battle& b, BattleFighter& f, const std::string& target);
    Result castFlame(Battle& b, BattleFighter& f, int x, int y);
    void resolveCast(Battle& b, const BattleCast& cast);
    // Gifts (RatwMagic.cpp): what happens at the start of a fighter's turn (false if the turn is lost); as it steps onto
    // a tile; a gathered Gift going off; a blow coming at a wolf (Slip, Interpose, Riposte: true if it is dealt with);
    // what gets through to it; a hit breaking a held Gift; what blocks a tile; the move's reach and breath; the senses.
    bool magicTurnStart(Battle& b, BattleFighter& f);
    void magicStep(Battle& b, BattleFighter& f);
    void magicResolve(Battle& b, const BattleCast& cast);
    bool magicBlow(Battle& b, BattleFighter& f, BattleFighter*& t, const std::string& weapon, Result& out);
    void magicAfterBlow(Battle& b, BattleFighter& f, BattleFighter& t);
    double magicDamage(const Battle& b, const BattleFighter& t, double damage, bool fire) const;
    void magicHurt(Battle& b, BattleFighter& t);
    bool magicBlocks(const Battle& b, int x, int y) const;
    int magicRange(const Battle& b, const BattleFighter& f, int range) const;
    double magicTileStamina(const Battle& b, const BattleFighter& f, double perTile) const;
    void magicSenses(const Battle& b, const BattleFighter& o, const BattleFighter& t, battle::Senses& s) const;
    double magicStrikeChance(const BattleFighter& f, const BattleFighter& t, double chance) const;
    void magicFightStart(Battle& b);
    void magicFightEnd(Battle& b);                  // (After a Trance: Trance fatigue, doc 45.)
    bool npcGift(Battle& b, BattleFighter& f, const BattleFighter& mark);
    void tendGiftSenses();                          // Danger Sense (Gifted Seers): hidden bandits nearby, told.
    std::map<std::string, double> workGiftAt_;      // Player|ability -> when a work Gift was last used (world seconds).
    std::map<std::string, double> mendedDay_;       // Injury id -> the day it was last Mended.
    std::set<std::pair<std::string, std::string>> dangerTold_;   // Seer, bandit: already warned.
    double giftSensesAt_ = 0;                       // When Danger Sense last looked.
    double progressAt_ = 0;                         // When apprentices' practice was last looked at (doc 49).
    std::map<std::string, std::pair<double, double>> teachers_;   // Who|skill -> (until, factor): a better player near (doc 49).
    double teacherNear(const Entity& e, const practice::Skill& s, double now);
    Result useGiftNow(const std::string& id, const std::string& ability, const std::string& target);   // (useGift's body.)
    void practiseMoving(Entity& a, double movedTime);
    void tendProgress();
    std::string giftWhyNot(const Battle& b, const BattleFighter& f, const std::string& ability) const;
    void overreach(Battle& b, BattleFighter& f, const std::string& family);
    void wardensSee(Battle& b, const BattleFighter& caster);
    bool throwFighter(Battle& b, BattleFighter& t, int dx, int dy, int tiles, const std::string& by, double crash);
    void dropItem(Battle& b, BattleFighter& f);
    void tendFightSurroundings(Battle& b);
    // Fighting skill grows by fighting (doc 49): a blow landed on a foe still able to fight, or standing to a fight's
    // end, is practice of the `source` ("fight.blow", "fight.end"), the foe the partner and the fight the occasion.
    void growSkill(Battle& b, const BattleFighter& learner, const BattleFighter* foe, const char* source);
    Battle* battleFor(const std::string& id);
    Battle* battleById(const std::string& battleId);
    // `terms`: a spar with a resident trainer (doc 53, 5) is no assault.
    Result startBattle(const std::string& attacker, const std::string& target, bool pvp, const std::string& terms = {});
    void enterBattle(Battle& b, const std::string& id, int side, bool full);
    void fitArena(Battle& b);
    void lineUp(Battle& b);
    void tendBattles(double dt);
    void beginTurn(Battle& b, BattleFighter& f);
    void endTurn(Battle& b, BattleFighter& f);
    void npcTurn(Battle& b, BattleFighter& f);
    void playPlan(Battle& b, BattleFighter& f);
    void senseOne(Battle& b, const BattleFighter& observer, const BattleFighter& target, bool moving);
    void sensedBy(Battle& b, const BattleFighter& target);
    void senseFoes(Battle& b, const BattleFighter& observer);
    void sprungOn(Battle& b, const BattleFighter& f, const BattleFighter& t);
    void hideSneakers(Battle& b);
    void revealFighter(Battle& b, BattleFighter& f, const std::string& line);
    void beginPlacing(Battle& b);
    void endPlacing(Battle& b);
    void mapZones(Battle& b) const;
    void unstick(Battle& b, BattleFighter& f);
    std::vector<std::pair<int, int>> reachWith(const Battle& b, const BattleFighter& f, const Entity& e, double stamina, int less = 0) const;
    Result bite(Battle& b, BattleFighter& f, const std::string& target);
    void downFighter(Battle& b, BattleFighter& f, double overkill, double base, const std::string& by);
    void yieldFighter(Battle& b, BattleFighter& f, const std::string& to, const std::string& line);
    void fightLine(Battle& b, const std::string& actor, const std::string& target, const std::string& kind, std::string text);
    void checkOver(Battle& b);
    void finishBattle(Battle& b);
    void leaveArena(Battle& b, BattleFighter& f, bool fleeing);
    std::vector<std::pair<int, int>> reachFrom(const Battle& b, const BattleFighter& f, int range) const;
    // Steps from each arena tile to (x, y), walking as fighters do (eight ways, no corner cut, around others but
    // `mover`): row by row over the arena, -1 where there is no way.
    std::vector<int> stepsTo(const Battle& b, int x, int y, const std::string& mover) const;
    // The tiles a fighter walks to (x, y), in order, not counting where it stands; empty if there is no way.
    std::vector<std::pair<int, int>> walkTo(const Battle& b, const BattleFighter& f, int x, int y) const;
    void walkFighters(Battle& b);
    double stepSeconds(const BattleFighter& f) const;
    void standUp(Entity& e, double health);
    void tendDowned(double dt);
    void fullRest(Entity& e);
    void restPlayers(double dt);

  public:
    // A player back in the world (doc 38): the time they were away counts down their downed period and rests them.
    void returnFromAway(Entity& e);
    // Lying on a bed or straw (doc 36's `b` and `z` tiles): where a full rest is had (doc 38).
    bool inBed(const Entity& e) const;
    // Taverns (doc 54, 1): a full rest needs a bed the wolf has a right to (its lodging, a paid inn bed, its Chapter's
    // place: the user, 2026-10-08), set by Game; without the check (no game), any bed. A common room (set by Game every
    // few seconds: its active players and whether a performer is there) heals 1.25 rest hours an hour, +10% for each
    // other active player up to +30%, or +30% with a performer; a partial rest.
    void setBedRight(std::function<bool(const std::string& id, const std::string& cell)> right) { bedRight_ = std::move(right); }
    bool bedIsTheirs(const Entity& e) const { return inBed(e) && (!bedRight_ || bedRight_(e.id, e.cellId)); }
    struct CommonRoom
    {
        std::set<std::string> active;
        bool performer = false;
    };
    void setCommonRooms(std::map<std::string, CommonRoom> rooms) { commonRooms_ = std::move(rooms); }
    const CommonRoom* commonRoom(const std::string& cell) const;
    double companyFactor(const Entity& e) const;    // 1 alone; up to 1.3 (0 outside a common room).
    double restRate(const Entity& e) const;         // Rest hours an hour where it stands, if still (doc 38, doc 54).
    // Renting (doc 54, 4): a cell's bed tiles, and those of a home nobody of the household sleeps on (spare beds).
    std::vector<std::pair<int, int>> bedTiles(const std::string& cellId) const;
    std::vector<std::pair<int, int>> spareBeds(const std::string& cellId) const;

  private:
    std::map<std::string, CommonRoom> commonRooms_;

  public:
    int fightPace(const Entity& e) const;           // The pace a fighter moves at: theirs, an NPC's run, or a walk.

  private:
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
    // Injuries (doc 38): who went down in a fight, and who has had their one lasting injury from it.
    struct FightMarks
    {
        std::set<std::string> downed, marked;
    };
    std::map<std::string, FightMarks> fightMarks_;
    void giveLasting(Battle& b, Entity& e, const std::string& cause, const std::string& by, double chance, const Injury* from);
    std::string injuryCause(double downedBase, const std::string& by) const;
    std::string injurerWords(const std::string& by) const;
    std::uint64_t nextInjury_ = 0;
    std::string injuryId() { return "injury-" + std::to_string(std::int64_t(calendarDays_ * 14400)) + "-" + std::to_string(++nextInjury_) + "-"; }
    void tendPrices();
    void residentsTakeWork();
    void tradeBetweenTowns();
    Caravan* sendCaravan(const Town& from, const Town& to, const std::map<std::string, int>& load);
    // Work on the road a resident has taken (a letter to carry, a caravan to guard): where it takes them now,
    // in place of the day's plan. False if they have none, or it waits (for the morning, say).
    // Who has work on the roads (doc 46's speed pass): a resident's taken contracts and the standing orders it is sent to
    // renegotiate, by their places in roads_ (in order), remade at each errand pass (indexErrands), so errand() needn't
    // look through all the land's contracts for each resident.
    std::unordered_map<std::string, std::vector<std::size_t>> errandContracts_, errandOrders_;
    void indexErrands();
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
    // A resident's own spot by a goal others share, and whether someone stands on it now (bodies: step::BodyRadius).
    Vec2 spotNear(const Entity& e, const std::string& goalCell, Vec2 target) const;
    bool spotTaken(const Entity& e, const std::string& cellId, Vec2 spot) const;
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
    // entitiesIn's index. A copy of the world starts with none (its pointers would be into the other world's entities).
    struct CellIndex
    {
        std::unordered_map<std::string, std::vector<const Entity*>> cells;
        std::uint64_t tick = ~0ULL;
        std::size_t size = 0;
        bool dirty = true;
        CellIndex() = default;
        CellIndex(const CellIndex&) {}
        CellIndex& operator=(const CellIndex&)
        {
            cells.clear();
            dirty = true;
            return *this;
        }
    };
    mutable CellIndex index_;
    Parallel parallel_;
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
    std::vector<WeatherSystem> systems_, fronts_;
    std::int64_t fieldStamp_ = std::numeric_limits<std::int64_t>::min();
    std::uint64_t frontNext_ = 1;
    void refreshWeatherField(bool force = false);
    WeatherSample sampleField(double x, double y) const;
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
    // Someone could stand at p, on its own tile's ground, at whatever height that is. (passable() with no `from` asks
    // about stepping up from height 0, which rules out any ground above or below it.)
    bool standable(const std::string& cellId, Vec2 p) const;
    // One fighter's step from tile (x, y) to (nx, ny) of a cell: standable there, and no ledge between.
    bool stepBetween(const std::string& cellId, int x, int y, int nx, int ny) const;
    bool visiblePoint(const Entity& observer, Vec2 point) const;
    bool visiblePoint(const Entity& observer, Vec2 point, double range) const;
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
    std::vector<Vec2> findPath(const Entity& actor, Vec2 goal, bool allowClosed = false) const;
    std::vector<Vec2> searchPath(const Entity& actor, Vec2 goal, bool allowClosed) const;   // findPath, uncached.
    // searchPath's checks before the search: false when no route can exist; else the closed doors' tiles, for astar.
    bool searchable(const Entity& actor, Vec2 goal, bool allowClosed, std::vector<char>& closedTiles) const;
    struct NavScratch;
    static std::vector<Vec2> astar(const Cell& cell, Vec2 origin, Vec2 goal, bool allowClosed, const std::vector<char>& closedTiles,
                                   NavScratch& nav, std::size_t& expanded);
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
    // The route thread (setRoutesOffThread): findPath, asked from a resident's seek (routeAsync_), hands a missing
    // route to it and says so in routePending_; takeRoutes() puts what it found in the path cache at the start of a tick.
    class RouteWorker;
    struct RouteWorkerSlot
    {
        std::shared_ptr<RouteWorker> worker;
        std::set<PathKey> requested;            // Handed over and not yet back.
        std::map<std::string, PathKey> waiting; // Resident → the route they wait for (skipped until it's back).
        PathKey last;                           // The route findPath last handed over or found waiting.
        RouteWorkerSlot() = default;
        RouteWorkerSlot(const RouteWorkerSlot&) {}
        RouteWorkerSlot& operator=(const RouteWorkerSlot&) { return *this; }
    };
    mutable RouteWorkerSlot routes_;
    mutable bool routeAsync_ = false, routePending_ = false;
    bool deterministic_ = false;
    // The ground of each cell as the route thread reads it: a copy, kept while the ground (its checksum) is unchanged.
    mutable std::map<std::string, std::pair<std::uint64_t, std::shared_ptr<const Cell>>> routeCells_;
    void takeRoutes();
    void integrate(Entity& actor, double dt);
    void updateStamina(Entity& actor, double dt, double movedTime);
    void updateTravel(Entity& actor);
    void prepareMovement(Entity& actor);
    void transition(Entity& actor, const Door& door);
    // Through a door or over a cell's edge, if walking from where `actor` stands toward `proposed` leads there.
    bool throughDoor(Entity& actor, const Cell& c, Vec2 direction, Vec2 proposed, double travel, double speed, double& movedTime);
    void updateSchedules();
    // The schedules pass as a chain of stages (doc 31, Phase 5): the society's half hour, bonds, roads, crime, then
    // each resident's errand, then what streaming keeps. continueSchedules() runs stages in order until `budgetMs` is
    // spent (a stage is never cut short, but the errands go a resident at a time) and says whether the chain is done;
    // the rest wait for the next step. In a small world the whole chain fits in one step, as before.
    bool continueSchedules(double budgetMs);
    static constexpr double ScheduleBudgetMs = 3;
    int scheduleStage_ = -1;                        // The next stage of the chain under way; -1 for none.
    std::string errandCursor_;                      // The last resident whose errand was seen to.
    std::set<std::string> errandStage_;             // The cells on stage for this chain (tiered).
    void separate(double dt);
    void createDemo();
    void rebuildFixtureIndex();
    bool blockedByDoor(const std::string& cellId, Vec2 point) const;
    class WalkingGrid;                                      // A cell as walking (RatwStep.h) sees it.
};

Tile tileFromGlyph(char glyph); // Terrain rules shared by built-in and authored cells.
const TerrainInfo* terrainInfo(char code); // Null for a code the catalog does not know.
const std::vector<TerrainInfo>& terrainCatalog();
const char* weatherName(Weather value);
bool parseWeather(const std::string& name, Weather& out); // False for an unknown name; `out` is then unchanged.
// How strong a weather system is at a point on a calendar day: 0 outside its reach or its life (RatwWeather.cpp).
double weatherStrengthAt(const WeatherSystem& system, double x, double y, double day);
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
