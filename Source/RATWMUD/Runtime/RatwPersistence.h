#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "SQLiteDatabase.h"
#include "Core/RatwPg.h"
#include "Core/RatwWorld.h"
#include <condition_variable>
#include <functional>
#include <map>
#include <set>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

// One transaction holds the complete internally consistent MVP state. The
// payload is versioned JSON. It lives in the world database (game.checkpoints,
// one row per world) for a server started with -RatwDatabase, and in a SQLite
// file for exported playtest worlds and the offline test worlds.
class FRatwPersistence
{
  public:
    ~FRatwPersistence();
    bool Open(const FString& Path);
    // Saves to game.checkpoints for `WorldId` over its own connection.
    bool OpenDatabase(const FString& ConnInfo, const FString& WorldId);
    bool IsDatabase() const { return !WorldId.IsEmpty(); }
    FString Load();
    // NpcStates (database only): a JSON array of {"id", ...state} written to live.npc_state in the same transaction.
    bool Save(const FString& Payload, uint64 Revision, const FString& NpcStates = FString());
    // Save() without waiting (database only; a file save is written at once): the write happens on a worker thread,
    // so the game does not stall on the database. A newer save replaces one still waiting. Returns false once a
    // background write has failed; Error() then says why.
    bool SaveInBackground(const FString& Payload, uint64 Revision, const FString& NpcStates = FString());
    // SaveInBackground() whose document is made on the worker too: Build fills in the save document and the NPC
    // states there, so the game thread only has to capture what Build reads. Build must not touch anything the game
    // changes. (A file save builds and writes at once.) The worker remembers what it last wrote and, most of the
    // time, sends only the rows that changed (game.save_checkpoint_delta, migration 0022); see Writer().
    using FBuild = std::function<void(TSharedPtr<FJsonObject>& Document, FString& NpcStates)>;
    bool SaveInBackground(FBuild Build, uint64 Revision);
    // The same, finished before returning (for changes that must be stored before the game replies).
    bool Save(FBuild Build, uint64 Revision);
    // A whole save is sent at least this often (and on start and after any failure), whatever changed.
    static constexpr int DeltasBetweenWholeSaves = 20;
    // Waits for any background save; false if one failed.
    bool Flush();
    // Events for the world's log (game.events), written in the same transaction as the next checkpoint. Only for a
    // database world whose database has the log (migration 0021; `python3 tools/world_db.py migrate`); otherwise
    // they are let go. At most EventsQueued wait; beyond that the oldest are dropped.
    static constexpr std::size_t EventsQueued = 200000;
    void QueueEvents(std::vector<ratw::WorldEvent> Events);
    bool LogsEvents() const { return EventsSupported; }
    // NPC states put in live.npc_state from outside since this server's last save (e.g. DEV copying PROD's): (id, state JSON).
    TArray<TPair<FString, FString>> ExternalNpcStates();
    FString Error() const;

  private:
    FSQLiteDatabase Database;
    FString DatabasePath;
    FString SecurityError;
    bool SecureFiles();
    ratw::PgClient Pg;
    FString WorldId, PgError;
    struct Checkpoint
    {
        std::string Payload, NpcStates;
        uint64 Revision = 0;
        FBuild Build;                               // When set, makes the document and NpcStates on the worker.
        bool Delta = false;                         // Payload is the document without its lists; Changes has them.
        std::string Changes;
        std::string Whole;                          // RATW_VERIFY_SAVES: the whole document, to check against.
    };
    // What the worker last wrote: for each list of the save, each entry's key and a fingerprint of it and its
    // place. Only the worker touches it.
    std::map<std::string, std::map<std::string, uint64>> WrittenRows;
    bool HaveWritten = false;
    bool DeltasSupported = false;                   // The database has game.save_checkpoint_delta (migration 0022).
    std::set<std::string> KnownLists;               // game.sections in this database: the lists stored as rows.
    int DeltasSinceWhole = 0;
    void Prepare(Checkpoint& Next, std::map<std::string, std::map<std::string, uint64>>& NowWritten);
    bool Write(const Checkpoint& Save, const std::vector<ratw::WorldEvent>& Events);   // One transaction (hold PgLock).
    void Writer();
    std::mutex PgLock;                              // The connection.
    mutable std::mutex QueueLock;                   // Everything below.
    std::condition_variable Wake, Idle;
    std::optional<Checkpoint> Pending;
    std::vector<ratw::WorldEvent> PendingEvents;
    bool EventsSupported = false;
    bool Writing = false, Stopping = false, BackgroundFailed = false;
    FString BackgroundError;
    std::thread WriterThread;
};
