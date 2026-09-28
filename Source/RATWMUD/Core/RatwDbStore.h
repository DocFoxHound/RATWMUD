#pragma once
// A world's save in the database (game.checkpoints and the tables it splits into; migrations 0012, 0021, 0022),
// portable: what the Unreal runtime's FRatwPersistence does for a database world, and what a standalone server does.
// Saves are written on a worker thread, most of them as just the rows that changed since the last one.
#include "RatwJsonDoc.h"
#include "RatwPg.h"
#include "RatwWorld.h"

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace ratw
{
class DbStore
{
  public:
    DbStore() = default;
    DbStore(const DbStore&) = delete;
    DbStore& operator=(const DbStore&) = delete;
    ~DbStore();

    bool open(const std::string& conninfo, const std::string& worldId, std::string& error);
    bool opened() const { return !worldId_.empty(); }
    const std::string& worldId() const { return worldId_; }
    // The save as one document (game.load_checkpoint reassembles it): "" for a world nobody has played yet, and
    // {"schema":-1} if it can't be read.
    std::string load();

    // Fills in the save's document, and the NPCs' running states (a JSON array for live.npc_state), on the worker:
    // the game thread only captures what it reads. It must not touch anything the game changes.
    using Build = std::function<void(json::Value& document, std::string& npcStates)>;
    // Without waiting: a newer save replaces one still waiting. False once a background write has failed.
    bool saveInBackground(Build build, std::uint64_t revision);
    // A document already made, as text, without waiting (the next save after it is whole).
    bool saveInBackground(std::string payload, std::uint64_t revision, std::string npcStates);
    // The same, finished before returning.
    bool save(Build build, std::uint64_t revision) { return saveInBackground(std::move(build), revision) && flush(); }
    bool flush();                                   // Waits for any background save; false if one failed.

    // A whole save is sent at least this often (and on start and after any failure), whatever changed.
    static constexpr int DeltasBetweenWholeSaves = 20;
    static constexpr std::size_t EventsQueued = 200000;
    // Events for game.events, written with the next checkpoint (if the database has the log; else let go).
    void queueEvents(std::vector<WorldEvent> events);
    bool logsEvents() const { return eventsSupported_; }
    bool deltasSupported() const { return deltasSupported_; }
    // NPC states put in live.npc_state from outside since this server's last save: (id, state JSON).
    std::vector<std::pair<std::string, std::string>> externalNpcStates();
    std::string error() const;
    // Checks after each delta that the database reads back the document it stood for (RATW_VERIFY_SAVES does too).
    void setVerify(bool on) { verify_ = on; }
    // Where its notes go (a save's verification, say); standard error until set. Called on the worker thread.
    void setLog(std::function<void(const std::string&)> log) { log_ = std::move(log); }

  private:
    struct Checkpoint
    {
        std::string payload, npcStates;
        std::uint64_t revision = 0;
        Build build;
        bool delta = false;
        std::string changes, whole;
    };
    PgClient pg_;
    std::string worldId_, pgError_;
    std::map<std::string, std::map<std::string, std::uint64_t>> written_;
    bool haveWritten_ = false, deltasSupported_ = false, eventsSupported_ = false, verify_ = false;
    std::set<std::string> knownLists_;
    int deltasSinceWhole_ = 0;
    void prepare(Checkpoint& next, std::map<std::string, std::map<std::string, std::uint64_t>>& nowWritten);
    bool write(const Checkpoint& save, const std::vector<WorldEvent>& events);
    void writer();
    std::mutex pgLock_;
    mutable std::mutex queueLock_;
    std::condition_variable wake_, idle_;
    std::optional<Checkpoint> pending_;
    std::vector<WorldEvent> pendingEvents_;
    bool writing_ = false, stopping_ = false, backgroundFailed_ = false;
    std::string backgroundError_;
    std::thread writerThread_;
    std::function<void(const std::string&)> log_;
};

// PostgreSQL's jsonb cannot hold the NUL character: each \u0000 escape becomes �.
std::string withoutNul(std::string json);
} // namespace ratw
