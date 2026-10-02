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
#include "RatwJournal.h"
#include "RatwJsonDoc.h"
#include "RatwMind.h"
#include "RatwVoice.h"
#include "RatwScenes.h"
#include "RatwArtwork.h"
#include "RatwPerf.h"
#include "RatwPg.h"
#include "RatwSections.h"
#include "RatwSocialCore.h"
#include "RatwWorld.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <fstream>
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
    // Movement (Docs/Design/31-responsiveness.md, Phase 3): whether this client walks its own wolf when it may (it
    // asks, with {"type":"walking","mode":"client"}), and the mode the server has it in: 0 free, 1 held (the server
    // walks it: the moments around a fight), 2 fighting (in an arena: no walking at all).
    bool clientWalking = false;
    // It last walked by keys (a "move"), not by poses: the server walks it until a pose comes (rising from a sit, or a
    // script that drives the wolf by keys).
    bool keysWalking = false;
    std::uint8_t movementMode = 0;
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
    // The journal of valuable changes (RatwJournal.h; Docs/Design/31-responsiveness.md, Phase 2), and its records after
    // a seq. Null where there is none: valuables are then saved whole, waiting, as before.
    virtual journal::Writer* journal() { return nullptr; }
    virtual bool journalAfter(std::uint64_t, std::vector<journal::Record>&, std::string&) { return true; }
    // The file a forked snapshot writes itself (a world saved to a file); "" for a store that is handed the document.
    virtual std::string snapshotFile() const { return {}; }
    // The highest revision stored so far; a store that stores before save() returns says so at once.
    virtual std::uint64_t storedRevision() const { return UINT64_MAX; }
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
    // A world build exported as files (world.ratw and the rest, cells/ID.cell, seams/ID: what tools/world_build.py
    // export writes), played offline with its residents as built: the load test's world. In place of worldFile.
    std::string worldExport;
    std::string savePath;                                     // For a world not from the database.
    std::vector<std::string> cellFiles;                       // For the demo world: authored cell files loaded over it.
    // Whether a save that can't be read (or saved to) stops the server. Always for a database world; a world from files
    // may play on without saving (as the offline demo worlds do), with the save left as it was.
    bool requireStorage = true;
    std::string dialogueEndpoint;                             // The NPC Mind (loopback only); empty for authored lines.
    // Cheaper voices (Docs/Design/28-ai-cost.md): the speech router and exchange library (a directory with
    // router.json and library.json; empty: off), and the ledger of who answered each NPC line (empty: none kept).
    std::string voiceData, voiceLog;
    // Overheard NPC exchanges written live by the Mind, at most this many an hour; 0 (the default): never, the written
    // scenes (voiceData/scenes, doc 30) and the library voice them all.
    int ambientModelCallsPerHour = 0;
    std::string directorDirectory;                            // The operator bridge's private directory (RatwDirector.h).
    bool devTools = false, devIdentity = false;               // Development-only commands and "hello" logins.
    bool fullSnapshots = false;                               // Send every snapshot whole (see RatwSections.h).
    // Snapshots of the world taken by a forked copy of the server, so the game never waits for one (doc 31, Phase 2).
    // For a database or file world; tests turn it off to snapshot in place.
    bool forkSnapshots = true;
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
    // Waits for the journal's records to be written and sends the replies waiting for them (tests and tools; the
    // tick does this as it goes, without waiting).
    void settle();
    // A character in a fight (doc 33's arenas, when they come): no walking of any kind until it ends.
    void setFighting(const std::string& id, bool fighting);
    enum MovementMode : std::uint8_t
    {
        FreeMovement = 0,
        HeldMovement = 1,
        Fighting = 2,
    };
    // Held around a fight: this close to a standing hostile, or this soon after a blow or an offence.
    static constexpr double HostileNear = 8, HeldAfter = 10;
    // A status the host should exit with, once asked (75: a new release was published and nobody is playing).
    int exitRequested() const { return exit_; }
    // Where the game thread's time is counted (Docs/Design/31-responsiveness.md, Phase 1); none by default.
    void setMeter(perf::Meter* meter)
    {
        meter_ = meter;
        worldDb_.meter = meter;
    }
    bool storageReady() const { return storageReady_; }
    World& world() { return world_; }
    // What the game says itself to what a player said to an NPC (doc 28), "" when a model should answer: for the voice
    // checks and the review page (tools/ai_review.py) as much as for play.
    std::string gameAnswer(const std::string& npcId, const std::string& playerId, const std::string& heard, bool identified);
    // What an NPC Mind is told for a reply to what a player said (also for the review page).
    mind::Context dialogueContext(const std::string& npcId, const std::string& playerId, const std::string& heardText,
                                  bool identified);
    const std::map<std::string, Entity>& characters() const { return characters_; }
    // The written scenes (doc 30): how many are loaded, and how many a character has heard.
    std::size_t sceneLibrarySize() const { return scenes_.size(); }
    // A Dungeon Master's decision on an uploaded portrait: "approve" or "reject" (doc 29, phase 9).
    Result reviewArtwork(const std::string& id, const std::string& decision, const std::string& reason);
    const std::map<std::string, art::Meta>& artworks() const { return artworkMeta_; }
    std::size_t scenesHeardBy(const std::string& id) const
    {
        const auto found = scenesHeard_.find(id);
        return found == scenesHeard_.end() ? 0 : found->second.order.size();
    }
    std::size_t clients() const { return clients_.size(); }
    std::uint64_t revision() const { return revision_; }

    // A snapshot of the world every SnapshotSeconds (and SaveSoonSeconds after a change worth keeping); its memory
    // consolidated every AutosaveSeconds.
    static constexpr double AutosaveSeconds = 15, SnapshotSeconds = 5, SaveSoonSeconds = 3;
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
    // Passwords are worked on off this thread (doc 31): a sign-in or registration finishes in a later tick.
    accounts::Hasher hasher_;
    struct PendingSignIn
    {
        Connection* c;
        bool registering;
        std::string user;
    };
    std::map<std::uint64_t, PendingSignIn> signIns_;
    std::uint64_t nextSignIn_ = 0;
    void finishSignIns();
    std::set<std::string> fighting_;
    void updateMovementModes();
    director::Bridge director_;
    std::vector<Connection*> clients_;
    std::map<std::string, Entity> characters_;
    std::map<std::string, double> operatorActivity_, typingExpiry_, lastChat_, npcLastSpeech_, lastMovementSound_;
    // Several NPCs spoken to at once answer in turn (Docs/Design/29, phase 4): after the NPC keyed here replies, the next.
    struct TalkTurn
    {
        std::string npcId, heardText;
        SensoryResult sense;
    };
    struct TalkChain
    {
        std::string playerId;
        Voice voice = Voice::Speak;
        std::deque<TalkTurn> rest;
        std::string said;                          // What the earlier ones answered, for the later ones to hear.
    };
    std::map<std::string, TalkChain> chainAfter_;
    void continueChain(TalkChain chain);
    std::map<std::string, std::string> replyingTo_;     // NPC → the player it is answering (for "→ you").
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
    double saveSoonIn_ = -1, snapshotAccumulator_ = 0, saveAccumulator_ = 0, snapshotSaveAccumulator_ = 0, ambientAccumulator_ = 0,
           releaseAccumulator_ = 0, dmAccumulator_ = 0, spawnAccumulator_ = 0, prefetchAccumulator_ = 0, streamLogAccumulator_ = 0;
    std::map<std::string, double> deadSince_, spawnBackoff_;
    PgClient worldDb_;
    perf::Meter* meter_ = nullptr;
    std::int64_t loadedBuild_ = 0, pendingRelease_ = 0;
    std::map<std::string, std::string> worldFiles_, cellHeaders_;
    std::map<std::string, std::pair<std::string, std::string>> exportCells_;   // A world export's cells: body, seams.
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
    bool loadExport(std::string& problem);
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
    // Speech to everyone who can perceive it. `to`: whom it was meant for (each listener is told, as they can tell).
    std::vector<std::string> publish(const std::string& author, const ParsedPost& post, Voice voice, const std::vector<std::string>& to = {});
    void logEvent(const char* kind, const std::string& actor, const std::string& target = {}, const std::string& detail = {});
    void followTransition(const std::string& id, const std::string& previousCell);

    // NPC conversation.
    // An NPC answers what a player said; `alsoHeard` is what others just answered (several spoken to at once). False
    // if the NPC couldn't take it up at all.
    bool talk(const std::string& npcId, const std::string& playerId, const std::string& heardText, Voice voice = Voice::Speak,
              const SensoryResult* perceived = nullptr, const std::string& alsoHeard = {});
    void consolidate();
    void heed(const std::string& npcId, const std::string& subjectId, bool identified, const mind::Reply& reply);
    void talkNext(const std::string& npcId);
    void ambient(double dt);                                  // Picks, voices and speaks NPC-to-NPC exchanges.
    // Cheaper voices (RatwGameVoice.cpp): the game's own answer to what was said ("" leaves it to a model), a reply
    // spoken and remembered, and a line of the ledger (no words in it: who, what kind, and which route).
    void speakReply(const std::string& npcId, const std::string& subjectId, bool identified, const mind::Reply& reply,
                    const char* route);
    void voiced(const char* kind, const char* route, const std::string& npcId);
    voice::Rules voices_;
    std::ofstream voiceLog_;
    struct Said
    {
        std::string asked, answer, line;
        double at = 0;
    };
    std::map<std::string, Said> lastSaid_;                    // "npc|player": the last the game answered itself.
    std::map<std::string, std::set<std::size_t>> recentExchanges_;   // By cell: library entries heard lately.
    std::set<std::string> voicedIncidents_;                   // Incidents an exchange was written live for.
    double polishOffUntil_ = 0;
    std::uint64_t voiceSeed_ = 0;
    // The written scenes (doc 30), and what each player character has heard of them (newest last, bounded).
    scenes::Library scenes_;
    struct Heard
    {
        std::deque<std::string> order;
        std::set<std::string> ids;
        void add(const std::string& id)
        {
            if (!ids.insert(id).second)
                return;
            order.push_back(id);
            while (order.size() > 3000)
            {
                ids.erase(order.front());
                order.pop_front();
            }
        }
    };
    std::map<std::string, Heard> scenesHeard_;
    // Uploaded portraits (RatwGameArtwork.cpp; doc 29, phase 9): where they are kept, every record, uploads under way.
    std::unique_ptr<art::Store> artwork_;
    std::map<std::string, art::Meta> artworkMeta_;
    struct Upload
    {
        std::string id, character;
        std::vector<unsigned char> data;
        int parts = 0, received = 0;
        double startedAt = 0;
    };
    std::map<std::uint64_t, Upload> uploads_;
    bool artworkCommand(Connection* c, const json::Value& j, const std::string& type);
    std::string artworkOwner(const Connection* c) const;
    bool ownsCharacter(const Connection* c, const std::string& character) const;
    const art::Meta* portraitOf(const std::string& character) const;
    // The portrait `viewer` may see of `character` ("" for none): approved, or their own.
    std::string visiblePortrait(const std::string& character, const std::string& viewer) const;
    std::map<std::string, std::deque<std::string>> recentScenes_;   // By cell: scenes said there lately.
    std::map<std::string, double> barkLast_;                         // By cell: when someone last called out there.
    double barkLookIn_ = 0;
    void barks(double dt);
    // The players in a cell who can hear `speaker` say something (for the heard lists).
    std::vector<std::string> hearersOf(const std::string& speaker) const;
    scenes::Person scenePerson(const Entity& e) const;

    // Saving.
    void saveSoon();
    void autosave();
    DbStore::Build capture();

    // ------------------------------------------------------------------ The journal and snapshots (RatwGameJournal.cpp)
    // What a valuable command may have changed: what the journal compares and records.
    enum Valuable : unsigned
    {
        Economy = 1,       // Purses, stock, the economy's ledger and counters.
        Careers = 2,       // Who holds and learns each position.
        Roads = 4,         // Caravans, camps and contracts.
        Crime = 8,         // Incidents, warrants, custody.
        Companions = 16,   // Who travels with whom.
        Accounts = 32,     // Sign-in accounts and the characters they own.
        Character = 64,    // The character itself.
    };
    // Journals what a valuable command changed (or, with no journal, saves whole and waits, as before). Replies to
    // the command's client wait for the record to be written.
    void record(unsigned what, const std::string& character = {});
    // The journal's view of what was last recorded, to find what changed since.
    struct Shadow
    {
        bool primed = false;
        std::map<std::string, EconomyAccount> accounts;
        std::vector<std::int64_t> economy;          // Its counters, in a fixed order.
        std::uint64_t ledgerLast = 0;
        std::size_t ledgerSize = 0;
        std::map<std::string, PositionState> positions;
        std::string roads, crime, companions, signIns;
    };
    void prime(Shadow& shadow) const;
    std::uint64_t journalSeq_ = 0;
    Shadow shadow_;
    // Replies held for a command whose record isn't written yet: released by releaseCommitted() in each tick.
    Connection* holding_ = nullptr;
    std::vector<std::string> held_;
    std::uint64_t heldFor_ = 0;
    struct Waiting
    {
        Connection* c;
        std::uint64_t seq;
        std::string event;
    };
    std::deque<Waiting> waiting_;
    void beginHolding(Connection* c);
    void endHolding();
    void releaseCommitted();
    bool journalFailing_ = false;
    // A forked snapshot under way, and the ones handed to the store whose journal records can go once stored.
    int snapshotChild_ = -1;
    std::uint64_t snapshotJournal_ = 0, snapshotRevision_ = 0;
    std::string snapshotDocument_, snapshotStates_;
    int forkFailures_ = 0;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> trimWhenStored_;   // (revision, journal seq)
    bool forkSnapshot();
    void reapSnapshot(bool wait);
    void trimStored();
    void syncCharacters();
    bool replayJournal(json::Value& document, std::string& problem);
    void load(const std::string& payload);
};
} // namespace ratw::game
