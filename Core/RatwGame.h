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
#include "RatwNames.h"
#include "RatwParty.h"
#include "RatwVoice.h"
#include "RatwScenes.h"
#include "RatwArtwork.h"
#include "RatwChapters.h"
#include "RatwFactions.h"
#include "RatwEstates.h"
#include "RatwCamps.h"
#include "RatwPerf.h"
#include "RatwPg.h"
#include "RatwPool.h"
#include "RatwSections.h"
#include "RatwPeople.h"
#include "RatwReports.h"
#include "RatwStanding.h"
#include "RatwStars.h"
#include "RatwBooks.h"
#include "RatwNewcomers.h"
#include "RatwDocuments.h"
#include "RatwSocialCore.h"
#include "RatwTavernGames.h"
#include "RatwFame.h"
#include "RatwChronicle.h"
#include "RatwTroubles.h"
#include "RatwProjects.h"
#include "RatwWatch.h"
#include "RatwHealth.h"
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
    // The snapshot as values, for a host that sends it in a form of its own (the server packs it: RatwPack.h). By
    // default, as JSON text to snapshot(). Called on the game's worker threads, one at a time for any one client.
    virtual void snapshotValue(const json::Value& root) { snapshot(json::dump(root)); }
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
    // It shades the terrain itself (doc 31, Phase 4.6): its snapshots leave the visibility rows out.
    bool clientSight = false;
    // It last walked by keys (a "move"), not by poses: the server walks it until a pose comes (rising from a sit, or a
    // script that drives the wolf by keys).
    bool keysWalking = false;
    std::uint8_t movementMode = 0;
    // Its line to the server as its page last told it (doc 31, the health tracker: {"type":"net"} every 30 s): ping
    // in ms, the middle, the 95th percentile and the worst of the last minute's; corrections ever received; when.
    double netP50 = -1, netP95 = -1, netMax = -1, netAt = -1;
    int netCorrections = 0, netCorrectionsCounted = 0;
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
// A scratch server's store (Options::scratch): reads its save from `inner`, writes nothing anywhere.
std::unique_ptr<Store> scratchStore(std::unique_ptr<Store> inner);

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
    // Residents' letters (doc 55, 5) polished by the light model: at most this many an hour; 0 (the default): templates.
    int letterModelCallsPerHour = 0;
    // Performing (doc 54, 1): how long a performer may be quiet, and perform at most (real seconds). Tests shorten them.
    double performQuietSeconds = 120, performLongestSeconds = 1800;
    double awayBreakSeconds = 3 * 86400;   // A break, for welcome back (doc 56, 10): three real days.
    std::string directorDirectory;                            // The operator bridge's private directory (RatwDirector.h).
    bool devTools = false, devIdentity = false;               // Development-only commands and "hello" logins.
    bool fullSnapshots = false;                               // Send every snapshot whole (see RatwSections.h).
    // A scratch server (Docs/Design/20-world-database.md, "Scratch servers"): the world and its save are read as usual,
    // but nothing is ever written back: no saves, journal, DM actions taken, spawns recorded, artwork, LIVE map or
    // health; its database session is read-only besides. It doesn't need to own the world, so it runs beside the
    // real server and any number of other scratch servers.
    bool scratch = false;
    // Snapshots of the world taken by a forked copy of the server, so the game never waits for one (doc 31, Phase 2).
    // For a database or file world; tests turn it off to snapshot in place.
    bool forkSnapshots = true;
    // Worker threads that key, write and compress players' snapshots in parallel after each tick (doc 31, Phase 4); 0
    // (the default, and tests') does it all on the game thread. The server asks for the cores it has, less two.
    unsigned workerThreads = 0;
    std::string connectionLabel = "Authoritative server · 20 Hz";
    // Names hidden until introduced (doc 32, 1.5). Off only for tests written before introductions.
    bool hiddenNames = true;
    // The social level each of a Chapter's three founders needs (doc 32, 3.1; a placeholder). Tests lower it.
    int chapterFoundingLevel = 5;
    // Earned Gift tiers (doc 49, Phase 5): with `openTiers` every account may make Gifted and Quickened wolves (tests,
    // scratch servers: --open-tiers); `oneWolfPerAccount` keeps an account's wolves out of the world together, so
    // nobody pays or stars themselves.
    bool openTiers = false;
    bool oneWolfPerAccount = true;
    // Ties (doc 52, 4): a new account's first wolf must take one; tests of other things make wolves without.
    bool tiesOptional = false;
    double tieLapseSeconds = -1, tieOfferSeconds = -1;        // -1: the rules' own (7 days, 3 minutes). Tests shorten them.
    // Scene recaps (doc 50, 5): how long the lines a member perceived must span for the model to write their recap, in
    // seconds; -1 for the rules' own (5 minutes). Tests lower it.
    double recapModelSeconds = -1;
    // How fast the world runs (Game::setSpeed): 1 as ever, 16 sixteen times as fast.
    double speed = 1;
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
    // Server health (doc 31, the health tracker; Core/RatwHealth.h): a minute's window as the host's meter saw it, with
    // the world's own parts, players' pings and slow connections; and one tick that ran long. Kept for a world in the
    // database or saved to a file; nowhere for one in memory.
    struct Backlog
    {
        std::size_t largest = 0;                    // Bytes waiting to go to the client with most waiting.
        std::size_t slow = 0;                       // Clients with more than SlowBytes waiting.
        std::size_t dropped = 0;                    // Frames dropped for a client too far behind, ever.
        static constexpr std::size_t SlowBytes = 256 * 1024;
    };
    void healthWindow(const perf::Meter::Window& w, std::size_t clients, const Backlog& backlog);
    void healthSpike(const perf::Meter::Window::Slow& pass);
    static constexpr double SpikeMs = 100;
    // Where notes go (level: "info", "warning", "error"). Standard error until set.
    std::function<void(const char* level, const std::string& text)> log;

    void connect(Connection* c);                              // A new client: it is shown the lobby.
    void disconnect(Connection* c);                           // Its character leaves the world first.
    void command(Connection* c, const std::string& json);
    void acknowledge(Connection* c, double revision, bool missing);   // It applied that snapshot (or lacks a part).
    // A client walking its own wolf says where it is (link::Pose; the same as the "pose" command, without the JSON).
    void pose(Connection* c, std::uint32_t seq, double x, double y, double facing, double ix, double iy);
    void tick(double dt);                                     // 20 times a second (dt 0.05), times the speed.
    // Fast-forward: the server ticks `speed` times as often, each tick the same 0.05 s of the world as ever, so nothing
    // is skipped; the world just runs faster than real time (as fast as the machine can, if it can't keep up). What
    // belongs to real time keeps to it: frames and snapshots to the clients, saves, the database polls, the Dungeon
    // Master's director and the NPC Mind's ambient voices (so no more model calls an hour). 1 to MaxSpeed.
    static constexpr double MaxSpeed = 1000;
    void setSpeed(double speed);
    double speed() const { return speed_; }
    void save();                                              // Stored before returning.
    // Waits for the journal's records to be written and sends the replies waiting for them (tests and tools; the
    // tick does this as it goes, without waiting).
    void settle();
    // Held for a story (doc 54, 4): what a DM's estate.hold does, for tools and tests.
    Result holdForStory(const std::string& cell, double days, const std::string& reason) { return holdPlace(cell, days, reason); }
    // A character held still by the host (tests): no walking of any kind until released. A fighter in an arena, and
    // anyone Downed, are held so by the world's fights (doc 33) without it.
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
    // A character's social standing is its account's (doc 49): the social XP of all the account's characters together
    // (a development identity's wolf, with no account, its own), and the level that makes on standing.json's curve.
    long long socialXp(const std::string& characterId) const;
    int socialLevel(const std::string& characterId) const;
    SocialLedger& ledger() { return social_; }               // The social ledger (doc 08), for tests and tools.
    // Whether either wolf's account blocks the other's (doc 50, 7): the test docs 51-58 call before anything between two.
    bool blocked(const std::string& a, const std::string& b) const;
    // Whether two wolves' accounts are friends (doc 50, 4): doc 51 shows friends the exact star count.
    bool areFriends(const std::string& a, const std::string& b) const;
    // One character's entry on another, as it sees it (doc 50, 5: with all its recaps), or null: for tests and docs 52-56.
    json::Value knownFor(const std::string& owner, const std::string& other) const;
    // Newcomers (doc 52): whether an account is still new (15 hours played or social level 3 ends it, for good), and
    // the start towns this world has, the busiest first-character choice among them, and each one's recent mean.
    bool isNewcomer(const std::string& account) const;
    std::vector<std::string> startTowns() const;
    std::string suggestedStart() const;
    double startTownMean(const std::string& town) const { return townCounts_.mean(town); }
    // Mentors (doc 52, 3): whether an account may mentor (and why not), and a Dungeon Master turning it off or back on.
    bool mayMentor(const std::string& account, std::string& why) const;
    Result revokeMentor(const std::string& account, bool revoke, const std::string& by);
    // Ties (doc 52, 4): every tie by its id, for tests and tools; a Dungeon Master ending a character's tie.
    const std::map<std::string, newcomers::Tie>& ties() const { return ties_; }
    const std::map<std::string, newcomers::Vouch>& vouches() const { return vouches_; }
    Result endTieOf(const std::string& character, const std::string& by);
    // Story books (doc 51, Phase 7): every book, for tests and tools; a DM's tie of one to a world storyline.
    const std::map<std::string, books::Book>& storyBooks() const { return books_; }
    Result tieBookToStoryline(const std::string& book, const std::string& storyline, const std::string& by);
    // The star book (doc 51): every star by the receiving account, and what a viewer sees of a wolf's (exact for its
    // own player and a friend who sees which wolf is theirs, bands for everyone else).
    const stars::Book& starBook() const { return starBook_; }
    stars::Book& starBook() { return starBook_; }            // (For tests and tools, as ledger() is.)
    json::Value starsFor(const std::string& viewer, const std::string& target) const;
    // Every report kept (doc 50), by id: for tests and tools (the DM app reads game.reports itself).
    const std::map<std::string, reports::Report>& reportsKept() const { return reportCache_; }
    Result decideReport(const std::string& id, const std::string& decision, const std::string& outcome, int hours, const std::string& by);
    // Upheld reports against an account in the last `days` days (doc 50): docs 49 and 52 ask.
    int upheldReportsWithin(const std::string& account, int days) const;
    // What a resident is told of a player it answers (doc 50): only what it could perceive of their profile.
    std::string profileContext(const std::string& npc, const std::string& player) const;
    // An account's earned Gift tiers (doc 49, Phase 5): the DM's account.unlock ("grant" or "revoke" a tier, "hold" or
    // "release" new unlocks), and what is open and still needed.
    Result unlockTier(const std::string& account, const std::string& tier, const std::string& op, const std::string& by);
    const json::Value* tiersOf(const std::string& account) const
    {
        const auto it = tiersViews_.find(account);
        return it == tiersViews_.end() ? nullptr : &it->second;
    }
    // Factions in play (doc 32, Part 4): for tests and tools to define factions without a database.
    faction::Factions& factions() { return factions_; }
    chapter::Chapters& chapters() { return chapters_; }
    estate::Estates& estates() { return estates_; }
    camp::Camps& camps() { return camps_; }
    // A Dungeon Master's decision on a treaty or a House (Phase 9); without one within a game day, the faction's rule.
    Result decideTreaty(const std::string& id, bool approve, const std::string& by);
    Result decideHouse(const std::string& chapterId, const std::string& factionId, bool approve, const std::string& by);
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
    static constexpr double AutosaveSeconds = 15, SnapshotSeconds = 30, SaveSoonSeconds = 3;
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
    // Each client's last cell rows, heights and visibility, reused while what it sees and remembers is unchanged
    // (doc 31, Phase 4): building and keying them for a 256x256 city cell every snapshot was most of a view's cost.
    struct CellRows
    {
        std::string cell;
        const void* tiles = nullptr;
        std::vector<bool> visible, remembered;
        json::Value rows, visibility, heights;
    };
    std::map<const Connection*, CellRows> cellRows_;
    // The worker pool, and whether the views due this tick are being built on it (they then only read the world).
    std::unique_ptr<Pool> pool_;
    std::uint64_t frameTick_ = 0;
    double speed_ = 1, frameDebt_ = 0;              // (setSpeed: a frame to the clients each 1/speed of a tick.)
    bool batching_ = false;
    void finishSnapshot(Connection* c, json::Value root, double revision);
    void updateMovementModes();
    // Fights (RatwGameBattle.cpp): the arena as a fighter or watcher sees it, the red squares an onlooker sees, and
    // the "battle" commands (false: not one of them).
    json::Value battleView(const Battle& b, const std::string& viewer) const;
    json::Value fightsInView(const Entity& self) const;
    bool battleCommand(Connection* c, const json::Value& j, Result& result);
    // The Dev Console's commands, for a player marked Dungeon Master (RatwGameDev.cpp): answered with a "devResult".
    void devCommand(Connection* c, const json::Value& j);
    std::map<std::string, double> lingering_;      // Players gone mid-fight, their bodies kept until then (doc 33).
    void releaseLingering();
    // Parties (RatwGameParty.cpp; doc 32, Part 2): the "party" commands (false: not one of them), an invitation, the
    // party's out-of-character chat, the rules each tick (places kept, fights called into), and what a player sees of
    // their party and who is a party mate or hostile to them.
    party::Parties parties_;
    struct Pull
    {
        std::string battle;
        int side = 0;
        double at = 0;                  // World seconds.
        std::string mate;               // Whose fight it is.
    };
    std::map<std::string, Pull> pulls_;                                  // Who is being called into a fight.
    std::map<std::string, std::set<std::string>> stayedOut_;             // Who won't be called into which fights.
    std::map<std::string, std::map<std::string, double>> foes_;          // A player's recent foes, until when.
    double partyAccumulator_ = 0;
    struct Relations
    {
        std::set<std::string> mates, chapterMates;
        std::string colour;                                              // Their Chapter's.
        std::map<std::string, std::string> hostile;                      // Who, and why.
    };
    bool partyCommand(Connection* c, const json::Value& j, Result& result);
    Result partyInvite(const std::string& from, const std::string& to);
    void partyChat(Connection* c, const Entity& speaker, const std::string& text, const std::string& channel = "partyooc");
    void partyTick(double dt);
    Relations relationsFor(const std::string& viewer) const;
    json::Value partyView(const std::string& id) const;
    void tellParty(const std::string& partyId, const std::string& words, const std::string& except = {});
    void tellParty(const std::vector<std::string>& members, const std::string& words, const std::string& except = {});
    std::string nameOf(const std::string& id) const;
    // Names and introductions (RatwGameNames.cpp; doc 32, 1.5): who knows whom by what name, players' aliases, what a
    // viewer calls a wolf (its name, or how it looks), introductions noticed in what was heard, residents giving their
    // names, and the game's own messages veiled for a viewer.
    names::Acquaintances known_;
    std::map<std::string, std::vector<std::string>> aliases_;
    std::map<std::string, std::vector<std::string>> introducedTo_;      // By whom, as one line is heard: for the receipt.
    std::set<std::string> owedName_;                                     // "npc|player": a friendly resident told a name.
    static constexpr double FamiliarEnough = 50;                         // Familiarity at which a resident offers a name.
    std::vector<std::string> namesOf(const std::string& id) const;
    bool knowsName(const std::string& viewer, const std::string& id) const;
    std::string strangerLabel(const std::string& id) const;
    std::string lookOf(const std::string& id, const std::map<std::string, int>* trades) const;
    void refreshLabels(double dt);
    std::map<std::string, std::string> veilMap(const std::string& viewer) const;
    // What this wolf calls each stranger round it (its cell, and any fight it is in or watches): how they look, and
    // where two look alike a number, in a fixed order ("a dun wolf (2)"). The In Sight list, the fight's log and
    // the red squares all use it, so the same wolf is called the same everywhere.
    std::map<std::string, std::string> strangerNames(const std::string& viewer) const;
    std::map<std::string, std::string> labels_;                          // How each wolf looks to a stranger.
    std::map<std::string, std::map<std::string, std::string>> strangers_; // Each player's strangerNames, a second old.
    std::map<std::string, std::string> nameStrangers(const std::string& viewer) const;
    double labelsAccumulator_ = 1;
    std::string labelFor(const std::string& viewer, const std::string& id) const;
    // A good's name as this wolf sees it (doc 35, Part 4): with a masterwork's maker's mark, named if it knows them, else
    // known only by the scent on it.
    std::string itemLabel(const std::string& viewer, const std::string& item) const;
    std::string veilFor(const std::string& viewer, const std::string& text) const;
    bool willName(const std::string& npcId, const std::string& playerId) const;
    bool learnName(const std::string& knower, const std::string& known, const std::string& name, const std::string& how);
    void noticeIntroduction(const std::string& author, const std::string& listener, const std::string& heard);
    void sendIntroductionReceipt(const std::string& author);
    void npcSpokeTo(const std::string& npcId, const std::string& playerId);
    bool namesCommand(Connection* c, const json::Value& j, Result& result);
    void seedAcquaintances();
    json::Value namesView(const std::string& id) const;
    // Residents travelling with a party (RatwGameCompanions.cpp; doc 32, 2.3): who may come and why, orders, following,
    // wages, leaving, fighting beside the party, growing closer, and a few words of their own.
    double companionAccumulator_ = 0, companionHours_ = 0;
    std::set<std::string> seenIncidents_, warnedWatch_;
    std::map<std::string, std::set<std::string>> sharedDanger_;       // Resident → party players they fought beside.
    struct Moments
    {
        std::string cell;
        bool trouble = false;
        std::set<std::string> hurt;
    };
    std::map<std::string, Moments> companionMoments_;
    std::map<std::string, double> remarkAt_, lastPartySpeech_;        // By party: next remark allowed; last words said.
    static constexpr double RemarkSeconds = 180, QuietSeconds = 600;
    std::string whyNotJoin(const std::string& npcId) const;
    std::int64_t wageFor(const std::string& npcId) const;
    void companionSays(const std::string& npcId, const std::string& words);
    Result askAlong(const std::string& playerId, const std::string& npcId, bool hire);
    Result orderCompanion(const std::string& playerId, const std::string& npcId, const std::string& order);
    void releaseCompanions();
    void companionLeaves(const std::string& npcId, const std::string& words, const std::string& why);
    void companionTick(double dt);
    void companionRemarks(double t);
    std::string companionContext(const std::string& npcId) const;
    void adoptOldCompanions(const std::map<std::string, std::string>& owners);
    // The individual social game (RatwGameSocial.cpp; doc 32, Part 1): scenes as players see them, Gold Stars and
    // Stories, titles, regard in words, and a name about town. (Private notes are in Known wolves now: doc 50.)
    std::size_t socialSeen_ = 0;
    bool socialViewsDirty_ = true;
    double socialViewsAccumulator_ = 0;
    std::map<std::string, json::Value> socialViews_;                 // By player: worked out on the game thread.
    std::string regardWords(const std::string& holder, const std::string& other) const;
    std::vector<std::string> reputationLines(const std::string& playerId) const;
    void afterSocial();
    void tendFightScenes();                       // Each fight a scene: its players in it; settled when it ends.
    void refreshSocialViews(double dt);
    bool socialCommand(Connection* c, const json::Value& j, Result& result);
    // Known wolves and scene recaps (doc 50, Phase 4; RatwGameKnown.cpp): each character's list of the wolves it has
    // met, by their id, and its recaps; what each player perceived lately (for recaps; in memory only, bounded); the
    // scenes whose ending has been seen, and members seen stepping out; when each pair last met (a throttle); and each
    // character's model recaps today.
    std::map<std::string, std::map<std::string, people::KnownWolf>> knownWolves_;
    std::map<std::string, std::vector<people::Recap>> recaps_;
    struct PerceivedLine
    {
        double at = 0;
        std::string cell, who, text;
    };
    struct Perceived
    {
        std::deque<PerceivedLine> lines;
        std::size_t characters = 0;
    };
    std::map<std::string, Perceived> perceived_;
    std::set<std::string> scenesDone_, scenesLeft_;
    std::map<std::string, double> metAt_;
    std::map<std::string, std::pair<std::int64_t, int>> modelRecaps_;
    // A scene's end card (doc 51, §8): what each wolf noticed for itself when its last scene ended (a first scene
    // with someone, how someone regards it now), by its id and that scene's; the regard it last saw, to tell a change;
    // and the moments in words for a viewer.
    std::map<std::string, std::pair<std::string, std::vector<std::string>>> endedNotes_;
    std::map<std::string, std::string> regardSeen_;
    json::Value sceneMoments(const std::string& viewer, const SocialSession& scene) const;
    void meet(const std::string& owner, const std::string& other, const std::string& how);
    void perceivedLine(const std::string& listener, const std::string& who, const std::string& text);
    void tendScenes();
    void seedScenes();
    void endScene(const std::string& member, const SocialSession& s, double end);
    void keepRecap(const std::string& member, people::Recap recap);
    std::string knownName(const std::string& owner, const std::string& other) const;
    json::Value knownView(const std::string& owner, const std::string& other, const people::KnownWolf& k, bool allRecaps) const;
    void sendKnown(Connection* c, const std::string& only = {});
    bool knownCommand(Connection* c, const json::Value& j, Result& result);
    void readProfile(const std::string& owner, const std::string& other);
    void knownSave(json::Value& root) const;
    void knownLoad(const json::Value& saved);
    // Earned Gift tiers (doc 49, Phase 5), by account: what is kept (when each tier opened, how, the DM's hold), what
    // was last measured, and the tiers' view (open or what they still need) for the lobby and the sheet.
    std::map<std::string, standing::Record> standing_;
    std::map<std::string, standing::Measures> measures_;
    std::map<std::string, json::Value> tiersViews_;
    standing::Measures measuresOf(const std::string& account) const;
    void checkUnlocks(const std::string& account);
    json::Value tiersView(const std::string& account) const;
    int upheldReports(const std::string& account) const;     // Doc 50's reports, when they exist: none until then.
    json::Value standingSave() const;
    void standingLoad(const json::Value& saved);
    // People (doc 50, RatwGamePeople.cpp): each account as a person (handle, experience, played time, settings) by
    // account ("dev:<id>" for a development identity), and each character's roleplay profile.
    std::map<std::string, people::AccountRecord> people_;
    std::map<std::string, people::Profile> profiles_;
    double peopleAccumulator_ = 0;
    std::string accountKey(const std::string& characterId) const;
    std::string accountKey(const Connection* c) const;
    people::AccountRecord& personOf(const std::string& account);
    bool profileCommand(Connection* c, const json::Value& j, Result& result);
    Result setHandle(const std::string& account, const std::string& username, const std::string& handle);
    void sendProfile(Connection* c);
    json::Value accountView(const std::string& account) const;
    people::Viewer viewerFacts(const std::string& viewer, const std::string& target) const;
    json::Value cardFor(const std::string& viewer, const std::string& target) const;
    std::string pronounsOf(const std::string& id) const;
    void tendPeople(double dt);
    json::Value peopleSave() const;
    void peopleLoad(const json::Value& saved);
    // Mute, block and report (doc 50, Phase 2; RatwGameSafety.cpp). Marks by the holder's account: a mute aims at a
    // character, a block at an account (found from the character pointed at, whose label is kept for the list).
    struct SafetyMark
    {
        std::string kind, target, character, label;
        double at = 0;
    };
    std::map<std::string, std::vector<SafetyMark>> safety_;
    // The lines each connected player received lately (the last 60, within 30 minutes): a report's evidence. Never saved.
    struct HeardLine
    {
        std::uint64_t seq = 0;
        double at = 0;
        std::string author, channel, text;
    };
    std::map<std::string, std::vector<HeardLine>> heard_;
    std::unique_ptr<reports::Store> reports_;
    std::map<std::string, reports::Report> reportCache_;   // Every report kept, by id (read from the store at start).
    double reportsPurgedAt_ = 0;
    bool hides(const std::string& listener, const std::string& author) const;   // Muted, or blocked, by the listener.
    void heardLine(const std::string& listener, std::uint64_t seq, const std::string& author, const std::string& channel,
                   const std::string& text);
    bool safetyCommand(Connection* c, const json::Value& j, Result& result);
    void sendSafety(Connection* c);
    bool silenced(const std::string& characterId, std::string* until = nullptr) const;
    bool blockedAccounts(const std::string& one, const std::string& two) const;
    // Friends and private messages (doc 50, Phase 3; RatwGameFriends.cpp). Each account's side of each friendship, by
    // the friend's account (when it began, whether this side shares its character); requests waiting; an offline
    // friend's private messages, by the recipient's account; and each account's connection while it has a wolf here.
    std::map<std::string, std::map<std::string, people::FriendLink>> friends_;
    std::vector<people::FriendRequest> friendRequests_;
    std::map<std::string, std::vector<people::PrivateMessage>> inbox_;
    std::map<std::string, Connection*> online_;
    double friendsTendedAt_ = 0;
    Connection* onlineClient(const std::string& account) const;
    std::string handleOf(const std::string& account) const;
    std::string accountByHandle(const std::string& handle) const;
    std::string sharedHandle(const std::string& viewerAccount, const std::string& target) const;
    void befriend(const std::string& one, const std::string& two);
    void unfriend(const std::string& one, const std::string& two);
    bool friendsCommand(Connection* c, const json::Value& j, Result& result);
    void sendFriends(const std::string& account, const std::string& toast = {});
    void cameOrWent(Connection* c, bool here);
    Result privateMessage(Connection* c, const std::string& to, const std::string& text);
    void deliverInbox(Connection* c);
    void tendFriends();
    void friendsSave(json::Value& root) const;
    void friendsLoad(const json::Value& saved);
    // Circles (doc 50, Phase 5; RatwGameCircles.cpp): out-of-character groups of accounts, by id.
    std::map<std::string, people::Circle> circles_;
    stars::Book starBook_;                        // (Doc 51, Phase 1; saved with the people.)
    // The gathering howl (doc 51, Phase 6; RatwGameHowl.cpp): choruses still open (where, when they close, who howled,
    // who has heard), when each wolf last howled (world time; not saved), the game day each pair last grew closer for
    // a chorus, and each town's latest howl, remembered a while for its residents to tell of.
    struct Chorus
    {
        std::string id, cell;
        double wx = 0, wy = 0, wz = 0, started = 0, closes = 0;
        std::vector<std::string> howlers;
        bool indoors = false;
        std::set<std::string> heard;
    };
    std::vector<Chorus> choruses_;
    std::map<std::string, double> howledAt_, chorusBondDay_;
    std::map<std::string, std::pair<double, std::string>> recentHowls_;
    Result howl(const std::string& id);
    double howlCooldownLeft(const std::string& id) const;
    void sendHowl(Chorus& chorus);
    void residentsHear(const Chorus& chorus);
    void tendHowls();
    void tendWorkScenes();
    // Newcomers (doc 52; RatwGameNewcomers.cpp): active wolves counted in each start town once a minute (in memory
    // only), and accounts that have stopped being new marked so.
    newcomers::Counts townCounts_;
    double newcomersAccumulator_ = 0;
    void tendNewcomers(double dt);
    json::Value startsView() const;
    double mentorsCheckedAt_ = 0;
    int accountSocialLevel(const std::string& account) const;
    bool mentorCommand(Connection* c, const json::Value& j, Result& result);
    void mentorOff(const std::string& account, const std::string& why);
    void tellAccount(const std::string& account, const std::string& words);
    json::Value mentorView(const std::string& account) const;
    bool availableMentor(const std::string& account) const;
    // Ties (doc 52, 4; RatwGameNewcomers.cpp): every tie by its id; each character's open tie (seeking, offered or
    // active) as the newcomer, and the tie each mentor or resident holds; offers asked once a second, lapses once a
    // minute. (A mentor may still have its own newcomer's tie while it holds one for someone else.)
    std::map<std::string, newcomers::Tie> ties_;
    std::map<std::string, std::string> tieOfCharacter_, heldTieOf_;
    double tiesAccumulator_ = 0, tiesLapsedAt_ = 0;
    void startTie(const std::string& newcomer, const std::string& account, const std::string& starter, const std::string& town,
                  const std::string& arrivalCell);
    void tendTies(double dt);
    bool offerTie(newcomers::Tie& t);
    void tieToResident(newcomers::Tie& t);
    void makeTie(newcomers::Tie& t, const std::string& other, const std::string& otherAccount, bool resident);
    void tellTie(newcomers::Tie& t);
    void closeTie(newcomers::Tie& t, const std::string& state, bool rest);
    void tieKnown(const std::string& owner, const std::string& other, const std::string& line);
    void reindexTies();
    int tieScenes(const newcomers::Tie& t) const;
    std::string lookOfCharacter(const std::string& viewer, const std::string& id) const;
    json::Value tieView(const std::string& character) const;
    std::string tieBriefing(const std::string& npc, const std::string& player) const;
    bool tieCommand(Connection* c, const json::Value& j, Result& result);
    void tiesOnEnter(const std::string& character);
    void tiesSave(json::Value& root) const;
    void tiesLoad(const json::Value& saved);
    // Residents as matchmakers (doc 52, 5; RatwGameNewcomers.cpp): each town's active players (the minute's count), a
    // pointer waiting to be said by each matchmaker, and the limits (by game hour).
    struct PendingMatch
    {
        std::string player, other, who, where, reason;
    };
    std::map<std::string, std::vector<std::string>> townRoster_;
    std::map<std::string, PendingMatch> matchPending_;
    std::map<std::string, std::int64_t> matchedHour_;
    std::map<std::string, std::pair<std::int64_t, int>> matchmakerHour_, pointedHour_;
    std::string jobCategoryOf(const std::string& npc) const;
    bool isMatchmaker(const std::string& npc) const;
    std::string matchmake(const std::string& npc, const std::string& player, const std::string& heard);
    std::string sayMatch(const std::string& npc, const std::string& text, bool modelSaid);
    // A newcomer's first evenings at an inn (doc 52, 6): each character's evenings tried, the last, and done; checked
    // every five seconds. Vouches (doc 52, 7) by id, and how far through the crime incidents they have been followed.
    struct Evening
    {
        int tries = 0;
        double lastDay = -1;
        bool done = false;
    };
    std::map<std::string, Evening> evenings_;
    double eveningsAt_ = 0;
    std::map<std::string, newcomers::Vouch> vouches_;
    std::int64_t vouchCursor_ = 0;
    void tendEvenings();
    std::map<std::string, double> welcomedAt_;
    void welcomeAtInn(const std::vector<std::string>& newcomers, const std::string& innkeeper, const std::vector<std::string>& others);
    bool vouchCommand(Connection* c, const json::Value& j, Result& result);
    void tendVouches();
    void breakVouch(newcomers::Vouch& v);
    std::string vouchBriefing(const std::string& npc, const std::string& player);
    void vouchesSave(json::Value& root) const;
    void vouchesLoad(const json::Value& saved);
    // Letters (doc 55, RatwGameLetters.cpp): the document store; writing at an inn or a scriptorium (or a Chapter's own
    // place) to a wolf known by name, the courier's fee to the town's treasury and its hours; delivered to the reader's
    // post town, handed over there or waiting at its inns; read with the writer's scent; kept until burnt.
    documents::Store documents_;
    std::set<std::string> innCells_, scriptoriumCells_;
    double letterPlacesAt_ = -1;
    std::map<std::string, double> lastFullRest_;    // (A full rest sets one's post town.)
    double lettersAccumulator_ = 0, staleCheckedHour_ = -1;
    bool letterCommand(Connection* c, const json::Value& j, Result& result);
    void sendLetters(Connection* c);
    json::Value lettersSelf(const std::string& id) const;
    void tendLetters(double dt);
    void deliverLetter(documents::Document& d, bool atInn);
    void refreshLetterPlaces();
    bool writingPlace(const std::string& id) const;
    bool atInn(const std::string& id) const;
    std::string townFor(const std::string& cellId) const;
    std::string townWords(const std::string& town) const;
    std::string postTownOf(const std::string& id) const;
    int courierCells(const std::string& fromTown, const std::string& toTown) const;
    std::string scentLine(const std::string& reader, const documents::Document& d) const;
    std::string knownByName(const std::string& writer, const std::string& typed, std::string& problem) const;
    documents::Document* sendLetter(Connection* c, const std::string& toId, std::string text, const std::string& sign, Result& result,
                                    const std::string& replyTo = {}, bool viaCourier = false, const json::Value& enclose = {});
    bool takeEnclosure(documents::Document& d, const std::string& reader, std::string& said);
    void returnEnclosure(documents::Document& d, const std::string& why);
    void orphanedEnclosures();
    // Giving (doc 55, 3; RatwGameGive.cpp): face to face, within 2 tiles, out of a fight; a player accepts within 30 s,
    // a resident takes it unless it dislikes the giver. Conserved (Society::shift, kind "a gift"); a gift warms the
    // receiver (the `gift` event) once a game day a pair. Players' goods carry who made and gave them (Entity::scents).
    struct GiveOffer
    {
        std::string from, item;
        int quantity = 0;
        std::int64_t coins = 0;
        double until = 0;
    };
    std::map<std::string, GiveOffer> giveOffers_;   // By the one offered.
    std::map<std::string, double> giftBondDay_;     // "giver|receiver" -> the game day a gift last warmed.
    bool giveCommand(Connection* c, const json::Value& j, Result& result);
    bool giveAnswer(Connection* c, bool accept, Result& result);
    void tendGives();
    int spareOf(const std::string& who, const std::string& item) const;   // Held and not worn (nor in the jaws).
    std::string giveRefusal(const std::string& giver, const std::string& target, const std::string& item, int quantity, std::int64_t coins) const;
    void gifted(const std::string& giver, const std::string& receiver, const std::string& item, int quantity, std::int64_t coins);
    std::string goodsWords(const std::string& item, int quantity, std::int64_t coins) const;
    // Scent records: moved with goods between players (a giver's own added unless masked), dropped when goods go to a
    // resident or a till; read newest first, within what is held.
    void moveScents(const std::string& from, const std::string& to, const std::string& item, int quantity, bool giverScent);
    void addScent(const std::string& who, const std::string& item, int quantity, const std::string& maker, const std::string& giver);
    std::string scentOfItem(const std::string& viewer, const std::string& owner, const std::string& item, int held = -1) const;
    // The maker's scent (doc 55, 4): a good bought from a shop whose business makes it carries its keeper's scent.
    void makersScent(const std::string& buyer, const std::string& seller, const std::string& item, int quantity);
    // Grooming (doc 55, 7; RatwGameFavours.cpp): asked of a wolf within 1.5 tiles, both still and out of a fight; a
    // player accepts within 30 s, a resident only from a player it likes; fifteen seconds, staying close; once a game
    // day for the groomer (and once for oneself); its line posted as the groomer's action. World::applyGrooming does
    // the rest.
    struct GroomOffer
    {
        std::string from, words;
        double until = 0;
    };
    struct Grooming
    {
        std::string groomer, groomed, words;
        double started = 0;
    };
    std::map<std::string, GroomOffer> groomOffers_;   // By the one asked.
    std::vector<Grooming> groomings_;
    bool groomCommand(Connection* c, const json::Value& j, Result& result);
    bool groomAnswer(Connection* c, bool accept, Result& result);
    void tendGrooming();
    std::string groomRefusal(const std::string& groomer, const std::string& target) const;
    void startGrooming(const std::string& groomer, const std::string& groomed, const std::string& words);
    // Lending gear (doc 55, 8; RatwGameFavours.cpp): an item for 1 to 7 game days to a player who accepts; worn and used,
    // never sold, given or put in a letter; returned within 2 tiles, or carried back by courier at the due day (1p from
    // the borrower); gone, it becomes a debt (Bonds::addOwed) and the lender trusts the borrower less.
    struct Loan
    {
        std::string id, lender, borrower, item;
        int quantity = 0;
        double due = 0;                             // Calendar day.
    };
    struct LendOffer
    {
        std::string from, item;
        int quantity = 0, days = 1;
        double until = 0;
    };
    std::vector<Loan> loans_;
    std::map<std::string, LendOffer> lendOffers_;  // By the one offered.
    std::uint64_t nextLoan_ = 1;
    double loansHour_ = -1;
    bool lendCommand(Connection* c, const json::Value& j, Result& result);
    bool lendAnswer(Connection* c, bool accept, Result& result);
    bool returnLoan(Connection* c, const json::Value& j, Result& result);
    void tendLoans();
    int lentTo(const std::string& borrower, const std::string& item) const;
    void loansSave(json::Value& root) const;
    void loansLoad(const json::Value& saved);
    // Residents' letters (doc 55, 5; RatwResidentLetters.cpp): thanks the day after a deed for a fond resident (one time
    // in three with a few pennies from its own purse), courier work offered first to a player it trusts; at most 3 a
    // game week to a player and one from any resident; templates (Data/Voice/letters.json), polished only when
    // Options::letterModelCallsPerHour allows; the resident's briefing remembers them.
    struct ThanksDue
    {
        std::string resident, player, deed;
        double day = 0;
    };
    struct ResidentLetterSent
    {
        std::string resident, player;
        double day = 0;
    };
    std::vector<ThanksDue> thanksDue_;
    // Who stood witness at a resident's occasion (doc 55, 6): host -> (player, kind, day), for its briefing.
    struct Witnessed
    {
        std::string player, kind;
        double day = 0;
    };
    std::map<std::string, std::vector<Witnessed>> witnessedBy_;
    void inviteToOccasions();
    std::vector<ResidentLetterSent> residentLettersSent_;
    double residentLettersHour_ = -1, letterCallsHour_ = -1;
    int letterCalls_ = 0;
    void watchEvent(const WorldEvent& e);
    void tendResidentLetters();
    bool residentLetterRoom(const std::string& resident, const std::string& player) const;
    documents::Document* residentLetter(const std::string& resident, const std::string& player, const std::string& kind, const std::string& text,
                                        const std::string& facts, std::int64_t coins, const std::string& contract);
    std::string residentLetterText(const std::string& kind, const std::string& resident, const std::string& player,
                                   const std::map<std::string, std::string>& blanks) const;
    std::string residentLetterBriefing(const std::string& npc, const std::string& player) const;
    // A letter carried by a friend (doc 55, 8): a courier contract given to a player who knows the recipient, paid the
    // fee in place of the town; and pacts: terms one wolf writes naming another, sealed by both and up to 3 witnesses,
    // a copy each in their letter cases. Nothing enforces a pact.
    std::set<std::string> carried_;                 // Letters in a friend's keeping, by id.
    std::uint64_t nextPact_ = 1;
    bool pactCommand(Connection* c, const json::Value& j, Result& result);
    // Taverns (doc 54, 1; RatwTaverns.cpp): common rooms (the inns' cells) with their company counted every 5 s for the
    // world's rest; performing (sing, a tale, an instrument carried) while the performer keeps at it every 2 minutes, up
    // to 30, then 10 to rest; one performer a room. A full rest needs a bed the wolf has a right to (World::setBedRight).
    struct Performance
    {
        std::string cell, kind;
        double started = 0, lastSaid = 0;
    };
    std::map<std::string, Performance> performers_;
    std::map<std::string, double> performRestUntil_, lastActiveReal_;
    double tavernsAccumulator_ = 0;
    bool performCommand(Connection* c, const json::Value& j, Result& result);
    void tendTaverns(double dt);
    bool hasBedRight(const std::string& id, const std::string& cell) const;
    // Renting by individuals (doc 54, 4; RatwLodgings.cpp): a bed upstairs at an inn (a night to noon, or a week, rent
    // to the inn's till); a lodger's spare bed in a resident's home (a week, to the head's purse); the inn's whole
    // upstairs for a night (to 06:00); a place listed for individuals (a night or a week). Each with a small chest
    // (`let:<id>`). Held for a story, new lodgings are refused and running ones get a week's notice, the rest refunded
    // or owed. A renter of a whole place may open its doors for a night.
    struct Lodging
    {
        std::string id, holder, cell, kind, landlord, account, period, town;
        int x = -1, y = -1;                         // The bed (a bed or a lodger's); -1 for a whole place.
        std::int64_t rent = 0;
        double paidTo = 0, noticeUntil = -1;
        bool open = false;
        std::set<std::string> guests;
    };
    struct Hold
    {
        double until = 0;
        std::string reason;
    };
    std::vector<Lodging> lodgings_;
    std::map<std::string, Hold> holds_;             // By cell.
    std::uint64_t nextLodging_ = 1;
    bool lodgeCommand(Connection* c, const json::Value& j, Result& result);
    json::Value lodgingView(const std::string& viewer);
    void tendLodgings();
    void endLodging(std::size_t index, const std::string& why, std::int64_t refund);
    const Lodging* lodgingOf(const std::string& holder) const;
    const Lodging* wholeLodgingAt(const std::string& cell) const;
    const estate::Property* innUpstairs(const std::string& cell) const;
    Result holdPlace(const std::string& cell, double days, const std::string& reason);
    void lodgingsSave(json::Value& root) const;
    void lodgingsLoad(const json::Value& saved);
    // Fame (doc 56; RatwGameFame.cpp): good deeds recorded from the world's events (a camp broken, a wolf tended, a
    // promise kept, a letter carried, a caravan escorted, a thief reported and caught, a festival won) and the DM's
    // awards, with their witnesses and the names each knew the doers by. Witnesses, the one it was done for and its
    // household believe it ("deed:<id>"); a notable deed warms each resident once, the first time it ties it to the doer.
    fame::Ledger fame_;
    std::map<std::string, std::vector<std::string>> reportedBy_;   // A crime's incident -> the wolves who told the watch.
    double fameDay_ = -1;
  public:
    // A deed of `kind` (Data/Fame/deeds.json) by these player characters, for `beneficiary` (a resident, "town:<id>" or
    // ""), where `cell` is; `weight` -1 for the kind's own. Its id, or "" if none was made (a kind capped, no doers).
    std::string recordDeed(const std::string& kind, const std::vector<std::string>& doers, const std::string& beneficiary,
                           const std::string& cell, const std::string& source, const std::string& detail = {}, int weight = -1);
    bool revokeDeed(const std::string& id);
    const fame::Ledger& deeds() const { return fame_; }
    // How a resident would speak of a wolf's best-known deed ("the one who broke the camp on the east road"), by name or
    // look, or "" (doc 56, 5: for introductions).
    std::string fameLine(const std::string& knower, const std::string& wolf);
    // What a resident knows of a wolf's deeds, for the Mind (doc 56, 5): at most 400 letters, "" if nothing.
    std::string fameBriefing(const std::string& npc, const std::string& wolf, bool note = true);
    // Nicknames (doc 56, 4): what `npc` calls `wolf` (from the heaviest, newest deed it can tie to them; "" none), and
    // whether it coined it; a wolf asking folk not to use one (true if it was theirs and in use).
    std::string nicknameFor(const std::string& npc, const std::string& wolf, bool* coined = nullptr);
    bool dropNickname(const std::string& wolf, const std::string& id);
    std::string awayBriefingFor(const std::string& npc, const std::string& wolf) { return awayBriefing(npc, wolf, false); }
    // A resident's trouble today (doc 57, 3: worked out now if not today), what its Mind would be told of it for `wolf`
    // ("" short of the trust rule), and the troubles a wolf has heard of (resident -> kind).
    troubles::Trouble troubleNow(const std::string& resident) { return troubleOf(resident); }
    std::string troubleBriefingFor(const std::string& npc, const std::string& wolf) { return troubleBriefing(npc, wolf); }
    std::map<std::string, std::string> troublesHeardBy(const std::string& wolf) const
    {
        std::map<std::string, std::string> out;
        if (const auto heard = troublesHeard_.find(wolf); heard != troublesHeard_.end())
            for (const auto& [npc, h] : heard->second)
                out[npc] = h.kind;
        return out;
    }
    void forgetTroubles() { troubles_.clear(); }    // (Tests: worked out afresh.)
    // Town projects (doc 57, 4; RatwGameProjects.cpp): posted by a town's needs or a Dungeon Master; the DM's hand
    // (cancel with refunds, complete by hand, remove the structure). Each says why not, or ok with the project's id.
    Result postProject(const std::string& kind, const std::string& town, const std::string& cell, int x, int y,
                       const std::string& title, const std::string& by);
    Result cancelProject(const std::string& id, const std::string& why);
    Result completeProject(const std::string& id);
    Result removeProject(const std::string& id);
    const projects::Ledger& projects() const { return projects_; }
    void hearTroubleFor(const std::string& wolf, const std::string& npc)   // (Tests: as if the resident had told them.)
    {
        if (const auto& t = troubleOf(npc))
            heardTrouble(wolf, npc, t.kind);
    }

  private:
    // Doc 56, Phase 2: deeds travel and are recognised.
    struct Recognition
    {
        const fame::Deed* deed = nullptr;
        bool byName = false;
        std::string how;                            // "you saw it", "the town's talk", "Wren told you"...
        double sure = 0;
    };
    std::vector<Recognition> recognise(const std::string& npc, const std::string& wolf, bool warm);
    bool townHeard(const std::string& npc, const fame::Deed& d) const;
    void spreadDeed(fame::Deed& d);
    void fameJoin(const std::string& listener, const std::string& wolf);
    World::DeedWords fameWords(const std::string& teller, const std::string& claim, const std::string& subject);
    std::map<std::string, double> fameMentioned_;   // "npc|deed": the day it spoke of it.
    std::map<std::string, std::string> fameRealised_;   // "npc|wolf": a deed it has just joined to the wolf's name.
    std::map<std::string, double> fameGreeted_;     // "npc|wolf": when its game greeting last named a deed.
    std::map<std::string, double> escortedTo_;      // Town: when a player's escorted caravan last arrived (world seconds).
    void tryNickname(const std::string& deedId);
    bool coinNickname(fame::Deed& d, const std::string& coiner);
    json::Value nicknamesView(const std::string& wolf);
    void fameFromEvent(const WorldEvent& e);
    void tendFame();
    std::string deedPhrase(const std::string& viewer, const fame::Deed& d) const;
    void fameSave(json::Value& root) const;
    void fameLoad(const json::Value& saved);
    // The library and the archive (doc 54, 7; RatwArchive.cpp): work where a keeper of records is at its post (posts from
    // Data/Lore/archives.json): sorting six records by their clues (the server keeps the order and checks it; three
    // tries), or copying at a desk for five minutes, sitting. 2p a task from the town, 4 a game day; each finished task
    // shows the next lore fragment of the archive's town (Data/Lore/fragments.json), else a shared one. The journal:
    // lore, bestiary, herbarium and places; a wolf who has read a town's records is known to its residents as a scholar.
    struct ArchiveTask
    {
        std::string kind, cell, rule;
        std::vector<std::pair<std::string, std::string>> records;   // (id, words), as shown: shuffled.
        std::vector<std::string> answer;             // The ids in order: never sent.
        double begun = 0;                            // World seconds.
        int tries = 0;
    };
    std::map<std::string, ArchiveTask> archiveTasks_;   // By character.
    std::map<std::string, int> archiveToday_;        // "who|day": tasks done today.
    std::map<std::string, std::vector<std::string>> archiveKeepers_;   // By cell: residents holding a records post there.
    double archivesAt_ = -1;
    bool archiveCommand(Connection* c, const json::Value& j, Result& result);
    void tendArchive();
    void refreshArchives();
    bool archiveKeeperHere(const std::string& cell);
    void sendArchiveTask(Connection* c, const ArchiveTask& t);
    void finishArchiveTask(Connection* c, const ArchiveTask& t, std::int64_t pay, const std::string& how);
    void sendJournal(Connection* c);
    std::string scholarBriefing(const std::string& npc, const std::string& player);
    // Festivals that draw players (doc 54, 6; RatwFestivals.cpp): on a town's festival day, the feast from noon (a meal
    // from the town's store, once), rested time for each hour at the square, and the programme's contests: races at 1,
    // tug-of-war at 2, howling at 3, the sparring tourney at 4, the hunting contest from noon to 5, storytelling at 7.
    // Entry 1p into the contest's pot (`fest:<town>:<day>:<contest>`), two or three residents entering too, and the
    // town adding to the pot from its own purse (the user, 2026-10-08); two thirds to the winner, a third to the second.
    struct Contest
    {
        std::string kind;
        std::vector<std::string> entrants;
        std::map<std::string, std::int64_t> paid;   // Entry pennies, by who paid ("" the town's share).
        bool begun = false, done = false;
        double begunAt = 0, turnAt = 0, marker = 0; // World seconds; a tug's marker (tiles toward team 0).
        std::map<std::string, double> score;        // What ranks them: a race's time, a howl's carry, a kill's worth.
        std::map<std::string, int> mark, team;      // A racer's next mark; a hauler's team (0 or 1).
        std::map<std::string, long> pulledBeat;     // A hauler's last pull on the beat.
        std::map<std::string, std::string> cheered; // Howling: who each cheerer cheered.
        std::map<std::string, std::set<std::string>> givers;   // Storytelling: each teller's star givers.
        std::vector<std::string> bracket, through; // The tourney's wolves in this round, and those through it.
        std::size_t bout = 0;                       // The bout under way (or next) in the round.
        double boutAt = -1;                         // When it began (world seconds); -1 not begun.
        std::string lastLoser;
        std::vector<Spot> marks;                    // The race's marks.
        std::size_t turn = 0;
        long beat = 0;
        std::vector<std::string> winners;           // First, then second.
        std::string result;
    };
    struct Fair
    {
        std::string community, name;
        std::int64_t day = 0;
        std::map<std::string, Contest> contests;
        std::set<std::string> fed, rested;          // Fed at the feast; "who|hour" given rested time.
    };
    std::map<std::string, Fair> fairs_;             // "community|day".
    double fairsAccumulator_ = 0;
    bool festivalCommand(Connection* c, const json::Value& j, Result& result);
    void tendFestivals(double dt);
    json::Value festivalSelf(const std::string& viewer);
    json::Value festivalBoard(const std::string& community);
    std::string festivalOn(const std::string& community, std::int64_t day) const;
    Fair* fairToday(const std::string& community);
    bool atSquare(const Entity& e, const std::string& community);
    void beginContest(Fair& f, Contest& c);
    void runContest(Fair& f, Contest& c);
    void endContest(Fair& f, Contest& c, const std::vector<std::string>& ranked, const std::vector<std::string>& losers = {});
    void festivalStar(const std::string& giver, const std::string& recipient);
    void festivalSay(const std::string& community, const std::vector<std::string>& also, const std::function<std::string(const std::string&)>& line);
    std::string festivalBriefing(const std::string& npc, const std::string& player);
    // Festival criers (doc 56, 7; RatwGameFame.cpp): the town's crier sent to the square to call the season's deeds and
    // nicknames, a line a minute where a player can hear; a legendary deed called in every town at noon the next day.
    struct CrierCall
    {
        std::string community, crier, cell;
        std::vector<std::string> lines;
        std::size_t next = 0;
        double nextAt = 0;
    };
    // The chronicle (doc 56, 8): asked for by the social verb `chronicle` (once in 30 s a character), read from
    // game.events on its own thread where there is a database, else from what is in memory (and said to be partial).
    chronicle::Reader chronicleReader_;
    std::map<std::string, double> chronicleAsked_;  // Character -> when it last asked (real seconds).
    bool chronicleCommand(Connection* c, Result& result);
    void tendChronicle();
    void sendChronicle(const std::string& owner, std::vector<chronicle::Row> rows, bool partial);
    // Welcome back and unfinished business (doc 56, 9-10; RatwGameFame.cpp).
    struct Absence
    {
        double realDays = 0, leftDay = 0, leftAt = 0;   // How long, the calendar day they left, and when (Unix).
        std::map<std::string, double> unseen;       // Residents who last saw them before the break, and when (until they speak).
    };
    std::map<std::string, Absence> absences_;       // Characters back from a break, this session.
    std::map<std::string, std::pair<double, json::Value>> unfinished_;   // Character -> (built at, the list).
    json::Value unfinishedView(const std::string& id);
    std::string awayBriefing(const std::string& npc, const std::string& wolf, bool note = true);
    void sendWelcome(const std::string& id, const std::vector<chronicle::Row>& events);
    std::map<std::string, CrierCall> criers_;       // By community.
    double criedDay_ = -1;
    void festivalCrier(const std::string& community, const std::string& festival);
    void cryLegends();
    void tendCriers();
    std::string crierOf(const std::string& community);
    // Residents' troubles (doc 57, 3; RatwGameTroubles.cpp): worked out when a player talks to a resident and kept a game
    // day (nothing scans the population); spoken of only to a wolf the resident trusts (troubles.json's `speak`). A wolf
    // who has heard one keeps it in its unfinished business until it is solved or gone.
    struct TroubleSeen
    {
        troubles::Trouble trouble;
        double day = -1;                            // The calendar day it was worked out.
    };
    std::map<std::string, TroubleSeen> troubles_;   // By resident.
    std::map<std::string, double> idleSince_;       // Resident -> the day it was first found without a post.
    struct TroubleHeard
    {
        std::string kind;
        double day = 0;
    };
    std::map<std::string, std::map<std::string, TroubleHeard>> troublesHeard_;   // Wolf -> resident -> what it heard.
    std::map<std::string, std::string> troubleBriefed_;   // "npc|wolf" -> the kind its Mind was told of for the coming reply.
    troubles::Reads troubleReads();
    const troubles::Trouble& troubleOf(const std::string& resident);
    bool troubleSpoken(const std::string& npc, const std::string& wolf) const;
    std::map<std::string, std::string> troubleBlanks(const std::string& viewer, const troubles::Trouble& t) const;
    std::string troubleBriefing(const std::string& npc, const std::string& wolf);
    std::string troubleSaid(const std::string& npc, const std::string& wolf, std::size_t seed);
    void heardTrouble(const std::string& wolf, const std::string& npc, const std::string& kind);
    void troublesSave(json::Value& root) const;
    void troublesLoad(const json::Value& saved);
    // Protected residents (doc 57, 6): marked in Atlas, heads of great houses, faction members with a rank, and any the
    // Dungeon Master marks; the Dungeon Master may unmark any (npc.unprotect). Kept by the game, not the society.
    std::set<std::string> protectedMarks_, unprotectedMarks_;
    bool isProtected(const std::string& id) const;
    // Solving them (doc 57, Phase 2): the menu's entries for a resident or the employer or master who can help, the
    // action, and its success; a gift that brings a short household to its refill is a solving too.
    std::map<std::string, double> troubleSolvedBy_; // "resident|wolf" -> the day the wolf last solved one of its troubles.
    std::map<std::string, double> troubleRests_;    // "resident|kind" -> the day one was solved (it rests a while).
    void troubleActions(const Entity& self, const Entity& e, double apart, json::Value& actions);
    Result troubleAction(const std::string& wolf, const std::string& target, const std::string& action);
    void troubleSolved(const std::string& wolf, const troubles::Trouble& t, const std::string& deedKind, int weight,
                       std::int64_t coins, const std::string& words);
    troubles::Trouble shortHousehold(const std::string& member);
    void troubleGiven(const std::string& giver, const troubles::Trouble& before);
    std::string vacancyFor(const std::string& employer, const std::string& resident) const;
    const Position* mastersTrade(const std::string& master) const;
    // Town projects (doc 57, Phase 3; RatwGameProjects.cpp): the ledger, who is working at which (and in what role), the
    // hands posted today, and the daily pass that turns a project's coin into contracts for its materials and hired hands.
    projects::Ledger projects_;
    struct ProjectWorker
    {
        std::string project, role;
    };
    std::map<std::string, ProjectWorker> projectWork_;   // By wolf.
    std::map<std::string, std::pair<std::int64_t, int>> projectHands_;   // Project -> (day, hands posted that day).
    double projectAccumulator_ = 0, projectDay_ = -1;
    bool projectCommand(Connection* c, const json::Value& j, Result& result);
    void projectTick(double dt);
    void projectSpend(projects::Project& p);
    bool projectReady(const projects::Project& p) const;
    void finishProject(projects::Project& p);
    void refundProject(projects::Project& p);
    bool placeProject(projects::Project& p);
    void projectsFromEvent(const WorldEvent& e);
    double projectHourValue(const std::string& town) const;
    int projectHas(const projects::Project& p, const std::string& item) const;
    json::Value projectView(const std::string& viewer, const projects::Project& p, bool here);
    // Phase 4: what standing projects do (handed to the world when it changes), their wear and mending, and the
    // proposer that posts a town's projects from its real needs, one town a day at dawn.
    std::string worksKey_;
    std::size_t projectTowns_ = 0;                  // How many projects the economy was last told the towns of.
    std::size_t proposeNext_ = 0;
    double proposedDay_ = -1;
    std::map<std::string, std::vector<std::pair<std::string, double>>> recentRaids_;   // Town -> (cell, day) robbed lately.
    std::map<std::string, std::vector<std::string>> distressRun_;   // Town -> its distress at the latest decisions.
    double distressDay_ = -1;
    void refreshWorks();
    void wearProjects(double days);
    void proposeProjects();
    static double projectStrength(const projects::Project& p);
    json::Value projectsView(const std::string& viewer, const std::string& town);
    const projects::Project* projectAt(const std::string& viewer) const;
    void projectsSave(json::Value& root) const;
    void projectsLoad(const json::Value& saved);
    // Tavern games (doc 54, 5; RatwGameTables.cpp; the rules in RatwTavernGames.cpp): at a table (`T`) in a common room
    // or an opened venue, within 1.5 tiles. One starts a game, others join or a resident is asked from the room (awake,
    // not at work, 14 or more, 16 for stakes); the game begins with 2 or more. Liar's Bones may be played for 0 to 5p
    // each, held in `table:<id>` and paid to the winner less a penny to the house (the inn's till). Residents stake only
    // from 30p or more, 3 staked games a game day, a twentieth of the purse at most. Moves show to the table and
    // watchers within 4 tiles; playing sharpens a wolf's skill at the game.
    struct Table
    {
        std::string id, cell, game, house;
        double x = 0, y = 0;                        // The table's tile.
        std::int64_t stake = 0;
        std::vector<std::string> seats;
        bool begun = false;
        double made = 0, lastMove = 0;              // World seconds.
        tavern::Rng rng;
        tavern::Knucklebones knuckles;
        tavern::WolvesAndDeer board;
        tavern::LiarsGame liars;
        std::map<std::string, double> awaySince;    // A seated wolf gone from the table, since.
        std::vector<std::pair<std::string, std::string>> log;   // Who (named as each viewer knows them) and what.
    };
    std::map<std::string, Table> tables_;           // By the table's tile ("cell|x|y").
    std::map<std::string, int> stakedToday_;        // "resident|day": staked games today.
    double tablesAccumulator_ = 0;
    bool tableCommand(Connection* c, const json::Value& j, Result& result);
    void tendTables(double dt);
    json::Value tableSelf(const std::string& viewer);
    std::string tableNear(const Entity& e, int& tx, int& ty);
    void tableSay(Table& t, const std::string& who, const std::string& what);   // "<who><what>" ("" who: as written).
    void endTable(Table& t, int winner, const std::string& why);
    void leaveTable(Table& t, const std::string& who, const std::string& why);
    void residentTurn(Table& t);
    std::string seatName(const std::string& viewer, const std::string& who) const;
    // Market stalls (doc 54, 3; RatwStalls.cpp): on Marketday from 7 to 2 in fair weather, a free stall spot on a city's
    // square rented for the morning (3p to the town); at most 6 a square (half its spots in a small one), one a wolf.
    // Wares go into the stall (`stall:<id>`), so nothing listed is sold elsewhere, given or eaten, each with a price.
    // A sale only while the keeper stands by it and has stirred in the last 10 minutes; at 2 (or in foul weather, the
    // fee returned) the stall clears and its goods go home.
    struct Stall
    {
        std::string id, keeper, community, cell;
        double x = 0, y = 0, day = 0;
        std::int64_t fee = 0, takings = 0;
        std::map<std::string, std::int64_t> prices;  // Each ware's price apiece; the goods themselves in `stall:<id>`.
        bool closed = false;                        // Cleared, with goods still to go home (a full purse).
    };
    std::vector<Stall> stalls_;
    std::uint64_t nextStall_ = 1;
    bool stallCommand(Connection* c, const json::Value& j, Result& result);
    json::Value stallsView(const std::string& cellId);
    json::Value stallSelf(const std::string& viewer);
    void tendStalls();
    // Residents at players' stalls (doc 54, 3, its second part): once a game hour, grown townsfolk near a kept stall buy
    // one thing a day there if it is something a household wants and no dearer than in the town's shops.
    std::set<std::string> stallBuyers_;             // "resident|day": bought at a stall today.
    double stallBuyersHour_ = -1;
    void residentsAtStalls();
    void clearStall(Stall& st, const std::string& why, bool refund);
    bool marketOpen(const std::string& community);
    bool keeperPresent(const Stall& st);
    void stallsSave(json::Value& root) const;
    void stallsLoad(const json::Value& saved);
    // Notice boards (doc 54, 2; RatwBoards.cpp): one by each town's square. The work side (the town's contracts) is built
    // when read; the public side holds players' notices (documents of kind "notice"), a penny to the town, seven game
    // days, three a writer and thirty a board; residents answer a seeking (or offering) notice's structured `what`, never
    // its words.
    std::string boardNear(const std::string& player);          // The community whose board is within 2 tiles, or "".
    json::Value boardsView(const std::string& cellId);
    bool boardCommand(Connection* c, const json::Value& j, Result& result);
    void sendBoard(Connection* c, const std::string& community);
    void answerNotice(documents::Document& d);
    std::string noticeAnswerWords(const std::string& viewer, const documents::Document& d) const;
    std::string whatWords(const std::string& what) const;
    double noticesDay_ = -1, noticesHour_ = -1;
    void tendNotices();
    void lettersSave(json::Value& root) const;
    void lettersLoad(const json::Value& saved);
    // Story books (doc 51, Phase 7; RatwGameBooks.cpp): every book by its id, and each character's model calls for
    // books today.
    std::map<std::string, books::Book> books_;
    std::map<std::string, std::pair<std::int64_t, int>> modelBooks_;
    bool canRead(const std::string& viewer, const books::Book& b) const;
    std::string shelfKind(const std::string& viewer, const books::Book& b) const;
    json::Value bookSpine(const std::string& viewer, const books::Book& b) const;
    json::Value bookView(const std::string& viewer, const books::Book& b) const;
    void sendBook(Connection* c, const std::string& id);
    void sendShelf(Connection* c, const std::string& tab, const std::string& filter, int offset);
    Result linkScene(const std::string& who, books::Book& b, const std::string& session, int after);
    void booksSceneEnded(const std::string& member, const SocialSession& s);
    void finishBook(books::Book& b);
    void syncStory(const books::Book& b);
    void tendBooks();
    bool bookCommand(Connection* c, const json::Value& j, Result& result);
    void booksSave(json::Value& root) const;
    void booksLoad(const json::Value& saved);
    void recordStar(const std::string& kind, const std::string& source, const std::string& giver, const std::string& recipient, int xp);
    std::vector<std::string> circlesOf(const std::string& account) const;
    void sendCircles(const std::string& account);
    void circleChanged(const people::Circle& circle);
    bool circleCommand(Connection* c, const json::Value& j, Result& result);
    Result circleLine(Connection* c, const std::string& circle, const std::string& text);
    void tendCircles();
    void circlesSave(json::Value& root) const;
    void circlesLoad(const json::Value& saved);
    void onSettled(const LedgerEntry& entry);
    void markChapterStory(SocialStory& story);
    void onStoryClosed(const SocialStory& story);
    // Chapters (RatwGameChapters.cpp; doc 32, Part 3).
    chapter::Chapters chapters_;
    std::map<std::string, json::Value> chapterViews_;
    bool chapterViewsDirty_ = true;
    double chapterViewsAccumulator_ = 0, chapterAccumulator_ = 0;
    bool chapterCommand(Connection* c, const json::Value& j, Result& result);
    void chapterTick(double dt);
    void refreshChapterViews(double dt);
    void tellChapter(const std::string& chapterId, const std::string& words, const std::string& except = {});
    void chapterAdvanced(const std::string& chapterId);
    // Factions in play (RatwGameFactions.cpp; doc 32, Part 4).
    faction::Factions factions_;
    double factionAccumulator_ = 0, factionRefresh_ = 0;
    std::set<std::string> factionIncidents_, factionCharged_, owing_;
    std::map<std::string, std::int64_t> factionTradePennies_;
    void refreshFactions();
    std::string officialOf(const std::string& npcId) const;
    std::vector<std::string> chapterMembers(const std::string& chapterId) const;
    double standingOf(const std::string& factionId, const std::string& chapterId) const;
    void factionTick(double dt);
    std::string factionReport(const std::string& askerId, const std::string& officialId, bool paid);
    void missionBoard(const std::string& factionId, const std::string& officialId, double day);
    std::string missionWords(const faction::Mission& m, const std::string& viewer) const;
    void completeMission(faction::Mission& m, double day);
    void missionTick(double dt, double day);
    bool factionCommand(Connection* c, const json::Value& j, Result& result);
    bool factionTrade(const std::string& playerId, const std::string& merchantId, bool buy, Result& refusal);
    void afterFactionTrade(const std::string& playerId, const std::string& merchantId, std::int64_t spent);
    void factionScene(const std::string& cellId, const std::string& chapterId);
    std::string factionContext(const std::string& npcId, const std::string& playerId) const;
    json::Value standingsView(const std::string& chapterId) const;
    // A Chapter's ground (RatwGameEstates.cpp; doc 32, Part 5).
    estate::Estates estates_;
    double estateAccumulator_ = 0, estateRefresh_ = 0;
    std::set<std::string> rentWarned_;
    void refreshEstates();
    bool mayEnterPlace(const std::string& who, const std::string& cell) const;
    void estateTick(double dt);
    bool estateCommand(Connection* c, const json::Value& j, Result& result);
    json::Value placeView(const std::string& viewer) const;
    json::Value leasesView(const std::string& chapterId) const;
    bool campHere(const std::string& who) const;
    // Camps, Halls and Holds (RatwGameCamps.cpp; doc 32, 5.3–5.7).
    camp::Camps camps_;
    std::map<std::string, std::string> building_;                    // Player → the structure they work on.
    std::set<std::string> townCells_;
    double campAccumulator_ = 0, lastWearDay_ = -1;
    std::string whyNotGround(const std::string& cellId, int x, int y, bool town = false) const;   // (town: a town's project, doc 57)
    bool campCommand(Connection* c, const json::Value& j, Result& result);
    void campTick(double dt);
    json::Value structuresView(const std::string& viewer, const std::string& cellId) const;
    json::Value storesView(const std::string& cellId) const;     // Home storage (doc 36).
    json::Value campView(const std::string& viewer) const;
    json::Value sitesView(const std::string& chapterId) const;
    std::string groundKey_;
    void refreshGround();
    bool treatyAllows(const std::string& factionId, const std::string& chapterId, const std::string& what) const;
    // Halls and Holds (RatwGameHolds.cpp; doc 32, Phase 9).
    double holdAccumulator_ = 0;
    std::map<std::string, double> levyWeek_;
    std::uint64_t levyNext_ = 0;
    bool holdCommand(Connection* c, const json::Value& j, Result& result);
    void holdTick(double dt);
    json::Value holdView(const std::string& chapterId, const std::string& viewer) const;
    std::string swornContext(const std::string& npcId) const;
    // Folk coming to live and work at a Hold (doc 32, 5.5; doc 16's migration): the Chapter's Hold site, those living
    // there for it (sworn, or its staff), why a resident may not come, finding them a home by a building, and the
    // resentment of the faction they leave.
    const camp::Site* holdSite(const std::string& chapterId) const;
    int housedAt(const camp::Site& site) const;
    std::string whyNotMigrate(const std::string& npcId, const std::string& chapterId) const;
    Result moveToHold(const std::string& npcId, const camp::Site& site);
    void resentLoss(const std::string& npcId, const std::string& chapterId);
    void migrationTick(double day);
    void payToll(const std::string& who);
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
    // One server per world (RatwGameOwner.cpp): the database lock's own connection, or the save's lock file.
    std::unique_ptr<PgClient> ownerPg_;
    int ownerFile_ = -1;
    double ownerCheck_ = 0;
    static constexpr double OwnerCheckSeconds = 30;
    bool takeOwnership(std::string& problem);
    void keepOwnership(double dt);
    void letGoOfOwnership();
    perf::Meter* meter_ = nullptr;
    std::int64_t loadedBuild_ = 0, pendingRelease_ = 0;
    std::map<std::string, std::string> worldFiles_, cellHeaders_;
    std::map<std::string, std::pair<std::string, std::string>> exportCells_;   // A world export's cells: body, seams.
    std::string liveWorldId_;
    std::unique_ptr<health::Recorder> health_;                         // Server health kept (RatwHealth.h).
    std::unique_ptr<watch::Feed> watch_;                               // The LIVE map's positions (doc 34); a database world only.
    double watchAccumulator_ = 0;
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
    void feedWatch(double dt);              // A frame for the LIVE map while a Dungeon Master watches (RatwGameWatch.cpp).
    std::string watchFrame() const;
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
    void sendSnapshots(const std::vector<Connection*>& sending);
    void movementSounds();
    // Speech to everyone who can perceive it. `to`: whom it was meant for (each listener is told, as they can tell).
    // `group`: "party" or "chapter", said to the author's party or Chapter (its members who hear it are told so).
    std::vector<std::string> publish(const std::string& author, const ParsedPost& post, Voice voice, const std::vector<std::string>& to = {},
                                     const std::string& group = {});
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
    // Armour and weapons someone has on, for the cards' little dolls (doc 35): [{place, name, weapon?, protect?}].
    json::Value gearView(const Entity& e) const;
    std::map<std::string, std::deque<std::string>> recentScenes_;   // By cell: scenes said there lately.
    std::map<std::string, double> barkLast_;                         // By cell: when someone last called out there.
    double barkLookIn_ = 0;
    void barks(double dt);
    // Restday sermons (doc 42, Phase 6): a preacher at the pulpit speaks this week's sermon, a line a minute, while a
    // player is in the church to hear it.
    struct SermonState
    {
        std::string sermon;
        std::size_t line = 0;
        double nextAt = 0;
        std::int64_t day = -1;
    };
    std::map<std::string, SermonState> sermons_;   // By preacher.
    double sermonLookIn_ = 0;
    void sermons(double dt);
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
        std::int64_t books = -1;                    // The month's books' revision.
        std::int64_t houses = -1;                   // The great houses' state's revision.
        std::int64_t memory = -1;                   // The economy's memory's revision (doc 42).
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
