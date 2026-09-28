#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "SQLiteDatabase.h"
#include "Core/RatwDbStore.h"
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
// file for exported playtest worlds and the offline test worlds. A database
// world is saved by the portable core (Core/RatwDbStore.h), as a standalone
// server saves it.
class FRatwPersistence
{
  public:
    ~FRatwPersistence()
    {
        if (Database.IsValid())
            Database.Close();
    }
    bool Open(const FString& Path);
    // Saves to game.checkpoints for `WorldId` over its own connection.
    bool OpenDatabase(const FString& ConnInfo, const FString& WorldId);
    bool IsDatabase() const { return Db.opened(); }
    FString Load();
    // NpcStates (database only): a JSON array of {"id", ...state} written to live.npc_state in the same transaction.
    bool Save(const FString& Payload, uint64 Revision, const FString& NpcStates = FString());
    // Save() without waiting (database only; a file save is written at once). Returns false once a background write
    // has failed; Error() then says why.
    bool SaveInBackground(const FString& Payload, uint64 Revision, const FString& NpcStates = FString());
    // SaveInBackground() whose document is made on the worker too (see ratw::DbStore::Build): the game thread only
    // captures what Build reads. Most saves then send only the rows that changed. (A file save builds and writes at once.)
    using FBuild = ratw::DbStore::Build;
    bool SaveInBackground(FBuild Build, uint64 Revision);
    // The same, finished before returning (for changes that must be stored before the game replies).
    bool Save(FBuild Build, uint64 Revision);
    static constexpr int DeltasBetweenWholeSaves = ratw::DbStore::DeltasBetweenWholeSaves;
    // Waits for any background save; false if one failed.
    bool Flush() { return !IsDatabase() || Db.flush(); }
    // Events for the world's log (game.events), written with the next checkpoint (database worlds with the log only).
    static constexpr std::size_t EventsQueued = ratw::DbStore::EventsQueued;
    void QueueEvents(std::vector<ratw::WorldEvent> Events) { if (IsDatabase()) Db.queueEvents(std::move(Events)); }
    bool LogsEvents() const { return Db.logsEvents(); }
    // NPC states put in live.npc_state from outside since this server's last save (e.g. DEV copying PROD's): (id, state JSON).
    TArray<TPair<FString, FString>> ExternalNpcStates();
    FString Error() const;

  private:
    FSQLiteDatabase Database;
    FString DatabasePath;
    FString SecurityError, OpenError;
    bool SecureFiles();
    ratw::DbStore Db;
};
