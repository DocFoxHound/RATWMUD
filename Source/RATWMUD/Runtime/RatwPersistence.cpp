#include "Runtime/RatwPersistence.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

FRatwPersistence::~FRatwPersistence()
{
    if (Database.IsValid())
        Database.Close();
}

bool FRatwPersistence::Open(const FString& Path)
{
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    if (!Database.Open(*Path))
        return false;
    return Database.Execute(TEXT("PRAGMA journal_mode=WAL;")) && Database.Execute(TEXT("PRAGMA synchronous=FULL;")) &&
           Database.Execute(
               TEXT("CREATE TABLE IF NOT EXISTS world_state (id INTEGER PRIMARY KEY CHECK(id=1), schema_version "
                    "INTEGER NOT NULL, revision INTEGER NOT NULL, payload TEXT NOT NULL);"));
}

FString FRatwPersistence::Load()
{
    FString Payload;
    if (!Database.IsValid())
        return Payload;
    auto Statement = Database.PrepareStatement(TEXT("SELECT schema_version,payload FROM world_state WHERE id=1;"));
    const auto Result = Statement.Step();
    if (Result == ESQLitePreparedStatementStepResult::Row)
    {
        int32 Version = 0;
        Statement.GetColumnValueByIndex(0, Version);
        if (Version != 1)
            return TEXT("{\"schema\":-1}");
        if (!Statement.GetColumnValueByIndex(1, Payload) || Payload.IsEmpty())
            return TEXT("{\"schema\":-1}");
    }
    else if (Result != ESQLitePreparedStatementStepResult::Done)
        return TEXT("{\"schema\":-1}");
    return Payload;
}

bool FRatwPersistence::Save(const FString& Payload, uint64 Revision)
{
    if (!Database.IsValid() || !Database.Execute(TEXT("BEGIN IMMEDIATE;")))
        return false;
    auto Statement = Database.PrepareStatement(
        TEXT("INSERT INTO world_state(id,schema_version,revision,payload) VALUES(1,1,?,?) ON CONFLICT(id) DO UPDATE "
             "SET schema_version=1,revision=excluded.revision,payload=excluded.payload;"));
    const bool Written = Statement.SetBindingValueByIndex(1, static_cast<int64>(Revision)) &&
                         Statement.SetBindingValueByIndex(2, *Payload) && Statement.Execute();
    if (Written && Database.Execute(TEXT("COMMIT;")))
        return true;
    Database.Execute(TEXT("ROLLBACK;"));
    return false;
}

FString FRatwPersistence::Error() const
{
    return Database.GetLastError();
}
