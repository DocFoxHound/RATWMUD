#pragma once
// The game (Docs/Design/26-living-npcs.md, Phase 6; 27-browser-client.md): the one authority. The server
// (Server/ratw_server.cpp) runs it behind its WebSockets; tests run it directly. It holds the world and its society,
// accounts and characters, commands, snapshots and events, NPC conversation through the NPC Mind, the Dungeon Master's
// actions and spawn rules, saves, and releases. A host (the server, or a test) makes a Connection for each client and
// calls in.
#include "RatwAccountsCore.h"
#include "RatwCheckpoint.h"
#include "RatwDbStore.h"
#include "RatwDirector.h"
#include "RatwJsonDoc.h"
#include "RatwMind.h"
#include "RatwPg.h"
#include "RatwSections.h"
#include "RatwSocialCore.h"
#include "RatwWorld.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace ratw::game
{
// One client as the game sees it. The host delivers what the game sends, and keeps the object alive until it has
// called Game::disconnect for it.
class Connection
{
  public:
    virtual ~Connection() = default;
    virtual void event(const std::string& json) = 0;          // Reliable and in order.
    virtual void snapshot(const std::string& json) = 0;       // May be lost; each replaces the last.
    virtual void motion(const json::Value& frame) = 0;        // May be lost (the host sends it in binary).
    // Account passwords are accepted only from this computer: the native transport is not encrypted.
    virtual bool allowsLocalCredentials() const = 0;

    std::uint64_t id = 0;                                     // Unique for the life of the server.
    std::string entityId, developmentIdentity, accountUsername;
    std::string motionSession, motionCell;
    int motionGeneration = 0;
    sections::Held held;                                      // What of its snapshots it is known to hold.
};

// Where the game's save lives. The database (DbStore) for a world from the database; a file for a world from files.
class Store
{
  public:
    virtual ~Store() = default;
    virtual bool database() const = 0;
    virtual std::string load() = 0;                           // "" for none yet; {"schema":-1} if unreadable.
    virtual bool save(DbStore::Build build, std::uint64_t revision) = 0;               // Stored before returning.
    virtual bool saveInBackground(DbStore::Build build, std::uint64_t revision) = 0;
    virtual bool flush() = 0;
    virtual void queueEvents(std::vector<WorldEvent> events) = 0;
    virtual std::vector<std::pair<std::string, std::string>> externalNpcStates() = 0;
    virtual std::string error() const = 0;
};
// The database store (game.checkpoints) as a Store.
std::unique_ptr<Store> databaseStore(const std::string& conninfo, const std::string& worldId, std::string& error);
// A private file (owner-only permissions), written whole each time by replacing it.
std::unique_ptr<Store> fileStore(const std::string& path, std::string& error);
// Kept in memory only: for tests.
std::unique_ptr<Store> memoryStore();

struct Options
{
    // A world from the database: "prod" or "dev", read with `conninfo` (a libpq connection string). Else a world
    // file (RATW_WORLD manifest, absolute path), else the built-in demo world.
    std::string database, conninfo, worldFile;
    std::string savePath;                                     // For a world not from the database.
    std::vector<std::string> cellFiles;                       // For the demo world: authored cell files loaded over it.
    // Whether a save that can't be read (or saved to) stops the server. Always for a database world; a world from files
    // may play on without saving (as the offline demo worlds do), with the save left as it was.
    bool requireStorage = true;
    std::string dialogueEndpoint;                             // The NPC Mind (loopback only); empty for authored lines.
    std::string directorDirectory;                            // The operator bridge's private directory (RatwDirector.h).
    bool devTools = false, devIdentity = false;               // Development-only commands and "hello" logins.
    bool fullSnapshots = false;                               // Send every snapshot whole (see RatwSections.h).
    std::string connectionLabel = "Authoritative server · 20 Hz";
};

class Game
{
  public:
    explicit Game(Options options);
    ~Game();
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    // Where the save of a world not from the database lives, in place of `savePath`: set before start().
    void useStore(std::unique_ptr<Store> store) { customStore_ = std::move(store); }
    // Loads the world and its save. False, with the problem, if the server must not run.
    bool start(std::string& problem);
    // Where notes go (level: "info", "warning", "error"). Standard error until set.
    std::function<void(const char* level, const std::string& text)> log;

    void connect(Connection* c);                              // A new client: it is shown the lobby.
    void disconnect(Connection* c);                           // Its character leaves the world first.
    void command(Connection* c, const std::string& json);
    void acknowledge(Connection* c, double revision, bool missing);   // It applied that snapshot (or lacks a part).
    void tick(double dt);                                     // 20 times a second (dt 0.05).
    void save();                                              // Stored before returning.
    // A status the host should exit with, once asked (75: a new release was published and nobody is playing).
    int exitRequested() const { return exit_; }
    bool storageReady() const { return storageReady_; }
    World& world() { return world_; }
    const std::map<std::string, Entity>& characters() const { return characters_; }
    std::size_t clients() const { return clients_.size(); }
    std::uint64_t revision() const { return revision_; }

    static constexpr double AutosaveSeconds = 15, SaveSoonSeconds = 3;
    static constexpr unsigned SnapshotPhases = 4;

  private:
    Options options_;
    World world_;
    MemoryStore memories_;
    SocialLedger social_;
    std::unique_ptr<Store> store_, customStore_;
    mind::Client mind_;
    accounts::Accounts accounts_;
    accounts::RateLimit authRate_;
    director::Bridge director_;
    std::vector<Connection*> clients_;
    std::map<std::string, Entity> characters_;
    std::map<std::string, double> operatorActivity_, typingExpiry_, lastChat_, npcLastSpeech_, lastMovementSound_;
    std::map<std::string, std::string> companionOwner_;
    std::set<std::string> pendingNpc_;
    struct QueuedTalk
    {
        std::string playerId, heardText;
        Voice voice = Voice::Speak;
        bool hasSense = false;
        SensoryResult sense;
    };
    static constexpr std::size_t MaxQueuedTalk = 4;
    std::map<std::string, std::deque<QueuedTalk>> queuedTalk_;
    std::map<std::string, std::string> npcMood_;
    // The ambient director (RatwGameAmbient.cpp, Phase 10): exchanges being voiced or spoken, and the model calls
    // made for them in the last hour (world seconds).
    struct AmbientTalk
    {
        std::uint64_t id = 0;
        AmbientPick pick;
        std::deque<std::pair<int, std::string>> lines;     // Still to say: 0 the teller, 1 the listener.
        double nextAt = 0;
        bool voiced = false, generated = false;
    };
    std::vector<AmbientTalk> ambient_;
    std::deque<double> ambientCalls_;
    double exchangeLookIn_ = 0;
    std::uint64_t ambientNext_ = 1;
    struct NudgeBudget
    {
        std::int64_t hour = -1;
        int affinity = 0, trust = 0;
    };
    std::map<std::string, NudgeBudget> nudgeBudget_;
    std::map<std::string, std::vector<std::string>> commandReceipts_;
    std::map<std::string, std::map<std::string, std::string>> responseReceipts_;
    std::map<std::string, std::string> currentCommands_;
    std::uint64_t sequence_ = 1, revision_ = 0;
    double dmExpiryAccumulator_ = 60;
    bool dmNotified_ = true;
    unsigned snapshotPhase_ = 0;
    double saveSoonIn_ = -1, snapshotAccumulator_ = 0, saveAccumulator_ = 0, ambientAccumulator_ = 0,
           releaseAccumulator_ = 0, dmAccumulator_ = 0, spawnAccumulator_ = 0, prefetchAccumulator_ = 0, streamLogAccumulator_ = 0;
    std::map<std::string, double> deadSince_, spawnBackoff_;
    PgClient worldDb_;
    std::int64_t loadedBuild_ = 0, pendingRelease_ = 0;
    std::map<std::string, std::string> worldFiles_, cellHeaders_;
    std::string liveWorldId_;
    bool streamedBuild_ = false, releaseAnnounced_ = false, storageReady_ = false;
    int exit_ = -1;
    std::mt19937_64 random_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);   // Answers arriving after the game is gone are dropped.

    void note(const char* level, const std::string& text);
    static double now();                                      // Unix seconds.
    static double clock();                                    // Monotonic seconds.
    std::string guid();                                       // 32 lowercase hex digits.

    // Loading the world.
    bool loadFromDatabase(std::string& problem);
    World::CellSource cellSource();
    bool loadWithLivePeople(World& into, std::string& problem);
    static std::string withoutPeople(const std::string& manifest);
    void readNotifications();
    void watchReleases(double dt);
    void applyDmActions(double dt);
    void makeResidents();
    void runSpawns(double dt);
    bool spawnTile(const std::string& area, const std::string& tilesJson, int& x, int& y);
    void applyExternalNpcStates();

    // Clients.
    void send(Connection* c, const json::Value& e);
    void system(Connection* c, const std::string& message);
    void lobby(Connection* c, bool ok = true, const std::string& message = {});
    bool enterCharacter(Connection* c, const std::string& actor, const std::string& name, const std::string& developmentId = {});
    void leaveCharacter(Connection* c);
    bool accountCommand(Connection* c, const json::Value& j, const std::string& type);
    void login(Connection* c, const json::Value& j);
    Connection* clientOf(const std::string& entityId) const;

    // The world as each client sees it.
    void stampFrame(Connection* c, json::Value& root, const std::string& cell);
    void sendSnapshot(Connection* c);
    void movementSounds();
    std::vector<std::string> publish(const std::string& author, const ParsedPost& post, Voice voice);
    void logEvent(const char* kind, const std::string& actor, const std::string& target = {}, const std::string& detail = {});
    void followTransition(const std::string& id, const std::string& previousCell);

    // NPC conversation.
    void talk(const std::string& npcId, const std::string& playerId, const std::string& heardText, Voice voice = Voice::Speak,
              const SensoryResult* perceived = nullptr);
    void consolidate();
    void heed(const std::string& npcId, const std::string& subjectId, bool identified, const mind::Reply& reply);
    void talkNext(const std::string& npcId);
    void ambient(double dt);                                  // Picks, voices and speaks NPC-to-NPC exchanges.
    static constexpr int AmbientCallsPerHour = 30;

    // Saving.
    void saveSoon();
    void autosave();
    DbStore::Build capture();
    void load(const std::string& payload);
};
} // namespace ratw::game
