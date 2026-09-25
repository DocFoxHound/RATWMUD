#include "Runtime/RatwPersistence.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#if PLATFORM_LINUX
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace
{
bool PrivateFile(const FString& Path, bool MayBeAbsent)
{
#if PLATFORM_LINUX
    struct stat Info;
    const FTCHARToUTF8 Utf8(*Path);
    if (lstat(Utf8.Get(), &Info) != 0) return MayBeAbsent && errno == ENOENT;
    if (!S_ISREG(Info.st_mode) || Info.st_uid != geteuid()) return false;
    return chmod(Utf8.Get(), S_IRUSR | S_IWUSR) == 0;
#else
    // Other platforms retain their OS ACL; public deployment is not supported.
    return true;
#endif
}
}

FRatwPersistence::~FRatwPersistence()
{
    if (Database.IsValid())
        Database.Close();
}

bool FRatwPersistence::Open(const FString& Path)
{
    DatabasePath = Path;
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    if (!PrivateFile(Path, true) || !PrivateFile(Path + TEXT("-wal"), true) || !PrivateFile(Path + TEXT("-shm"), true))
    {
        SecurityError = TEXT("Checkpoint must be a regular file owned by this OS user; symlinks are not supported.");
        return false;
    }
    if (!Database.Open(*Path))
        return false;
    return SecureFiles() && Database.Execute(TEXT("PRAGMA journal_mode=WAL;")) && Database.Execute(TEXT("PRAGMA synchronous=FULL;")) &&
           Database.Execute(
               TEXT("CREATE TABLE IF NOT EXISTS world_state (id INTEGER PRIMARY KEY CHECK(id=1), schema_version "
                    "INTEGER NOT NULL, revision INTEGER NOT NULL, payload TEXT NOT NULL);")) && SecureFiles();
}

bool FRatwPersistence::SecureFiles()
{
    if (PrivateFile(DatabasePath, false) && PrivateFile(DatabasePath + TEXT("-wal"), true) &&
        PrivateFile(DatabasePath + TEXT("-shm"), true)) return true;
    SecurityError = TEXT("Unable to protect checkpoint credential verifiers with owner-only file permissions.");
    return false;
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
    if (!Database.IsValid() || !SecureFiles() || !Database.Execute(TEXT("BEGIN IMMEDIATE;")))
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
    return SecurityError.IsEmpty() ? Database.GetLastError() : SecurityError;
}
