#include "Runtime/RatwPersistence.h"
#include "Runtime/RatwJson.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#if PLATFORM_LINUX
#include <cerrno>
#include <cstdio>
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
    Flush();
    {
        std::lock_guard<std::mutex> Guard(QueueLock);
        Stopping = true;
    }
    Wake.notify_all();
    if (WriterThread.joinable())
        WriterThread.join();
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

namespace
{
// PostgreSQL's jsonb cannot hold the NUL character. The JSON writer escapes one as \u0000; turn each such escape
// (not a literal backslash followed by "u0000" in some text, which the writer escapes as \\u0000) into U+FFFD.
std::string WithoutNul(const char* Json, int32 Length)
{
    std::string Out(Json, static_cast<size_t>(Length));
    for (size_t At = Out.find("\\u0000"); At != std::string::npos; At = Out.find("\\u0000", At + 1))
    {
        size_t Slashes = 0;
        while (At > Slashes && Out[At - 1 - Slashes] == '\\')
            ++Slashes;
        if (Slashes % 2 == 0)
            Out.replace(At, 6, "\\ufffd");
    }
    return Out;
}
} // namespace

bool FRatwPersistence::OpenDatabase(const FString& ConnInfo, const FString& InWorldId)
{
    std::string Problem;
    if (!Pg.connect(TCHAR_TO_UTF8(*ConnInfo), Problem))
    {
        PgError = UTF8_TO_TCHAR(Problem.c_str());
        return false;
    }
    WorldId = InWorldId;
    const auto Log = Pg.exec("SELECT to_regprocedure('game.record_events(ratw_id,jsonb)') IS NOT NULL");
    EventsSupported = Log.ok && !Log.rows.empty() && Log.rows[0][0] && *Log.rows[0][0] == "t";
    if (!EventsSupported)
        UE_LOG(LogTemp, Warning, TEXT("RATW event log off: this database has no game.events yet (run tools/world_db.py migrate)."));
    // A list the database doesn't store as rows (an older database) stays in the checkpoint row, as save_checkpoint
    // leaves it; a delta must do the same.
    const auto Lists = Pg.exec("SELECT name FROM game.sections");
    for (const auto& Row : Lists.rows)
        if (Row[0])
            KnownLists.insert(*Row[0]);
    const auto Delta = Pg.exec("SELECT to_regprocedure('game.save_checkpoint_delta(ratw_id,bigint,text,jsonb)') IS NOT NULL");
    DeltasSupported = Delta.ok && !Delta.rows.empty() && Delta.rows[0][0] && *Delta.rows[0][0] == "t";
    if (!DeltasSupported)
        UE_LOG(LogTemp, Warning, TEXT("RATW saves are whole: this database has no game.save_checkpoint_delta yet (run tools/world_db.py migrate)."));
    return true;
}

void FRatwPersistence::QueueEvents(std::vector<ratw::WorldEvent> Events)
{
    if (Events.empty() || !IsDatabase() || !EventsSupported)
        return;
    std::lock_guard<std::mutex> Guard(QueueLock);
    for (auto& Event : Events)
        PendingEvents.push_back(std::move(Event));
    if (PendingEvents.size() > EventsQueued)
        PendingEvents.erase(PendingEvents.begin(), PendingEvents.begin() + std::ptrdiff_t(PendingEvents.size() - EventsQueued));
}

FString FRatwPersistence::Load()
{
    if (IsDatabase())
    {
        // game.load_checkpoint reassembles the save from its tables (migration 0012).
        std::lock_guard<std::mutex> Connection(PgLock);
        const auto Result = Pg.exec("SELECT game.load_checkpoint($1)", {std::string(TCHAR_TO_UTF8(*WorldId))});
        if (!Result.ok)
        {
            PgError = UTF8_TO_TCHAR(Result.error.c_str());
            return TEXT("{\"schema\":-1}");
        }
        if (Result.rows.empty() || !Result.rows[0][0])
            return FString();                     // A world nobody has played yet.
        if (Result.rows[0][0]->empty())
            return TEXT("{\"schema\":-1}");
        return FString(UTF8_TO_TCHAR(Result.rows[0][0]->c_str()));
    }
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

namespace
{
// The lists of a save stored one row per entry, and how each entry is keyed: game.sections (migration 0012) exactly.
struct FSaveList
{
    const char* Name;
    const TCHAR* Parent;                            // The list is Document[Parent][Field], or Document[Field].
    const TCHAR* Field;
    std::vector<const TCHAR*> Key;                  // Fields joined with '|' (a missing one is empty).
};
const std::vector<FSaveList>& SaveLists()
{
    static const std::vector<FSaveList> Lists{
        {"accounts", TEXT("accounts"), TEXT("entries"), {TEXT("username")}},
        {"players", nullptr, TEXT("players"), {TEXT("id")}},
        {"npcs", nullptr, TEXT("npcs"), {TEXT("id")}},
        {"mapMemories", nullptr, TEXT("mapMemories"), {TEXT("observer"), TEXT("id")}},
        {"activeMemory", nullptr, TEXT("activeMemory"), {TEXT("key")}},
        {"summaries", nullptr, TEXT("summaries"), {TEXT("id")}},
        {"ledger", nullptr, TEXT("ledger"), {TEXT("event"), TEXT("actor"), TEXT("partner"), TEXT("reason")}},
        {"socialRecent", nullptr, TEXT("socialRecent"), {TEXT("event"), TEXT("actor"), TEXT("cell")}},
        {"socialSessions", nullptr, TEXT("socialSessions"), {TEXT("id")}},
        {"bonds", nullptr, TEXT("bonds"), {TEXT("holder"), TEXT("other")}}};
    return Lists;
}
// A field as `e->>'field'` gives it: text as it is, whole numbers without a decimal point, missing or null as ''.
std::string KeyText(const TSharedPtr<FJsonObject>& Entry, const TCHAR* Field)
{
    const auto* Value = Entry.IsValid() ? Entry->Values.Find(Field) : nullptr;
    if (!Value || !Value->IsValid())
        return {};
    switch ((*Value)->Type)
    {
    case EJson::String:
        return TCHAR_TO_UTF8(*(*Value)->AsString());
    case EJson::Number:
    {
        const double Number = (*Value)->AsNumber();
        if (FMath::IsFinite(Number) && Number == FMath::FloorToDouble(Number) && FMath::Abs(Number) < 1e15)
            return std::to_string(static_cast<long long>(Number));
        return TCHAR_TO_UTF8(*FString::SanitizeFloat(Number));
    }
    case EJson::Boolean:
        return (*Value)->AsBool() ? "true" : "false";
    default:
        return {};
    }
}
std::string Condensed(const TSharedPtr<FJsonValue>& Value)
{
    FString Text;
    const auto Out = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);
    FJsonSerializer::Serialize(Value, FString(), Out);
    const FTCHARToUTF8 Utf8(*Text);
    return std::string(Utf8.Get(), Utf8.Length());
}
void QuoteJson(std::string& Out, const std::string& Text)
{
    Out += '"';
    for (const unsigned char Ch : Text)
        if (Ch == '"' || Ch == '\\')
            Out += '\\', Out += char(Ch);
        else if (Ch < 0x20)
        {
            char Escape[8];
            std::snprintf(Escape, sizeof Escape, "\\u%04x", Ch);
            Out += Escape;
        }
        else
            Out += char(Ch);
    Out += '"';
}
uint64 Fingerprint(const std::string& Data, int32 Position)
{
    uint64 Hash = 1469598103934665603ULL;
    for (const unsigned char Ch : Data)
        Hash = (Hash ^ Ch) * 1099511628211ULL;
    return (Hash ^ uint64(Position)) * 1099511628211ULL;
}
} // namespace

void FRatwPersistence::Prepare(Checkpoint& Next, std::map<std::string, std::map<std::string, uint64>>& NowWritten)
{
    TSharedPtr<FJsonObject> Document;
    FString NpcStates;
    Next.Build(Document, NpcStates);
    Next.Build = nullptr;
    const FTCHARToUTF8 States(NpcStates.IsEmpty() ? TEXT("[]") : *NpcStates);
    Next.NpcStates = std::string(States.Get(), States.Length());
    if (!Document.IsValid())
        Document = MakeShared<FJsonObject>();
    const bool Whole = !DeltasSupported || !HaveWritten || DeltasSinceWhole >= DeltasBetweenWholeSaves;
    static const bool Verify = !FPlatformMisc::GetEnvironmentVariable(TEXT("RATW_VERIFY_SAVES")).IsEmpty();
    if (Whole || Verify)
    {
        const FTCHARToUTF8 Utf8(*ratwjson::Encode(Document));
        (Whole ? Next.Payload : Next.Whole) = WithoutNul(Utf8.Get(), Utf8.Length());
    }
    // Every list: each entry's key (repeats numbered "#2", "#3"... in order, as the database numbers them), and what
    // changed since the last write. The lists then come out of the document, which leaves what the checkpoint row holds.
    std::string Changes = "{";
    for (const auto& List : SaveLists())
    {
        if (!KnownLists.count(List.Name))
            continue;
        TSharedPtr<FJsonObject> Container = Document;
        if (List.Parent)
        {
            const TSharedPtr<FJsonObject>* Parent = nullptr;
            Container = Document->TryGetObjectField(List.Parent, Parent) && Parent ? *Parent : nullptr;
        }
        const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
        if (!Container.IsValid() || !Container->TryGetArrayField(List.Field, Entries) || !Entries)
            continue;
        auto& Now = NowWritten[List.Name];
        const auto Before = WrittenRows.find(List.Name);
        std::map<std::string, int> Seen;
        std::string Rows;
        for (int32 Position = 0; Position < Entries->Num(); ++Position)
        {
            const auto& Entry = (*Entries)[Position];
            std::string Key;
            const auto Fields = Entry.IsValid() ? Entry->AsObject() : nullptr;
            for (std::size_t Part = 0; Part < List.Key.size(); ++Part)
                Key += (Part ? "|" : "") + KeyText(Fields, List.Key[Part]);
            if (const int Count = ++Seen[Key]; Count > 1)
                Key += "#" + std::to_string(Count);
            const std::string Data = Condensed(Entry);
            const uint64 Mark = Fingerprint(Data, Position);
            Now[Key] = Mark;
            if (Whole)
                continue;
            if (Before != WrittenRows.end())
                if (const auto Old = Before->second.find(Key); Old != Before->second.end() && Old->second == Mark)
                    continue;
            if (!Rows.empty())
                Rows += ',';
            Rows += "{\"key\":";
            QuoteJson(Rows, Key);
            Rows += ",\"position\":" + std::to_string(Position) + ",\"data\":" + Data + "}";
        }
        if (!Whole)
        {
            std::string Deleted;
            if (Before != WrittenRows.end())
                for (const auto& [Key, Mark] : Before->second)
                    if (!Now.count(Key))
                    {
                        if (!Deleted.empty())
                            Deleted += ',';
                        QuoteJson(Deleted, Key);
                    }
            if (Changes.size() > 1)
                Changes += ',';
            QuoteJson(Changes, List.Name);
            Changes += ":{\"rows\":[" + Rows + "],\"deleted\":[" + Deleted + "]}";
        }
        Container->RemoveField(List.Field);
    }
    if (!Whole)
    {
        const FTCHARToUTF8 Rest(*ratwjson::Encode(Document));
        Next.Payload = WithoutNul(Rest.Get(), Rest.Length());
        Next.Changes = WithoutNul(Changes.c_str(), int32(Changes.size())) + "}";
        Next.Delta = true;
    }
}

bool FRatwPersistence::Write(const Checkpoint& Save, const std::vector<ratw::WorldEvent>& Events)
{
    const std::string World(TCHAR_TO_UTF8(*WorldId));
    // One transaction: the checkpoint and the published NPC states share a timestamp (now() is the transaction's
    // start), which is how start-up tells this server's own NPC states from ones written by someone else.
    auto Failed = [this](const ratw::PgResult& R) {
        PgError = UTF8_TO_TCHAR(R.error.c_str());
        Pg.exec("ROLLBACK");
        return false;
    };
    auto Result = Pg.exec("BEGIN");
    if (!Result.ok)
        return Failed(Result);
    // game.save_checkpoint splits a whole save into its tables, writing only what changed (migration 0012);
    // game.save_checkpoint_delta takes just the changed rows (migration 0022).
    Result = Save.Delta
                 ? Pg.exec("SELECT game.save_checkpoint_delta($1, $2, $3, $4::jsonb)",
                           {World, std::to_string(Save.Revision), Save.Payload, Save.Changes})
                 : Pg.exec("SELECT game.save_checkpoint($1, $2, $3)", {World, std::to_string(Save.Revision), Save.Payload});
    if (!Result.ok)
        return Failed(Result);
    Result = Pg.exec(
        "INSERT INTO live.npc_state (world_id, npc_id, alive, state, updated_at) "
        "SELECT $1, e->>'id', NOT coalesce((e->>'dead')::boolean, false), e - 'id', now() FROM jsonb_array_elements($2::jsonb) e "
        "ON CONFLICT (world_id, npc_id) DO UPDATE SET alive = excluded.alive, state = excluded.state, updated_at = excluded.updated_at "
        // Only residents whose state changed: most of a town is where it was a few seconds ago. A row left alone keeps
        // an updated_at no later than this checkpoint, so ExternalNpcStates() still sees only outside edits.
        "WHERE live.npc_state.alive IS DISTINCT FROM excluded.alive OR live.npc_state.state IS DISTINCT FROM excluded.state",
        {World, Save.NpcStates});
    if (!Result.ok)
        return Failed(Result);
    if (!Events.empty())
    {
        Result = Pg.exec("SELECT game.record_events($1, $2::jsonb)", {World, ratw::eventsJson(Events)});
        if (!Result.ok)
            return Failed(Result);
    }
    Result = Pg.exec("COMMIT");
    PgError = UTF8_TO_TCHAR(Result.error.c_str());
    return Result.ok;
}

void FRatwPersistence::Writer()
{
    std::unique_lock<std::mutex> Queue(QueueLock);
    for (;;)
    {
        Wake.wait(Queue, [this] { return Stopping || Pending.has_value(); });
        if (!Pending)
            return;
        Checkpoint Next = std::move(*Pending);
        Pending.reset();
        std::vector<ratw::WorldEvent> Events;
        Events.swap(PendingEvents);                 // Everything queued so far goes with this checkpoint.
        Writing = true;
        Queue.unlock();
        bool Ok;
        FString Problem;
        const bool Built = static_cast<bool>(Next.Build);
        std::map<std::string, std::map<std::string, uint64>> NowWritten;
        if (Built)
            Prepare(Next, NowWritten);
        {
            std::lock_guard<std::mutex> Connection(PgLock);
            Ok = Write(Next, Events);
            Problem = PgError;
            if (Ok && Next.Delta && !Next.Whole.empty())
            {
                // What the database now reads back, against the whole document this delta stood for.
                const auto Same = Pg.exec("SELECT game.load_checkpoint($1)::jsonb = $2::jsonb",
                                          {std::string(TCHAR_TO_UTF8(*WorldId)), Next.Whole});
                const bool Match = Same.ok && !Same.rows.empty() && Same.rows[0][0] && *Same.rows[0][0] == "t";
                UE_LOG(LogTemp, Display, TEXT("RATW_SAVE_VERIFY revision=%llu %s; sent %llu of %llu bytes"), Next.Revision,
                       Match ? TEXT("matches") : TEXT("MISMATCH"),
                       static_cast<unsigned long long>(Next.Payload.size() + Next.Changes.size()),
                       static_cast<unsigned long long>(Next.Whole.size()));
            }
        }
        // What is in the database now. A save handed over as text (not built here) leaves nothing to compare the next
        // one with, so that one is whole.
        HaveWritten = Ok && Built;
        if (HaveWritten)
        {
            WrittenRows = std::move(NowWritten);
            DeltasSinceWhole = Next.Delta ? DeltasSinceWhole + 1 : 0;
        }
        Queue.lock();
        Writing = false;
        if (!Ok)
        {
            BackgroundFailed = true;
            BackgroundError = Problem;
        }
        Idle.notify_all();
    }
}

bool FRatwPersistence::SaveInBackground(const FString& Payload, uint64 Revision, const FString& NpcStates)
{
    if (!IsDatabase())
        return Save(Payload, Revision, NpcStates);
    const FTCHARToUTF8 Utf8(*Payload);
    const FTCHARToUTF8 States(NpcStates.IsEmpty() ? TEXT("[]") : *NpcStates);
    {
        std::lock_guard<std::mutex> Guard(QueueLock);
        if (BackgroundFailed)
            return false;
        Pending = Checkpoint{WithoutNul(Utf8.Get(), Utf8.Length()), std::string(States.Get(), States.Length()), Revision};
        if (!WriterThread.joinable())
            WriterThread = std::thread([this] { Writer(); });
    }
    Wake.notify_all();
    return true;
}

bool FRatwPersistence::Save(FBuild Build, uint64 Revision)
{
    if (IsDatabase())
        return SaveInBackground(MoveTemp(Build), Revision) && Flush();
    TSharedPtr<FJsonObject> Document;
    FString NpcStates;
    Build(Document, NpcStates);
    return Save(ratwjson::Encode(Document), Revision, NpcStates);
}

bool FRatwPersistence::SaveInBackground(FBuild Build, uint64 Revision)
{
    if (!IsDatabase())
        return Save(MoveTemp(Build), Revision);
    {
        std::lock_guard<std::mutex> Guard(QueueLock);
        if (BackgroundFailed)
            return false;
        Checkpoint Next;
        Next.Revision = Revision;
        Next.Build = MoveTemp(Build);
        Pending = MoveTemp(Next);
        if (!WriterThread.joinable())
            WriterThread = std::thread([this] { Writer(); });
    }
    Wake.notify_all();
    return true;
}

bool FRatwPersistence::Flush()
{
    std::unique_lock<std::mutex> Queue(QueueLock);
    Idle.wait(Queue, [this] { return !Pending && !Writing; });
    return !BackgroundFailed;
}

bool FRatwPersistence::Save(const FString& Payload, uint64 Revision, const FString& NpcStates)
{
    if (IsDatabase())
    {
        // In order after any background save still waiting, and finished before returning.
        return SaveInBackground(Payload, Revision, NpcStates) && Flush();
    }
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

TArray<TPair<FString, FString>> FRatwPersistence::ExternalNpcStates()
{
    TArray<TPair<FString, FString>> Out;
    if (!IsDatabase())
        return Out;
    std::lock_guard<std::mutex> Connection(PgLock);
    const auto Result = Pg.exec(
        "SELECT npc_id, state::text FROM live.npc_state WHERE world_id = $1 AND updated_at > "
        "coalesce((SELECT saved_at FROM game.checkpoints WHERE world_id = $1), '-infinity'::timestamptz)",
        {std::string(TCHAR_TO_UTF8(*WorldId))});
    for (const auto& Row : Result.rows)
        if (Row[0] && Row[1])
            Out.Emplace(FString(UTF8_TO_TCHAR(Row[0]->c_str())), FString(UTF8_TO_TCHAR(Row[1]->c_str())));
    return Out;
}

FString FRatwPersistence::Error() const
{
    {
        std::lock_guard<std::mutex> Guard(QueueLock);
        if (BackgroundFailed)
            return BackgroundError;
    }
    if (IsDatabase() || !PgError.IsEmpty())
        return PgError;
    return SecurityError.IsEmpty() ? Database.GetLastError() : SecurityError;
}
