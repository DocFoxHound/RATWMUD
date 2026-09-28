#include "Runtime/RatwGameMode.h"
#include "Runtime/RatwPlayerController.h"
#include "Runtime/RatwPersistence.h"
#include "Runtime/RatwDialogueProvider.h"
#include "Core/RatwSocialCore.h"
#include "Core/RatwCheckpoint.h"
#include "Core/RatwJsonDoc.h"
#include "Runtime/RatwJson.h"
#include "Runtime/RatwSocietyJson.h"
#include "Runtime/RatwDMBridge.h"
#include "Runtime/RatwAccounts.h"
#include "Runtime/RatwMotion.h"
#include "Core/RatwCellPrefetch.h"
#include "Core/RatwPg.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Misc/ScopeExit.h"
#include "Misc/Guid.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <iomanip>
#include <sstream>

using namespace ratwjson;

class FRatwRuntime : public TSharedFromThis<FRatwRuntime>
{
  public:
    ratw::World World;
    ratw::MemoryStore Memories;
    ratw::SocialLedger Social;
    FRatwPersistence Persistence;
    FRatwDialogueProvider Dialogue;
    FRatwDMBridge DM;
    FRatwAccounts Accounts;
    FRatwAccountRateLimit AuthRate;
    std::map<std::string, double> OperatorActivity;
    TArray<TWeakObjectPtr<ARatwPlayerController>> Clients;
    std::map<std::string, ratw::Entity> Characters;
    std::map<std::string, double> TypingExpiry, LastChat, NpcLastSpeech;
    std::map<std::string, double> LastMovementSound;
    std::map<std::string, std::string> CompanionOwner;
    std::set<std::string> PendingNpc;
    // What was said to an NPC while it was still answering someone, answered in turn when it finishes (see Talk()).
    struct FQueuedTalk
    {
        std::string PlayerId;
        FString HeardText;
        ratw::Voice Voice;
        bool HasSense;
        ratw::SensoryResult Sense;
    };
    static constexpr std::size_t MaxQueuedTalk = 4;
    std::map<std::string, std::deque<FQueuedTalk>> QueuedTalk;
    std::map<std::string, std::string> NpcMood;    // How each NPC felt after its last generated reply.
    struct FNudgeBudget
    {
        int64 Hour = -1;
        int32 Affinity = 0, Trust = 0;
    };
    std::map<std::string, FNudgeBudget> NudgeBudget;   // "npc|subject": what conversation has moved this game hour.
    std::map<std::string, std::vector<std::string>> CommandReceipts;
    std::map<std::string, std::map<std::string, FString>> ResponseReceipts;
    std::map<std::string, std::string> CurrentCommands;
    uint64 Sequence = 1, Revision = 0;
    double DmExpiryAccumulator = 60;
    bool DmNotified = true;                       // Check once at start for actions queued while the server was down.
    static constexpr uint32 SnapshotPhases = 4;     // 20 Hz ticks per client snapshot (five a second).
    uint32 SnapshotPhase = 0;
    // A change that should reach storage soon but needn't hold up the game (see SaveSoon()): seconds until it is.
    double SaveSoonIn = -1;
    double SnapshotAccumulator = 0, SaveAccumulator = 0, AmbientAccumulator = 0, ReleaseAccumulator = 0, DmAccumulator = 0,
           SpawnAccumulator = 0;
    // Spawn rules: when each spawned NPC was first seen dead, and rules backing off after a failure.
    std::map<std::string, double> DeadSince, SpawnBackoff;
    // -RatwDatabase: the world comes from the newest build in the world database, saves go to
    // game.checkpoints, and a new release is picked up by a restart once the server is empty.
    ratw::PgClient WorldDb;
    FString DatabaseName;
    int64 LoadedBuild = 0, PendingRelease = 0;
    // The loaded build without its NPC records: NPCs come from the live tables (live.people_manifest).
    std::map<std::string, std::string> WorldFiles;
    std::string LiveWorldId;
    // A streamed build (RATW_WORLD 3): every cell's header, read once; tiles are read from world.build_cells on demand.
    bool StreamedBuild = false;
    FRatwCellPrefetch CellPrefetch;                 // Streamed builds: the next cells' files, fetched ahead of need.
    bool Prefetching = false;
    double PrefetchAccumulator = 0;
    std::map<std::string, std::string> CellHeaders;
    double StreamLogAccumulator = 0;
    bool ReleaseAnnounced = false;
    bool StorageReady = false, DevTools = false, DevIdentity = false;

    static double Now()
    {
        return static_cast<double>(FDateTime::UtcNow().ToUnixTimestamp());
    }
    bool Start()
    {
        FString Manifest;
        const bool HasManifest = FParse::Value(FCommandLine::Get(), TEXT("RatwWorld="), Manifest);
        const bool Custom = HasManifest || FParse::Param(FCommandLine::Get(), TEXT("RatwWorld"));
        const bool Town = FParse::Param(FCommandLine::Get(), TEXT("RatwTown"));
        const bool Live = FParse::Value(FCommandLine::Get(), TEXT("RatwDatabase="), DatabaseName);
        if (int(Custom) + int(Town) + int(Live) > 1)
        {
            UE_LOG(LogTemp, Error, TEXT("RATW_WORLD_REJECTED: choose one of -RatwDatabase, -RatwWorld or -RatwTown."));
            FPlatformMisc::RequestExitWithStatus(false, 2);
            return false;
        }
        FString ConnInfo, WorldId;
        if (Live)
        {
            if (!LoadFromDatabase(ConnInfo, WorldId))
            {
                FPlatformMisc::RequestExitWithStatus(false, 2);
                return false;
            }
        }
        else if (Town)
        {
            // Greyfen Crossing is an Atlas Workshop world bundled with the project.
            const auto Loaded =
                World.loadWorldFile(S(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Data/Worlds/Greyfen/world.ratw"))));
            if (!Loaded.ok)
            {
                UE_LOG(LogTemp, Error, TEXT("RATW_WORLD_REJECTED: %s"), *F(Loaded.message));
                FPlatformMisc::RequestExitWithStatus(false, 2);
                return false;
            }
            UE_LOG(LogTemp, Display, TEXT("RATW_TOWN_LOADED cells=%d residents=%d"),
                   static_cast<int32>(World.cells().size()),
                   static_cast<int32>(World.society().state().residents.size()));
        }
        else if (Custom)
        {
            if (Manifest.IsEmpty() || FPaths::IsRelative(Manifest))
            {
                UE_LOG(LogTemp, Error, TEXT("RATW_WORLD_REJECTED: -RatwWorld requires an absolute manifest path."));
                FPlatformMisc::RequestExitWithStatus(false, 2);
                return false;
            }
            Manifest = FPaths::ConvertRelativePathToFull(Manifest);
            const auto Loaded = World.loadWorldFile(S(Manifest));
            if (!Loaded.ok)
            {
                UE_LOG(LogTemp, Error, TEXT("RATW_WORLD_REJECTED: %s"), *F(Loaded.message));
                FPlatformMisc::RequestExitWithStatus(false, 2);
                return false;
            }
            UE_LOG(LogTemp, Display, TEXT("RATW_WORLD_IMPORTED cells=%d fixtures=%d residents=%d"),
                   static_cast<int32>(World.cells().size()), static_cast<int32>(World.doors().size()),
                   static_cast<int32>(World.society().state().residents.size()));
        }
        else
            for (const char* CellId : {"tavern", "exterior", "loft"})
            {
                const FString File = FPaths::ProjectDir() / TEXT("Data/Cells") / (F(CellId) + TEXT(".cell"));
                if (FPaths::FileExists(File))
                {
                    const auto Loaded = World.loadCellFile(S(File));
                    if (!Loaded.ok)
                        UE_LOG(LogTemp, Warning, TEXT("RATW authored cell rejected; using built-in fallback: %s"),
                               *F(Loaded.message));
                }
            }
        FString Path = Custom ? FPaths::ProjectSavedDir() / TEXT("Atlas") / FMD5::HashAnsiString(*Manifest) /
                                    TEXT("ratw-world.sqlite")
                       : Town ? FPaths::ProjectSavedDir() / TEXT("ratw-town.sqlite")
                              : FPaths::ProjectSavedDir() / TEXT("ratw-world.sqlite");
        if (!Custom && !Town && !Live) for (const auto& Pair : World.cells()) World.cell(Pair.first)->region = "demo_reach";
        DevTools = FParse::Param(FCommandLine::Get(), TEXT("RatwDevTools"));
        DevIdentity = FParse::Param(FCommandLine::Get(), TEXT("RatwDevIdentity"));
        FParse::Value(FCommandLine::Get(), TEXT("RatwSave="), Path);
        StorageReady = Live ? Persistence.OpenDatabase(ConnInfo, WorldId) : Persistence.Open(FPaths::ConvertRelativePathToFull(Path));
        if (Live)
            Path = FString::Printf(TEXT("%s database, game.checkpoints[%s]"), *DatabaseName.ToUpper(), *WorldId);
        if (StorageReady)
            Load(Persistence.Load());
        else
            UE_LOG(LogTemp, Error, TEXT("RATW persistence open failed: %s"), *Persistence.Error());
        if (Live && StorageReady)
            ApplyExternalNpcStates();
        if (Live && !StorageReady)
        {
            UE_LOG(LogTemp, Error, TEXT("RATW_WORLD_REJECTED: the world database save is unavailable: %s"), *Persistence.Error());
            FPlatformMisc::RequestExitWithStatus(false, 2);
            return false;
        }
        if (Custom && !StorageReady)
        {
            UE_LOG(LogTemp, Error,
                   TEXT("RATW_WORLD_REJECTED: custom-world save is invalid or unavailable; "
                        "use a fresh -RatwSave path after changing geometry."));
            FPlatformMisc::RequestExitWithStatus(false, 2);
            return false;
        }
        FString DMPath;
        if (FParse::Value(FCommandLine::Get(), TEXT("RatwDMDirectory="), DMPath))
        {
            if (!StorageReady || !DM.Configure(DMPath))
            {
                UE_LOG(LogTemp, Error, TEXT("RATW operator bridge requires a healthy checkpoint and an absolute owner-private directory."));
                FPlatformMisc::RequestExitWithStatus(false, 2); return false;
            }
            Save();
            if (!StorageReady) { FPlatformMisc::RequestExitWithStatus(false, 2); return false; }
            UE_LOG(LogTemp, Display, TEXT("RATW private operator bridge enabled; no character credentials are accepted."));
        }
        FString Endpoint;
        FParse::Value(FCommandLine::Get(), TEXT("RatwDialogueEndpoint="), Endpoint);
        Dialogue.Configure(Endpoint);
        Consolidate();
        UE_LOG(LogTemp, Display, TEXT("RATW authoritative world ready; 20Hz; save=%s; dialogue=%s"), *Path,
               *Dialogue.Label());
        return true;
    }

    /** -RatwDatabase=prod|dev: the newest build of the one world, from the database named by RATW_DATABASE_URL. */
    bool LoadFromDatabase(FString& ConnInfo, FString& WorldId)
    {
        DatabaseName = DatabaseName.ToLower();
        if (DatabaseName != TEXT("prod") && DatabaseName != TEXT("dev"))
        {
            UE_LOG(LogTemp, Error, TEXT("RATW_WORLD_REJECTED: -RatwDatabase must be prod or dev."));
            return false;
        }
        ConnInfo = FPlatformMisc::GetEnvironmentVariable(TEXT("RATW_DATABASE_URL"));
        if (ConnInfo.IsEmpty())
        {
            UE_LOG(LogTemp, Error, TEXT("RATW_WORLD_REJECTED: set RATW_DATABASE_URL (tools/live.sh does this from Database/.env)."));
            return false;
        }
        std::string Problem;
        if (!WorldDb.connect(TCHAR_TO_UTF8(*ConnInfo), Problem))
        {
            UE_LOG(LogTemp, Error, TEXT("RATW_WORLD_REJECTED: cannot reach the %s database: %s"), *DatabaseName.ToUpper(), *F(Problem));
            return false;
        }
        const auto Build = WorldDb.exec(
            "SELECT w.id, b.id, coalesce(b.release, 0), b.files::text FROM world.worlds w "
            "JOIN world.builds b ON b.world_id = w.id ORDER BY b.id DESC LIMIT 1");
        if (!Build.ok || Build.rows.empty() || !Build.rows[0][3])
        {
            UE_LOG(LogTemp, Error, TEXT("RATW_WORLD_REJECTED: the %s database has no world build yet%s%s"), *DatabaseName.ToUpper(),
                   Build.ok ? TEXT(" (Push to live makes one for PROD; python3 tools/world_build.py dev for DEV)") : TEXT(": "),
                   Build.ok ? TEXT("") : *F(Build.error));
            return false;
        }
        const auto& Row = Build.rows[0];
        TSharedPtr<FJsonObject> Files;
        const auto Reader = TJsonReaderFactory<>::Create(FString(UTF8_TO_TCHAR(Row[3]->c_str())));
        if (!FJsonSerializer::Deserialize(Reader, Files) || !Files.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("RATW_WORLD_REJECTED: build %s is not readable."), *F(*Row[1]));
            return false;
        }
        WorldFiles.clear();
        for (const auto& Pair : Files->Values)
        {
            FString Text;
            if (!Pair.Value.IsValid() || !Pair.Value->TryGetString(Text))
            {
                UE_LOG(LogTemp, Error, TEXT("RATW_WORLD_REJECTED: build %s has a malformed file entry."), *F(*Row[1]));
                return false;
            }
            WorldFiles[S(Pair.Key)] = S(Text);
        }
        WorldFiles["world.ratw"] = WithoutPeople(WorldFiles["world.ratw"]);
        LiveWorldId = *Row[0];
        LoadedBuild = FCString::Atoi64(*F(*Row[1]));
        StreamedBuild = WorldFiles["world.ratw"].rfind("RATW_WORLD 3", 0) == 0;
        CellHeaders.clear();
        if (StreamedBuild)
        {
            const auto Headers = WorldDb.exec("SELECT cell_id, header FROM world.build_cells WHERE build_id = $1",
                                              {std::to_string(LoadedBuild)});
            if (!Headers.ok || Headers.rows.empty())
            {
                UE_LOG(LogTemp, Error, TEXT("RATW_WORLD_REJECTED: build %lld has no cells: %s"), LoadedBuild, *F(Headers.error));
                return false;
            }
            for (const auto& Cell : Headers.rows)
                if (Cell[0] && Cell[1])
                    CellHeaders[*Cell[0]] = *Cell[1];
            std::string PrefetchProblem;
            Prefetching = CellPrefetch.Start(TCHAR_TO_UTF8(*ConnInfo), std::to_string(LoadedBuild), PrefetchProblem);
            if (!Prefetching)
                UE_LOG(LogTemp, Warning, TEXT("RATW cells will not be fetched ahead (each loads when needed): %s"), *F(PrefetchProblem));
        }
        std::string Failure;
        if (!LoadWithLivePeople(World, Failure))
        {
            UE_LOG(LogTemp, Error, TEXT("RATW_WORLD_REJECTED: %s"), *F(Failure));
            return false;
        }
        WorldId = F(*Row[0]);
        std::string ListenProblem;
        if (!WorldDb.listen("ratw_release", ListenProblem))
            UE_LOG(LogTemp, Warning, TEXT("RATW will not hear about new releases: %s"), *F(ListenProblem));
        if (!WorldDb.listen("ratw_dm", ListenProblem))
            UE_LOG(LogTemp, Warning, TEXT("RATW will check for Dungeon Master actions once a minute: %s"), *F(ListenProblem));
        UE_LOG(LogTemp, Display, TEXT("RATW_DATABASE_WORLD_LOADED database=%s world=%s build=%lld release=%s cells=%d residents=%d streamed=%d loaded=%d"),
               *DatabaseName, *WorldId, LoadedBuild, *F(*Row[2]), static_cast<int32>(World.cells().size()),
               static_cast<int32>(World.society().state().residents.size()), StreamedBuild ? 1 : 0,
               static_cast<int32>(World.loadedCells()));
        return true;
    }

    /**
     * Dungeon Master actions (dm.actions, written by tools/dungeon_master.py): applied in order, each exactly once,
     * with the outcome written back. Actions nobody could apply within ten minutes expire rather than surprise later.
     */
    void ApplyDmActions(double Dt)
    {
        if (DatabaseName.IsEmpty() || (DmAccumulator += Dt) < 1)
            return;
        DmAccumulator = 0;
        ReadNotifications();
        // The database is asked only when the Dungeon Master has said there is something to do (NOTIFY ratw_dm), and
        // once a minute in case a notice was missed. Every round trip after a quiet spell can take tens of
        // milliseconds, and this runs on the game's own thread.
        const bool Minute = (DmExpiryAccumulator += 1) >= 60;
        if (!DmNotified && !Minute)
            return;
        DmNotified = false;
        if (Minute)
        {
            DmExpiryAccumulator = 0;
            WorldDb.exec("UPDATE dm.actions SET status = 'expired', result = 'Not applied within ten minutes.', done_at = now() "
                         "WHERE status = 'queued' AND requested_at < now() - interval '10 minutes'");
        }
        const auto Queued = WorldDb.exec("SELECT id, kind, target_id, requested_by FROM dm.actions WHERE status = 'queued' ORDER BY id LIMIT 50");
        bool Changed = false;
        for (const auto& Row : Queued.rows)
        {
            if (!Row[0] || !Row[1] || !Row[2])
                continue;
            const std::string Kind = *Row[1], Target = *Row[2];
            ratw::Result Outcome{false, "Unknown action.", {}};
            if (Kind == "layers.sync")
            {
                // Patrol routes or wander areas changed: take them from the world as the tables now read.
                ratw::World Candidate;
                std::string Problem;
                if (LoadWithLivePeople(Candidate, Problem))
                {
                    World.adoptLayers(Candidate);
                    Outcome = {true, "Routes and areas are updated.", {}};
                }
                else
                    Outcome = {false, "The routes and areas could not be applied: " + Problem, {}};
            }
            else if (Kind == "factions.sync")
            {
                // Factions or territory claims changed.
                ratw::World Candidate;
                std::string Problem;
                if (LoadWithLivePeople(Candidate, Problem))
                {
                    World.adoptFactions(Candidate);
                    Outcome = {true, "Factions and territory are updated.", {}};
                }
                else
                    Outcome = {false, "The factions could not be applied: " + Problem, {}};
            }
            else if (Kind == "npc.sync")
            {
                // The Dungeon Master changed this NPC's row: check the whole world as it now reads, then take them over.
                ratw::World Candidate;
                std::string Problem;
                Outcome = LoadWithLivePeople(Candidate, Problem) ? World.adoptResident(Candidate, Target)
                                                                  : ratw::Result{false, "The NPC could not be placed: " + Problem, {}};
            }
            else if (Kind == "npc.kill" || Kind == "npc.revive")
            {
                const auto* Npc = World.entity(Target);
                Outcome = Npc && Npc->npc ? World.setDead(Target, Kind == "npc.kill") : ratw::Result{false, "No such NPC.", {}};
            }
            else if (Kind == "character.kill" || Kind == "character.resurrect")
            {
                const bool Kill = Kind == "character.kill";
                if (auto* Online = World.entity(Target); Online && !Online->npc)
                {
                    Outcome = World.setDead(Target, Kill);
                    for (const auto& C : Clients)
                        if (C.IsValid() && S(C->EntityId) == Target && Outcome.ok)
                            System(C.Get(), Kill ? TEXT("You have died.") : TEXT("You have been brought back to life."));
                    if (Outcome.ok)
                        Characters[Target] = *Online;
                }
                else if (auto Saved = Characters.find(Target); Saved != Characters.end())
                {
                    auto& E = Saved->second;
                    if (E.dead == Kill)
                        Outcome = {false, E.name + (Kill ? " is already dead." : " is not dead."), {}};
                    else
                    {
                        E.dead = Kill;
                        E.posture = Kill ? "lying" : "standing";
                        E.state = E.activity = Kill ? "dead" : "";
                        Outcome = {true, E.name + (Kill ? " is dead (offline)." : " lives again (offline)."), {}};
                    }
                }
                else
                    Outcome = {false, "No such character.", {}};
            }
            Changed |= Outcome.ok;
            WorldDb.exec("UPDATE dm.actions SET status = $2, result = $3, done_at = now() WHERE id = $1 AND status = 'queued'",
                         {*Row[0], std::string(Outcome.ok ? "applied" : "refused"), Outcome.message});
            UE_LOG(LogTemp, Display, TEXT("RATW_DM_ACTION %s %s by %s: %s"), *F(Kind), *F(Target), *F(Row[3] ? *Row[3] : ""),
                   *F(Outcome.message));
            LogEvent("operator", Row[3] ? *Row[3] : "", Target, Kind + (Outcome.ok ? ": applied" : ": refused"));
        }
        if (Changed)
        {
            ++Revision;
            Save();
        }
    }

    /**
     * Spawn rules (live.spawns): keep `count` NPCs made from a template alive in the rule's area. A newcomer arrives on a
     * random open tile of the area and roams it. A fallen spawned NPC is cleared once `respawn_minutes` have passed, and
     * a new one takes their place. A rule that fails backs off for five minutes.
     */
    // Residents the society wants (Society::takeRequests via World::takeResidentRequests): a stranger for a post
    // nobody here could fill, or a child. A live world adds them to live.npcs, cloned from the template resident with
    // their own name, age and home and no job of their own (work label "-"), takes them in like any spawn, then gives
    // them their place (World::welcomeResident). A world from files can't keep new people, so none are made there.
    void MakeResidents()
    {
        for (const auto& Request : World.takeResidentRequests())
        {
            if (DatabaseName.IsEmpty())
            {
                UE_LOG(LogTemp, Display, TEXT("RATW_RESIDENT %s wanted (%s), but a world from files can't add residents."),
                       *F(Request.kind), *F(Request.name));
                continue;
            }
            const auto* Model = World.society().spec(Request.templateId);
            const auto* ModelLife = World.society().resident(Request.templateId);
            if (!Model || !ModelLife)
                continue;
            std::string Id;
            for (int Attempt = 0; Id.empty() && Attempt < 50; ++Attempt)
            {
                std::string Suffix;
                for (auto Stamp = uint64(FDateTime::UtcNow().ToUnixTimestamp()) * 64 + uint64(Attempt); Stamp; Stamp /= 36)
                    Suffix.insert(Suffix.begin(), "0123456789abcdefghijklmnopqrstuvwxyz"[Stamp % 36]);
                const auto Candidate = (Request.kind == "birth" ? std::string("born_") : std::string("new_")) + Suffix;
                const auto Taken = WorldDb.exec("SELECT 1 FROM live.npcs WHERE world_id = $1 AND id = $2 UNION ALL "
                                                "SELECT 1 FROM live.npc_state WHERE world_id = $1 AND npc_id = $2", {LiveWorldId, Candidate});
                if (Taken.ok && Taken.rows.empty() && !World.entity(Candidate))
                    Id = Candidate;
            }
            if (Id.empty())
                continue;
            const auto Home = Request.home.cell.empty() ? ratw::Spot{ModelLife->homeCell, ModelLife->homeX, ModelLife->homeY} : Request.home;
            const bool Child = Request.kind == "birth";
            const auto* JobFor = World.society().position(Request.positionId);
            const std::string Description = Child
                ? "A young wolf, born here to " + Model->name + "'s family."
                : "A stranger lately come to town" + (JobFor ? " to take up " + JobFor->title : std::string()) + ".";
            const auto X = std::to_string(int(std::floor(Home.x))), Y = std::to_string(int(std::floor(Home.y)));
            const auto Made = WorldDb.exec(
                "INSERT INTO live.npcs (world_id, id, position, name, role, description, greeting, personality, backstory, work_label, age, voice, "
                "appearance, route_id, paid, purse, herbs, meals, hours_start, hours_end, home_area, home_x, home_y, work_area, work_x, work_y, "
                "evening_area, evening_x, evening_y, origin, wander_area, spawn_id) "
                "SELECT world_id, $2, (SELECT coalesce(max(position) + 1, 0) FROM live.npcs WHERE world_id = $1), $3, 'civilian', $4, "
                "'Hello.', personality, $5, '-', $6::integer, voice, appearance, NULL, false, $7::integer, 0, 1, hours_start, hours_end, "
                "$8, $9::integer, $10::integer, $8, $9::integer, $10::integer, $8, $9::integer, $10::integer, 'runtime', NULL, NULL "
                "FROM live.npcs WHERE world_id = $1 AND id = $11",
                {LiveWorldId, Id, Request.name, Description, Child ? std::string("Born in the world, not written.") : std::string(),
                 std::to_string(Request.age), Child ? std::string("0") : std::string("20"), Home.cell, X, Y, Request.templateId});
            ratw::World Candidate;
            std::string Problem;
            const bool Loaded = Made.ok && LoadWithLivePeople(Candidate, Problem);
            const auto Adopted = Loaded ? World.adoptResident(Candidate, Id) : ratw::Result{false, Made.ok ? Problem : Made.error, {}};
            if (!Adopted.ok)
            {
                WorldDb.exec("DELETE FROM live.npcs WHERE world_id = $1 AND id = $2", {LiveWorldId, Id});
                UE_LOG(LogTemp, Warning, TEXT("RATW_RESIDENT %s could not be made: %s"), *F(Request.name), *F(Adopted.message));
                continue;
            }
            const auto Welcomed = World.welcomeResident(Request, Id);
            UE_LOG(LogTemp, Display, TEXT("RATW_RESIDENT %s: %s (%s) %s"), *F(Request.kind), *F(Request.name), *F(Id), *F(Welcomed.message));
            ++Revision;
            SaveSoon();
        }
    }

    void RunSpawns(double Dt)
    {
        if (DatabaseName.IsEmpty() || (SpawnAccumulator += Dt) < 30)   // Respawn times are minutes: no need to ask often.
            return;
        SpawnAccumulator = 0;
        const double Clock = FPlatformTime::Seconds();
        const auto Rules = WorldDb.exec(
            "SELECT s.id, s.name, s.template_id, s.count, s.respawn_minutes, a.id, a.area, a.tiles::text, "
            "coalesce((SELECT string_agg(n.id, ',' ORDER BY n.id) FROM live.npcs n WHERE n.world_id = s.world_id AND n.spawn_id = s.id), '') "
            "FROM live.spawns s JOIN live.npc_areas a ON a.world_id = s.world_id AND a.id = s.area_id "
            "WHERE s.world_id = $1 AND s.enabled AND EXISTS (SELECT 1 FROM live.npcs t WHERE t.world_id = s.world_id AND t.id = s.template_id) "
            "ORDER BY s.position",
            {LiveWorldId});
        bool Changed = false;
        for (const auto& Row : Rules.rows)
        {
            const std::string Rule = *Row[0], Name = *Row[1], Template = *Row[2], WanderArea = *Row[5], Area = *Row[6];
            const int Count = std::stoi(*Row[3]);
            const double Respawn = std::stod(*Row[4]) * 60;
            if (SpawnBackoff.count(Rule) && SpawnBackoff[Rule] > Clock)
                continue;
            std::vector<std::string> Alive, Dead;
            std::stringstream Ids(*Row[8]);
            for (std::string Id; std::getline(Ids, Id, ',');)
                if (const auto* E = World.entity(Id))
                {
                    if (!E->dead) { Alive.push_back(Id); DeadSince.erase(Id); }
                    else { Dead.push_back(Id); DeadSince.emplace(Id, Clock); }
                }
            if (int(Alive.size()) >= Count)
                continue;
            // The oldest fallen body whose respawn time has passed is cleared first.
            std::string Cleared;
            for (const auto& Id : Dead)
                if (Clock - DeadSince[Id] >= Respawn)
                {
                    Cleared = Id;
                    break;
                }
            if (!Dead.empty() && Cleared.empty() && int(Alive.size() + Dead.size()) >= Count)
                continue;                         // Waiting out the respawn time.
            if (!Cleared.empty())
            {
                WorldDb.exec("DELETE FROM live.npc_state WHERE world_id = $1 AND npc_id = $2", {LiveWorldId, Cleared});
                WorldDb.exec("DELETE FROM live.npcs WHERE world_id = $1 AND id = $2", {LiveWorldId, Cleared});
                ratw::World Candidate;
                std::string Problem;
                if (LoadWithLivePeople(Candidate, Problem))
                    World.adoptResident(Candidate, Cleared);
                DeadSince.erase(Cleared);
                UE_LOG(LogTemp, Display, TEXT("RATW_SPAWN %s cleared the fallen %s"), *F(Rule), *F(Cleared));
                LogEvent("cleared", Cleared, {}, "spawn rule " + Rule);
                Changed = true;
            }
            const auto Spot = SpawnTile(Area, *Row[7]);
            if (!Spot)
            {
                SpawnBackoff[Rule] = Clock + 300;
                UE_LOG(LogTemp, Warning, TEXT("RATW_SPAWN %s: no open tile in its area; trying again in five minutes."), *F(Rule));
                continue;
            }
            // A newcomer is someone new: their ID is never reused (memories and history hang on it), while the name takes
            // the lowest free number among those the rule keeps now ("Rat catchers 2").
            std::string Id;
            for (int Attempt = 0; Id.empty(); ++Attempt)
            {
                std::string Suffix;
                for (auto Stamp = uint64(FDateTime::UtcNow().ToUnixTimestamp()) + Attempt; Stamp; Stamp /= 36)
                    Suffix.insert(Suffix.begin(), "0123456789abcdefghijklmnopqrstuvwxyz"[Stamp % 36]);
                const auto Candidate = Rule + "_" + Suffix;
                const auto Taken = WorldDb.exec("SELECT 1 FROM live.npcs WHERE world_id = $1 AND id = $2 UNION ALL "
                                                "SELECT 1 FROM live.npc_state WHERE world_id = $1 AND npc_id = $2", {LiveWorldId, Candidate});
                if (Taken.ok && Taken.rows.empty() && !World.entity(Candidate))
                    Id = Candidate;
                else if (!Taken.ok || Attempt > 50)
                    break;
            }
            if (Id.empty())
            {
                SpawnBackoff[Rule] = Clock + 300;
                continue;
            }
            const auto NameTaken = [&](int N) {
                return std::any_of(Alive.begin(), Alive.end(), [&](const std::string& Other) {
                    const auto* E = World.entity(Other);
                    return E && E->name == Name + " " + std::to_string(N);
                });
            };
            int Number = 1;
            while (NameTaken(Number))
                ++Number;
            const auto X = std::to_string(Spot->first), Y = std::to_string(Spot->second);
            const auto Made = WorldDb.exec(
                "INSERT INTO live.npcs (world_id, id, position, name, role, description, greeting, personality, backstory, work_label, age, voice, "
                "appearance, route_id, paid, purse, herbs, meals, hours_start, hours_end, home_area, home_x, home_y, work_area, work_x, work_y, "
                "evening_area, evening_x, evening_y, origin, wander_area, spawn_id) "
                "SELECT world_id, $2, (SELECT coalesce(max(position) + 1, 0) FROM live.npcs WHERE world_id = $1), $3, role, description, greeting, "
                "personality, backstory, work_label, age, voice, appearance, route_id, paid, purse, herbs, meals, hours_start, hours_end, "
                "$4, $5::integer, $6::integer, $4, $5::integer, $6::integer, $4, $5::integer, $6::integer, 'runtime', $7, $8 "
                "FROM live.npcs WHERE world_id = $1 AND id = $9",
                {LiveWorldId, Id, Name + " " + std::to_string(Number), Area, X, Y, WanderArea, Rule, Template});
            ratw::World Candidate;
            std::string Problem;
            const bool Loaded = Made.ok && LoadWithLivePeople(Candidate, Problem);
            const auto Outcome = Loaded ? World.adoptResident(Candidate, Id) : ratw::Result{false, Made.ok ? Problem : Made.error, {}};
            if (!Outcome.ok)
            {
                WorldDb.exec("DELETE FROM live.npcs WHERE world_id = $1 AND id = $2", {LiveWorldId, Id});
                SpawnBackoff[Rule] = Clock + 300;
                UE_LOG(LogTemp, Warning, TEXT("RATW_SPAWN %s failed; trying again in five minutes: %s"), *F(Rule), *F(Outcome.message));
                continue;
            }
            Changed = true;
            UE_LOG(LogTemp, Display, TEXT("RATW_SPAWN %s: %s arrives in %s at %s, %s"), *F(Rule), *F(Id), *F(Area), *F(X), *F(Y));
            LogEvent("spawn", Id, {}, "spawn rule " + Rule);
        }
        if (Changed)
        {
            ++Revision;
            Save();
        }
    }

    /** A random open tile of a spawn area ([[x, y], ...] in `Area`) with nobody standing on it. */
    TOptional<std::pair<int, int>> SpawnTile(const std::string& Area, const std::string& TilesJson)
    {
        TArray<TSharedPtr<FJsonValue>> Tiles;
        if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(F(TilesJson)), Tiles))
            return {};
        if (!World.ensureLoaded(Area).ok)
            return {};
        const auto* Cell = World.cell(Area);
        if (!Cell)
            return {};
        std::vector<std::pair<int, int>> Open;
        for (const auto& Value : Tiles)
        {
            const auto* Pair = Value.IsValid() ? &Value->AsArray() : nullptr;
            if (!Pair || Pair->Num() != 2)
                continue;
            const int X = static_cast<int>((*Pair)[0]->AsNumber()), Y = static_cast<int>((*Pair)[1]->AsNumber());
            const auto* Tile = Cell->tile(X, Y);
            bool Taken = false;
            for (const auto& E : World.entities())
                Taken |= E.second.cellId == Area && std::abs(E.second.position.x - (X + .5)) < .8 && std::abs(E.second.position.y - (Y + .5)) < .8;
            if (Tile && !Tile->solid && !Taken)
                Open.emplace_back(X, Y);
        }
        if (Open.empty())
            return {};
        return Open[FMath::RandRange(0, int(Open.size()) - 1)];
    }

    /** The build's manifest without NPC records (economy, route, resident, story) and without factions and their claims:
     *  those come from the live tables (live.people_manifest). Territory records keep their region and Chapter. */
    static std::string WithoutPeople(const std::string& Manifest)
    {
        std::istringstream In(Manifest);
        std::string Line, Out;
        bool First = true;
        while (std::getline(In, Line))
        {
            if (First)
            {
                First = false;
                if (Line == "RATW_WORLD 1")
                    Line = "RATW_WORLD 2";          // The version that allows NPC records, which are added next.
            }
            else if (Line.rfind("economy ", 0) == 0 || Line.rfind("route ", 0) == 0 || Line.rfind("resident ", 0) == 0 ||
                     Line.rfind("story ", 0) == 0 || Line.rfind("faction ", 0) == 0)
                continue;
            else if (Line.rfind("territory ", 0) == 0)
            {
                std::istringstream Fields(Line.substr(10));
                std::string Id, Region, Chapter;
                if (Fields >> std::quoted(Id) >> std::quoted(Region) >> std::quoted(Chapter))
                {
                    std::ostringstream Kept;
                    Kept << "territory " << std::quoted(Id) << ' ' << std::quoted(Region) << ' ' << std::quoted(Chapter) << " 0";
                    Line = Kept.str();
                }
            }
            Out += Line + "\n";
        }
        return Out;
    }

    /** Where a streamed build's cells come from: headers read at start, each cell's file and seams from the database
     *  when someone comes near it. The build is fixed for the life of this server (a new release restarts it). */
    ratw::World::CellSource CellSource()
    {
        ratw::World::CellSource Source;
        Source.header = [this](const std::string& Id, std::string& Header) {
            const auto Found = CellHeaders.find(Id);
            if (Found == CellHeaders.end())
                return std::string("The build has no cell ") + Id + ".";
            Header = Found->second;
            return std::string();
        };
        const auto Build = std::to_string(LoadedBuild);
        Source.load = [this, Build](const std::string& Id, std::string& Body, std::string& Seams) {
            if (Prefetching && CellPrefetch.Take(Id, Body, Seams))
                return std::string();
            const auto Cell = WorldDb.exec("SELECT body, seams FROM world.build_cells WHERE build_id = $1 AND cell_id = $2", {Build, Id});
            if (!Cell.ok || Cell.rows.empty() || !Cell.rows[0][0])
                return "Cannot read cell " + Id + " of build " + Build + ": " + (Cell.ok ? std::string("missing") : Cell.error);
            Body = *Cell.rows[0][0];
            Seams = Cell.rows[0][1] ? *Cell.rows[0][1] : "";
            return std::string();
        };
        return Source;
    }

    /** Loads the world build plus the NPCs as the live tables define them now, through the validated loader. */
    bool LoadWithLivePeople(ratw::World& Into, std::string& Problem)
    {
        const auto People = WorldDb.exec("SELECT live.people_manifest($1)", {LiveWorldId});
        if (!People.ok || People.rows.empty() || !People.rows[0][0])
        {
            Problem = "Cannot read the NPCs from the live tables: " + People.error;
            return false;
        }
        auto Files = WorldFiles;
        Files["world.ratw"] += *People.rows[0][0] + "\n";
        if (StreamedBuild)
            Into.setCellSource(CellSource());
        const auto Loaded = Into.loadWorldFiles(Files, "live world");
        Problem = Loaded.message;
        return Loaded.ok;
    }

    /** Notices from the database, read without a round trip: new releases and Dungeon Master actions. */
    void ReadNotifications()
    {
        for (const auto& Note : WorldDb.notifications())
        {
            if (Note.first == "ratw_dm")
            {
                DmNotified = true;
                continue;
            }
            TSharedPtr<FJsonObject> Payload;
            const auto Reader = TJsonReaderFactory<>::Create(F(Note.second));
            if (Note.first != "ratw_release" || !FJsonSerializer::Deserialize(Reader, Payload) || !Payload.IsValid())
                continue;
            const int64 Build = static_cast<int64>(Payload->GetNumberField(TEXT("build")));
            if (Build > LoadedBuild)
            {
                PendingRelease = static_cast<int64>(Payload->GetNumberField(TEXT("release")));
                ReleaseAnnounced = false;
                UE_LOG(LogTemp, Display, TEXT("RATW_RELEASE_PUBLISHED release=%lld build=%lld"), PendingRelease, Build);
            }
        }
    }

    /**
     * A new release arrived (NOTIFY ratw_release). The server tells anyone playing, and once nobody is connected
     * it saves and exits with status 75; tools/live.sh restarts it, loading the new build and the saved state.
     */
    void WatchReleases(double Dt)
    {
        if (DatabaseName.IsEmpty() || (ReleaseAccumulator += Dt) < 2)
            return;
        ReleaseAccumulator = 0;
        ReadNotifications();
        if (!PendingRelease)
            return;
        Clients.RemoveAll([](const TWeakObjectPtr<ARatwPlayerController>& P) { return !P.IsValid(); });
        if (Clients.Num() == 0)
        {
            Save();
            UE_LOG(LogTemp, Display, TEXT("RATW_RELEASE_RESTART release=%lld; saved, exiting so the new world loads."), PendingRelease);
            PendingRelease = 0;
            FPlatformMisc::RequestExitWithStatus(false, 75);
        }
        else if (!ReleaseAnnounced)
        {
            ReleaseAnnounced = true;
            for (const auto& C : Clients)
                if (C.IsValid())
                    System(C.Get(), FString::Printf(TEXT("A new version of the world (release %lld) has been published. It takes effect after a short restart once everyone has left."), PendingRelease));
        }
    }

    void Send(ARatwPlayerController* C, const Object& E)
    {
        if (C)
            C->ClientEvent(Encode(E));
    }
    void System(ARatwPlayerController* C, const FString& Message)
    {
        auto E = New();
        E->SetStringField(TEXT("type"), TEXT("system"));
        E->SetStringField(TEXT("speaker"), TEXT("World"));
        E->SetStringField(TEXT("text"), Message);
        E->SetNumberField(TEXT("sequence"), Sequence++);
        E->SetNumberField(TEXT("color"), 7);
        if (C)
        {
            const auto Id = S(C->EntityId);
            const auto Command = CurrentCommands.find(Id);
            if (Command != CurrentCommands.end() && !Command->second.empty())
                ResponseReceipts[Id][Command->second] = Encode(E);
        }
        Send(C, E);
    }

    void Lobby(ARatwPlayerController* C, bool Ok = true, const FString& Message = FString())
    {
        if (!C) return;
        if (!C->EntityId.IsEmpty())
        {
            System(C, Message.IsEmpty() ? TEXT("Leave your character before returning to account selection.") : Message);
            return;
        }
        auto Event = New();
        Event->SetStringField(TEXT("type"), TEXT("lobby"));
        Event->SetStringField(TEXT("stage"), C->AccountUsername.IsEmpty() ? TEXT("login") : TEXT("characters"));
        Event->SetBoolField(TEXT("ok"), Ok);
        Event->SetBoolField(TEXT("localOnly"), true);
        Event->SetBoolField(TEXT("credentialsAllowed"), C->AllowsLocalCredentials());
        Event->SetStringField(TEXT("message"), Message.IsEmpty()
            ? TEXT("Local development accounts only. Native networking is not encrypted; use a unique test password.") : Message);
        Event->SetNumberField(TEXT("slotsLimit"), FRatwAccounts::CharacterSlots);
        Array Roster;
        if (!C->AccountUsername.IsEmpty())
            for (const auto& Id : Accounts.Characters(C->AccountUsername))
            {
                const auto It = Characters.find(S(Id));
                if (It == Characters.end()) continue;
                auto Character = It->second;
                ratw::advanceAge(Character, World.calendarDays());
                auto Item = New();
                Item->SetStringField(TEXT("id"), Id);
                Text(Item, TEXT("name"), Character.name);
                Item->SetNumberField(TEXT("age"), Character.age);
                Item->SetObjectField(TEXT("appearance"), ratwjson::Appearance(Character.appearance));
                Roster.Add(V(Item));
            }
        Event->SetArrayField(TEXT("characters"), Roster);
        Send(C, Event);
    }

    bool EnterCharacter(ARatwPlayerController* C, const std::string& Actor, const FString& Name,
                        const FString& DevelopmentId = FString())
    {
        for (const auto& Other : Clients)
            if (Other.IsValid() && Other.Get() != C && S(Other->EntityId) == Actor)
            {
                Lobby(C, false, TEXT("That character is already connected. Leave it on the other client first."));
                return false;
            }
        const auto BeforeWorld = World;
        const auto BeforeCharacters = Characters;
        auto& Player = World.addPlayer(Actor, S(Name));
        if (!World.society().account(Actor))
        {
            World = BeforeWorld;
            Lobby(C, false, TEXT("Character entry could not allocate a valid economy account; no world change was saved."));
            return false;
        }
        const auto Saved = Characters.find(Actor);
        if (Saved != Characters.end()) Player = Saved->second;
        else
        {
            Player.description = "A road-worn quadrupedal wolf with a small shoulder satchel. Their coat and history are yours to imagine.";
            Player.speakingColor = static_cast<int>(Characters.size() * 9) % 32;
        }
        Player.input = {}; Player.velocity = {}; Player.path.clear();
        Player.turning = false; Player.turnTarget = Player.facing;
        if (Player.posture == "rising") Player.posture = Player.postureTarget;
        Player.postureRemaining = 0; Player.postureTarget.clear();
        Player.typing = false; Player.speakingUntil = 0;
        ratw::advanceAge(Player, World.calendarDays());
        Characters[Actor] = Player;
        World.observe(Actor);
        LogEvent("arrival", Actor);
        ++Revision;
        Save();
        if (!StorageReady)
        {
            World = BeforeWorld; Characters = BeforeCharacters;
            Lobby(C, false, TEXT("Character entry could not be saved; the world remains closed."));
            return false;
        }
        C->EntityId = F(Actor);
        C->DevelopmentIdentity = DevelopmentId;
        C->MotionSession = FGuid::NewGuid().ToString(EGuidFormats::Digits);
        C->MotionCell.Empty(); C->MotionGeneration = 0;
        C->ResetSections();                            // A new session: the client starts with nothing kept.
        auto Entered = New();
        Entered->SetStringField(TEXT("type"), TEXT("entered"));
        Entered->SetStringField(TEXT("id"), C->EntityId);
        Entered->SetStringField(TEXT("motionSession"), C->MotionSession);
        Send(C, Entered);
        System(C, TEXT("Connected to ") + F(World.cell(Player.cellId)->name) +
                      TEXT(". Enter to write; Shift+Enter for a new line; Escape preserves your draft. "
                           "Dialogue is authored offline unless a local provider is configured."));
        Snapshot(C);
        UE_LOG(LogTemp, Display, TEXT("RATW_LOGIN %s connected=%d"), *C->EntityId, Clients.Num());
        return true;
    }

    void LeaveCharacter(ARatwPlayerController* C)
    {
        const auto Id = S(C->EntityId);
        const bool HadCharacter = !Id.empty();
        if (auto* E = World.entity(Id))
        {
            E->typing = false; World.stop(E->id);
            Characters[E->id] = *E;
            LogEvent("departure", Id);
            World.removePlayer(Id);
            ++Revision;
        }
        LastMovementSound.erase(Id); TypingExpiry.erase(Id); CurrentCommands.erase(Id);
        C->EntityId.Empty(); C->DevelopmentIdentity.Empty();
        if (HadCharacter) Save();
    }

    bool AccountCommand(ARatwPlayerController* C, const Object& J, const FString& Type)
    {
        if (Type != TEXT("auth_register") && Type != TEXT("auth_login") && Type != TEXT("character_create") &&
            Type != TEXT("character_enter") && Type != TEXT("character_leave") && Type != TEXT("auth_logout")) return false;
        if (Type == TEXT("auth_logout") || Type == TEXT("character_leave"))
        {
            LeaveCharacter(C);
            if (Type == TEXT("auth_logout")) C->AccountUsername.Empty();
            Lobby(C, StorageReady, StorageReady ? TEXT("Your character has left the world.") : TEXT("Storage failed; the character was removed from play, but the latest state could not be saved."));
            return true;
        }
        if (!C->AllowsLocalCredentials())
        {
            Lobby(C, false, TEXT("Account access is disabled for remote peers. Native transport is unencrypted; connect to loopback on this computer."));
            return true;
        }
        if (!StorageReady)
        {
            Lobby(C, false, TEXT("Account access is unavailable because world storage is not healthy."));
            return true;
        }
        TSet<FString> Allowed = {TEXT("type"), TEXT("commandId")};
        if (Type == TEXT("auth_register") || Type == TEXT("auth_login"))
        {
            Allowed.Add(TEXT("username")); Allowed.Add(TEXT("password"));
        }
        else if (Type == TEXT("character_create"))
        {
            Allowed.Add(TEXT("name")); Allowed.Add(TEXT("age")); Allowed.Add(TEXT("appearance"));
        }
        else if (Type == TEXT("character_enter")) Allowed.Add(TEXT("id"));
        for (const auto& Field : J->Values)
            if (!Allowed.Contains(FString(Field.Key.ToView())))
            {
                Lobby(C, false, TEXT("The account request contains unsupported fields.")); return true;
            }
        if (!C->EntityId.IsEmpty())
        {
            if (Type == TEXT("character_enter") && String(J, TEXT("id")) == C->EntityId && Accounts.Owns(C->AccountUsername, C->EntityId))
            {
                auto Entered = New(); Entered->SetStringField(TEXT("type"), TEXT("entered"));
                Entered->SetStringField(TEXT("id"), C->EntityId);
                Entered->SetStringField(TEXT("motionSession"), C->MotionSession);
                C->ResetSections();
                Send(C, Entered); Snapshot(C); return true;
            }
            System(C, TEXT("Leave the current character before changing account or character selection."));
            return true;
        }
        if (Type == TEXT("auth_register") || Type == TEXT("auth_login"))
        {
            if (!C->AccountUsername.IsEmpty())
            {
                Lobby(C, false, TEXT("Log out before signing into another account.")); return true;
            }
            if (!AuthRate.Allow(FString::FromInt(C->GetUniqueID()), FPlatformTime::Seconds()))
            {
                Lobby(C, false, TEXT("Too many account attempts. Wait up to a minute before trying again.")); return true;
            }
            FString User, Password;
            const auto* UserValue = J->Values.Find(TEXT("username"));
            const auto* PasswordValue = J->Values.Find(TEXT("password"));
            if (!UserValue || !PasswordValue || !UserValue->IsValid() || !PasswordValue->IsValid() ||
                (*UserValue)->Type != EJson::String || (*PasswordValue)->Type != EJson::String ||
                !(*UserValue)->TryGetString(User) || !(*PasswordValue)->TryGetString(Password) ||
                !FRatwAccounts::NormalizeUsername(User, User) || !FRatwAccounts::ValidPassword(Password))
            {
                Lobby(C, false, TEXT("Use a 3–32 character username starting with a letter (letters, digits, _ or -) and a 12–128 byte password without control characters.")); return true;
            }
            if (Type == TEXT("auth_register"))
            {
                const auto Before = Accounts;
                FString Error;
                if (!Accounts.Register(User, Password, Error)) { Lobby(C, false, Error); return true; }
                ++Revision; Save();
                if (!StorageReady)
                {
                    Accounts = Before;
                    Lobby(C, false, TEXT("Account creation could not be saved. No account was acknowledged.")); return true;
                }
            }
            else if (!Accounts.Authenticate(User, Password))
            {
                Lobby(C, false, TEXT("Username or password was not accepted.")); return true;
            }
            C->AccountUsername = User;
            Lobby(C, true, Type == TEXT("auth_register") ? TEXT("Account saved. Create your first character.") : TEXT("Signed in. Choose a character or create one."));
            return true;
        }
        if (C->AccountUsername.IsEmpty() || !Accounts.Exists(C->AccountUsername))
        {
            Lobby(C, false, TEXT("Sign in before managing characters.")); return true;
        }
        if (Type == TEXT("character_create"))
        {
            const FString Name = String(J, TEXT("name")).TrimStartAndEnd();
            const double Age = StrictNumber(J, TEXT("age"), -1);
            ratw::Appearance Appearance;
            const FString CommandId = String(J, TEXT("commandId"));
            if (!FRatwAccounts::ValidDisplayName(Name) || Age < 6 || Age > 99 || Age != FMath::FloorToDouble(Age) ||
                !ratwjson::ReadAppearance(Child(J, TEXT("appearance")), Appearance) ||
                !FRatwAccounts::ValidCommandId(CommandId))
            {
                Lobby(C, false, TEXT("Choose a 2–32 character name, a whole age from 6–99, and a complete valid wolf appearance.")); return true;
            }
            auto Canonical = New();
            Canonical->SetStringField(TEXT("name"), Name); Canonical->SetNumberField(TEXT("age"), Age);
            Canonical->SetObjectField(TEXT("appearance"), ratwjson::Appearance(Appearance));
            const FString Fingerprint = FRatwAccounts::Fingerprint(Encode(Canonical));
            bool Conflict = false;
            const FString Previous = Accounts.CreatedCharacter(C->AccountUsername, CommandId, Fingerprint, Conflict);
            if (Conflict) { Lobby(C, false, TEXT("That creation request ID was already used for different choices.")); return true; }
            if (!Previous.IsEmpty()) { Lobby(C, true, TEXT("That character was already saved; no duplicate was created.")); return true; }
            if (Accounts.Characters(C->AccountUsername).Num() >= FRatwAccounts::CharacterSlots)
            {
                Lobby(C, false, TEXT("This local account already has six characters. Deletion is not available.")); return true;
            }
            const auto BeforeWorld = World;
            const auto BeforeCharacters = Characters;
            const auto BeforeAccounts = Accounts;
            const FString NewId = TEXT("wolf-") + FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
            if (Characters.count(S(NewId)) || !Accounts.AddCharacter(C->AccountUsername, NewId, CommandId, Fingerprint))
            {
                Lobby(C, false, TEXT("Character ownership could not be allocated; please try again.")); return true;
            }
            auto& Player = World.addPlayer(S(NewId), S(Name));
            if (!World.society().account(S(NewId)))
            {
                World = BeforeWorld; Accounts = BeforeAccounts;
                Lobby(C, false, TEXT("Character creation could not allocate a valid economy account; no character was saved.")); return true;
            }
            Player.age = int(Age); Player.lastBirthdayDay = World.calendarDays();
            Player.ageNoticePending = 0; Player.appearance = Appearance;
            Player.speakingColor = static_cast<int>(Characters.size() * 9) % 32;
            Characters[Player.id] = Player;
            World.removePlayer(S(NewId));
            LogEvent("character created", Player.id);
            ++Revision; Save();
            if (!StorageReady)
            {
                World = BeforeWorld; Characters = BeforeCharacters; Accounts = BeforeAccounts;
                Lobby(C, false, TEXT("Character creation could not be saved. No character was acknowledged.")); return true;
            }
            Lobby(C, true, TEXT("Character saved. Select it to enter the world."));
            return true;
        }
        const FString Id = String(J, TEXT("id"));
        if (!Accounts.Owns(C->AccountUsername, Id) || !Characters.count(S(Id)))
        {
            Lobby(C, false, TEXT("That character is not available on this account.")); return true;
        }
        EnterCharacter(C, S(Id), F(Characters.at(S(Id)).name));
        return true;
    }
    void Login(ARatwPlayerController* C, const Object& Command)
    {
        if (!DevIdentity)
        {
            Lobby(C, false, TEXT("Development identity entry is disabled. Register or sign in to a local account.")); return;
        }
        if (!StorageReady) { Lobby(C, false, TEXT("World storage is unavailable.")); return; }
        if (!C->EntityId.IsEmpty())
            return;
        FString Id = String(Command, TEXT("id"), TEXT("ash")).ToLower();
        if (Id.Len() < 2 || Id.Len() > 32)
        {
            System(C, TEXT("Development identity must be 2–32 letters, digits, hyphens or underscores."));
            return;
        }
        for (TCHAR Ch : Id)
            if (!FChar::IsAlnum(Ch) || Ch > 127)
            {
                if (Ch != TEXT('-') && Ch != TEXT('_'))
                {
                    System(C, TEXT("Invalid development identity."));
                    return;
                }
            }
        const std::string Actor = S(TEXT("player-") + Id);
        FString Name = String(Command, TEXT("name"), Id).Left(32).TrimStartAndEnd();
        if (Name.IsEmpty())
            Name = Id;
        EnterCharacter(C, Actor, Name, Id);
    }

    void Disconnect(ARatwPlayerController* C)
    {
        LeaveCharacter(C);
        C->AccountUsername.Empty();
        AuthRate.Forget(FString::FromInt(C->GetUniqueID()));
        Clients.RemoveAll([C](const TWeakObjectPtr<ARatwPlayerController>& P) { return !P.IsValid() || P.Get() == C; });
    }
    void Tick(double Dt)
    {
        std::map<std::string, std::string> BeforeCells;
        for (const auto& C : Clients)
            if (C.IsValid())
                if (const auto* E = World.entity(S(C->EntityId)))
                    BeforeCells[E->id] = E->cellId;
        World.tick(Dt);
        ++Revision;
        // What happened to players that no action of theirs answered (a bandit's blow, a caravan arriving...).
        for (const auto& [Who, Words] : World.takeNotices())
            for (const auto& C : Clients)
                if (C.IsValid() && S(C->EntityId) == Who)
                    System(C.Get(), F(Words));
        for (const auto& Pair : BeforeCells)
            if (const auto* E = World.entity(Pair.first))
                if (E->cellId != Pair.second)
                    FollowTransition(Pair.first, Pair.second);
        for (auto& Entry : TypingExpiry)
            if (Entry.second <= World.time())
                if (auto* E = World.entity(Entry.first))
                    E->typing = false;
        for (const auto& Owner : CompanionOwner)
        {
            auto* Npc = World.entity(Owner.first);
            const auto* Leader = World.entity(Owner.second);
            if (!Npc || !Leader)
                continue;
            Npc->activity = "travelling with a companion";
            if (Npc->cellId == Leader->cellId)
            {
                const double Dist =
                    std::hypot(Npc->position.x - Leader->position.x, Npc->position.y - Leader->position.y);
                if (Dist > 2.7 && Npc->path.empty())
                    World.moveTo(Npc->id, Leader->position.x - 0.9, Leader->position.y + 0.7);
                if (Dist < 1.4)
                    World.stop(Npc->id);
            }
        }
        SnapshotAccumulator += Dt;
        // Small observer-filtered poses at simulation cadence. Full map/sheet
        // snapshots stay at 5 Hz; a room change gets fresh metadata immediately.
        for (const auto& C : Clients)
            if (C.IsValid())
                if (const auto* E = World.entity(S(C->EntityId)))
                {
                    if (C->MotionCell != F(E->cellId)) Snapshot(C.Get());
                    auto Motion = ratwmotion::Frame(World, E->id);
                    StampFrame(C.Get(), Motion, E->cellId);
                    C->ClientMotion(Motion);
                }
        SaveAccumulator += Dt;
        AmbientAccumulator += Dt;
        if (SnapshotAccumulator >= 0.2)
        {
            SnapshotAccumulator = 0;
            MovementSounds();
        }
        // Each client's full snapshot comes five times a second, a quarter of the clients in each 20 Hz tick
        // (by a phase fixed per connection), so no one tick builds every client's view at once.
        SnapshotPhase = (SnapshotPhase + 1) % SnapshotPhases;
        {
            std::vector<std::string> Due;             // Their sight is worked out together, on several threads.
            for (const auto& C : Clients)
                if (C.IsValid() && !C->EntityId.IsEmpty() && C->GetUniqueID() % SnapshotPhases == SnapshotPhase)
                    Due.push_back(S(C->EntityId));
            World.prepareViews(Due);
        }
        for (const auto& C : Clients)
            if (C.IsValid() && !C->EntityId.IsEmpty() && C->GetUniqueID() % SnapshotPhases == SnapshotPhase)
            {
                auto* E = World.entity(S(C->EntityId));
                if (E && E->ageNoticePending > 0)
                {
                    System(C.Get(), FString::Printf(TEXT("A birthday has passed. You are now %d years old (%d year%s gained). Your character sheet reflects annual growth and age-related changes."), E->age, E->ageNoticePending, E->ageNoticePending == 1 ? TEXT("") : TEXT("s")));
                    E->ageNoticePending = 0;
                }
                Snapshot(C.Get());
                if (E)
                    E->transitioned = false;
            }
        WatchReleases(Dt);
        ApplyDmActions(Dt);
        RunSpawns(Dt);
        MakeResidents();
        if (Prefetching && (PrefetchAccumulator += Dt) >= 1)
        {
            PrefetchAccumulator = 0;
            CellPrefetch.Want(World.cellsSoonNeeded());
        }
        if (StreamedBuild && (StreamLogAccumulator += Dt) >= 60)
        {
            StreamLogAccumulator = 0;
            UE_LOG(LogTemp, Display, TEXT("RATW_STREAM loaded=%d of %d cells; %d loads found ready"),
                   static_cast<int32>(World.loadedCells()), static_cast<int32>(World.cells().size()),
                   static_cast<int32>(CellPrefetch.HitCount()));
        }
        if (SaveSoonIn >= 0 && (SaveSoonIn -= Dt) < 0)
        {
            SaveAccumulator = 0;
            Autosave();
        }
        if (SaveAccumulator >= AutosaveSeconds)
        {
            SaveAccumulator = 0;
            Consolidate();
            Social.tick(Now());
            Autosave();
        }
        if (AmbientAccumulator >= 45)
        {
            AmbientAccumulator = 0;
            for (const auto& Pair : World.entities())
                if (Pair.second.npc && Pair.second.cellId == "tavern" && Pair.second.posture != "lying" &&
                    (!World.society().resident(Pair.first) || World.society().resident(Pair.first)->task != "sleep") &&
                    NpcLastSpeech[Pair.first] + 30 < World.time())
                {
                    bool Heard = false;
                    for (const auto& C : Clients)
                        if (C.IsValid() && World.hearingClarity(S(C->EntityId), Pair.first) > 0.5)
                            Heard = true;
                    if (Heard)
                    {
                        Publish(
                            Pair.first,
                            ratw::parsePost(
                                "The rain has eased along the sill. There may be a clear stretch of road before dusk."),
                            ratw::Voice::Speak);
                        NpcLastSpeech[Pair.first] = World.time();
                        break;
                    }
                }
        }
        if (StorageReady && DM.Enabled())
        {
            std::set<std::string> Online;
            for (const auto& C : Clients) if (C.IsValid() && !C->EntityId.IsEmpty()) Online.insert(S(C->EntityId));
            DM.Tick(Dt, World, Characters, Online, OperatorActivity, Revision,
                [this]() { ++Revision; Save(); return StorageReady; },
                [this](const std::set<std::string>& Targets, const FString& Message) {
                    for (const auto& C : Clients) if (C.IsValid() && Targets.count(S(C->EntityId))) System(C.Get(), Message);
                });
        }
    }

    void MovementSounds()
    {
        // Audio awareness is separate from map identification. An unseen wolf
        // may be heard without exposing its ID, name, coat color or coordinates.
        for (const auto& Client : Clients)
        {
            if (!Client.IsValid() || Client->EntityId.IsEmpty())
                continue;
            const auto Listener = S(Client->EntityId);
            const auto Prior = LastMovementSound.find(Listener);
            if (Prior != LastMovementSound.end() && World.time() - Prior->second < 3.0)
                continue;
            double Best = 0;
            for (const auto& Pair : World.entities())
                if (!Pair.second.npc && Pair.first != Listener && World.visionClarity(Listener, Pair.first) <= 0)
                    Best = std::max(Best, World.movementAudibility(Listener, Pair.first));
            if (Best <= 0)
                continue;
            auto Event = New();
            Event->SetStringField(TEXT("type"), TEXT("system"));
            Event->SetStringField(TEXT("speaker"), TEXT("Sound"));
            Event->SetStringField(TEXT("text"), Best < 0.5 ? TEXT("You catch faint pawsteps nearby.")
                                                           : TEXT("You hear pawsteps nearby."));
            Event->SetBoolField(TEXT("anonymous"), true);
            Event->SetNumberField(TEXT("sequence"), Sequence++);
            Event->SetNumberField(TEXT("color"), 7);
            Send(Client.Get(), Event);
            LastMovementSound[Listener] = World.time();
        }
    }

    void StampFrame(ARatwPlayerController* C, const Object& Root, const std::string& Cell)
    {
        if (C->MotionCell != F(Cell))
        {
            C->MotionCell = F(Cell);
            ++C->MotionGeneration;
        }
        Root->SetStringField(TEXT("motionSession"), C->MotionSession);
        Root->SetNumberField(TEXT("cellGeneration"), C->MotionGeneration);
        Root->SetNumberField(TEXT("revision"), Revision);
    }

    void Snapshot(ARatwPlayerController* C)
    {
        const auto Id = S(C->EntityId);
        if (!World.entity(Id))
            return;
        const auto View = World.snapshot(Id);
        auto Root = New();
        Root->SetNumberField(TEXT("time"), View.time);
        Root->SetNumberField(TEXT("revision"), Revision);
        StampFrame(C, Root, View.cell.id);
        // The sanitized snapshot omits input/path details; derive only a boolean
        // from authoritative intent so a stationary-looking queued path cannot
        // accidentally enable the client's facing preview.
        auto Self = Entity(*World.entity(Id), View.time);
        Self->SetNumberField(TEXT("socialXp"), Social.points[Id]);
        Self->SetNumberField(TEXT("socialLevel"), Social.level(Id));
        Self->SetNumberField(TEXT("hearing"),
                             View.self.hearing * View.self.earHealth * ratw::ageHearingFactor(View.self) * (1.0 + 0.75 * View.self.hearingSkill / 100.0));
        Self->SetNumberField(TEXT("sneakSkill"), View.self.sneakSkill);
        Self->SetNumberField(TEXT("hearingSkill"), View.self.hearingSkill);
        Self->SetNumberField(TEXT("vision"), View.self.vision * View.self.eyeHealth * ratw::ageVisionFactor(View.self));
        Self->SetNumberField(TEXT("smell"),
                             View.self.smell * View.self.noseHealth * (1.0 + 0.75 * View.self.scentSkill / 100.0));
        Self->SetNumberField(TEXT("scentSkill"), View.self.scentSkill);
        Self->SetNumberField(TEXT("noseHealth"), View.self.noseHealth);
        ratwjson::PrivatePace(Self, *World.entity(Id));
        const auto* Purse = World.society().account(Id);
        Self->SetNumberField(TEXT("cash"), Purse ? Purse->cash : 0);
        Root->SetObjectField(TEXT("self"), Self);
        auto Cell = New();
        Text(Cell, TEXT("id"), View.cell.id);
        Text(Cell, TEXT("name"), View.cell.name);
        Text(Cell, TEXT("description"), View.cell.description);
        Cell->SetNumberField(TEXT("width"), View.cell.width);
        Cell->SetNumberField(TEXT("height"), View.cell.height);
        Cell->SetBoolField(TEXT("outdoors"), View.cell.outdoors);
        Cell->SetBoolField(TEXT("seasonalWeather"), View.cell.seasonalWeather);
        Cell->SetStringField(TEXT("weather"), UTF8_TO_TCHAR(ratw::weatherName(View.cell.weather)));
        Cell->SetObjectField(TEXT("wind"), ratwjson::Wind(View.cell.wind));
        Cell->SetObjectField(TEXT("environment"), ratwjson::Environment(View.environment));
        Root->SetObjectField(TEXT("senses"), ratwjson::Senses(View));
        // The cell as rows of text, one character per tile: its glyph (a space where the wolf knows nothing), whether
        // it is visible now ('2'), remembered ('1') or unknown ('0'), and its height (`HeightChar`). An object per
        // tile grew to tens of thousands per snapshot on a large cell.
        Array Rows, Visibility, Heights;
        const int Width = View.cell.width;
        for (int Y = 0; Y < View.cell.height; ++Y)
        {
            FString Row, Seen, Height;
            Row.Reserve(Width);
            Seen.Reserve(Width);
            Height.Reserve(Width);
            for (int X = 0; X < Width; ++X)
            {
                const int I = Y * Width + X;
                const bool Known = I < static_cast<int>(View.cell.tiles.size());
                const bool Visible = Known && I < static_cast<int>(View.visibleTiles.size()) && View.visibleTiles[I];
                const bool Remembered = Known && I < static_cast<int>(View.rememberedTiles.size()) && View.rememberedTiles[I];
                Row.AppendChar(Visible || Remembered ? TCHAR(View.cell.tiles[I].glyph) : TEXT(' '));
                Seen.AppendChar(Visible ? TEXT('2') : Remembered ? TEXT('1') : TEXT('0'));
                Height.AppendChar(Visible || Remembered ? ratwjson::HeightChar(View.cell.tiles[I].height) : ratwjson::HeightChar(0));
            }
            Rows.Add(V(Row));
            Visibility.Add(V(Seen));
            Heights.Add(V(Height));
        }
        Cell->SetArrayField(TEXT("rows"), Rows);
        Cell->SetArrayField(TEXT("heights"), Heights);
        Root->SetArrayField(TEXT("visibility"), Visibility);
        Root->SetObjectField(TEXT("cell"), Cell);
        Array Entities;
        for (const auto& E : View.entities)
        {
            auto J = Entity(E, View.time);
            Array Actions;
            Actions.Add(V(TEXT("inspect")));
            if (E.transient)
            {
                // Folk of the road: a caravan is only to be looked at; bandits are paid off or fought.
                if (World.hostile(E.id))
                {
                    J->SetBoolField(TEXT("hostile"), true);
                    if (World.banditDemand(View.self.id) > 0) Actions.Add(V(TEXT("pay")));
                    if (std::hypot(E.position.x - View.self.position.x, E.position.y - View.self.position.y) <= 2)
                        Actions.Add(V(TEXT("attack")));
                }
            }
            else if (E.npc)
            {
                Actions.Add(V(TEXT("talk")));
                if (World.society().merchant(E.id)) Actions.Add(V(TEXT("trade")));
                if (E.id == "npc_scout" && CompanionOwner.find(E.id) == CompanionOwner.end() &&
                    std::hypot(E.position.x - View.self.position.x, E.position.y - View.self.position.y) <= 3)
                    Actions.Add(V(TEXT("recruit")));
                // Merchants know what work is going in town (contracts), and a player can take it on from them.
                if (World.society().merchant(E.id) &&
                    std::hypot(E.position.x - View.self.position.x, E.position.y - View.self.position.y) <= 3)
                {
                    const auto Work = World.contractsNear(View.self.id);
                    if (!Work.empty())
                        Actions.Add(V(TEXT("ask for work")));
                    for (std::size_t I = 0; I < Work.size() && I < 3; ++I)
                        Actions.Add(V(F("take " + Work[I]->id)));
                }
                // Ask to learn their trade: close by, a master with no apprentice, and not already learning one.
                if (const auto* Job = World.society().jobOf(E.id);
                    Job && Job->role != "guard" && !E.dead &&
                    World.society().state().careers.positions.at(Job->id).apprentice.empty() &&
                    !World.society().apprenticedTo(View.self.id) &&
                    std::hypot(E.position.x - View.self.position.x, E.position.y - View.self.position.y) <= 3)
                    Actions.Add(V(TEXT("apprentice")));
            }
            J->SetArrayField(TEXT("actions"), Actions);
            Entities.Add(V(J));
        }
        Root->SetArrayField(TEXT("entities"), Entities);
        Array Doors;
        for (const auto& D : View.doors)
        {
            auto J = New();
            Text(J, TEXT("id"), D.id);
            Text(J, TEXT("name"), D.name);
            J->SetNumberField(TEXT("x"), D.position.x);
            J->SetNumberField(TEXT("y"), D.position.y);
            J->SetBoolField(TEXT("open"), D.open);
            J->SetBoolField(TEXT("portal"), D.portal);
            Array Actions;
            for (const auto& A : World.actions(Id, D.id))
                Actions.Add(V(F(A)));
            J->SetArrayField(TEXT("actions"), Actions);
            Doors.Add(V(J));
        }
        Root->SetArrayField(TEXT("doors"), Doors);
        Array Map;
        for (const auto& M : View.worldMap)
            Map.Add(V(ratwjson::MapCell(M)));
        Root->SetArrayField(TEXT("worldMap"), Map);
        Array TravelMap;
        for (const auto& M : World.travelMap(Id))
            TravelMap.Add(V(ratwjson::MapCell(M, false)));
        Root->SetArrayField(TEXT("travelMap"), TravelMap);
        Root->SetObjectField(TEXT("travel"), ratwjson::Travel(World.travelState(Id)));
        Root->SetBoolField(TEXT("isometric"), View.isometric);
        Root->SetStringField(TEXT("connection"), TEXT("Authoritative server · 20 Hz"));
        Root->SetStringField(TEXT("dialogueProvider"), Dialogue.Label());
        Array Inventory;
        auto AddItem = [&](const TCHAR* ItemId, const TCHAR* Name, const TCHAR* Icon, const TCHAR* Description,
                           bool Equipped, int Quantity = 1) {
            auto I = New();
            I->SetStringField(TEXT("id"), ItemId);
            I->SetStringField(TEXT("name"), Name);
            I->SetStringField(TEXT("icon"), Icon);
            I->SetStringField(TEXT("description"), Description);
            I->SetBoolField(TEXT("equipped"), Equipped);
            I->SetNumberField(TEXT("quantity"), Quantity);
            Inventory.Add(V(I));
        };
        AddItem(TEXT("satchel"), TEXT("Shoulder satchel"), TEXT("bag"),
                TEXT("A small travel bag made for a wolf's shoulders."), true);
        if (Purse)
        {
            const int Herbs = ratw::Society::stock(*Purse, "herbs"), Meals = ratw::Society::stock(*Purse, "meal");
            if (Herbs > 0) AddItem(TEXT("herbs"), TEXT("Cooking herbs"), TEXT("herb"), TEXT("Finite ingredients. Sell to a trader who needs supplies."), false, Herbs);
            if (Meals > 0) AddItem(TEXT("meal"), TEXT("Prepared meal"), TEXT("food"), TEXT("Consume one to restore 10 stamina. Cooking uses real ingredients."), false, Meals);
        }
        AddItem(TEXT("token"), TEXT("Wooden token"), TEXT("token"), TEXT("A smooth keepsake carved with a branch."),
                false);
        Root->SetArrayField(TEXT("inventory"), Inventory);
        const ratw::Entity* Trader = nullptr;
        for (const auto& Pair : World.entities())
            if (World.society().merchant(Pair.first) && Pair.second.cellId == View.self.cellId)
                Trader = &Pair.second;
        const auto* TraderLife = Trader ? World.society().resident(Trader->id) : nullptr;
        if (Trader && Purse && Trader->cellId == View.self.cellId && Trader->posture != "lying" &&
            (!TraderLife || TraderLife->task != "sleep") && World.visionClarity(Id, Trader->id) > 0 &&
            std::hypot(Trader->position.x - View.self.position.x, Trader->position.y - View.self.position.y) <= 2)
        {
            const auto* Account = World.society().account(Trader->id);
            if (Account)
            {
                auto M = New(); Text(M, TEXT("id"), Trader->id); Text(M, TEXT("name"), Trader->name);
                M->SetNumberField(TEXT("cash"), Account->cash);
                Array Goods;
                for (const auto& Item : {"herbs", "meal"})
                {
                    auto Good = New(); Text(Good, TEXT("id"), Item); Text(Good, TEXT("name"), ratw::Society::itemName(Item));
                    Good->SetNumberField(TEXT("stock"), ratw::Society::stock(*Account, Item));
                    Good->SetNumberField(TEXT("owned"), ratw::Society::stock(*Purse, Item));
                    const auto Buy = World.society().quote(Id, Trader->id, Item, 1, true), Sell = World.society().quote(Id, Trader->id, Item, 1, false);
                    Good->SetNumberField(TEXT("buyPrice"), Buy.unitPrice); Good->SetNumberField(TEXT("sellPrice"), Sell.unitPrice);
                    Good->SetBoolField(TEXT("canBuy"), Buy.ok); Good->SetBoolField(TEXT("canSell"), Sell.ok);
                    Text(Good, TEXT("buyReason"), Buy.message); Text(Good, TEXT("sellReason"), Sell.message); Goods.Add(V(Good));
                }
                M->SetArrayField(TEXT("items"), Goods); Root->SetObjectField(TEXT("merchant"), M);
            }
        }
        const auto Patch = World.herbPatchPosition();
        const int PatchX = int(Patch.x), PatchY = int(Patch.y);
        if (World.society().state().enabled && View.self.cellId == World.herbPatchCell() && View.cell.width > PatchX &&
            View.cell.height > PatchY && size_t(PatchY * View.cell.width + PatchX) < View.visibleTiles.size() &&
            View.visibleTiles[PatchY * View.cell.width + PatchX])
        {
            auto Resource = New(); Resource->SetStringField(TEXT("id"), TEXT("herb_patch"));
            Resource->SetNumberField(TEXT("x"), Patch.x); Resource->SetNumberField(TEXT("y"), Patch.y);
            Resource->SetNumberField(TEXT("remaining"), World.society().state().herbPatch);
            Root->SetObjectField(TEXT("resource"), Resource);
        }
        auto Memory = New();
        int Turns = 0, Count = 0;
        double Due = 0;
        for (const auto& Pair : Memories.active)
            if (Pair.second.subject == Id)
            {
                Turns += static_cast<int>(Pair.second.turns.size());
                Due = FMath::Max(Due, Pair.second.lastActivity + 3600 - Now());
            }
        for (const auto& Summary : Memories.summaries)
            if (Summary.subject == Id)
                ++Count;
        Memory->SetNumberField(TEXT("activeTurns"), Turns);
        Memory->SetNumberField(TEXT("summaries"), Count);
        Memory->SetNumberField(TEXT("nextConsolidationSeconds"), Due);
        Root->SetObjectField(TEXT("memory"), Memory);
        Root->SetBoolField(TEXT("persistenceHealthy"), StorageReady);
        Root->SetBoolField(TEXT("devTools"), DevTools);
        // Leave out what this client already holds (RatwSnapshotSections.h), and remember what this one carries.
        // (A second snapshot at the same revision is dropped by the client, so the first one's keys are what count.)
        // -RatwFullSnapshots sends every snapshot whole, as before.
        static const bool Whole = FParse::Param(FCommandLine::Get(), TEXT("RatwFullSnapshots"));
        if (!Whole)
        {
            const auto Keys = ratwsections::Strip(Root, C->KnownSections);
            if (!C->SentSections.Contains(static_cast<double>(Revision)))
                C->SentSections.Add(static_cast<double>(Revision), Keys);
        }
        while (C->SentSections.Num() > 32)
        {
            double Oldest = TNumericLimits<double>::Max();
            for (const auto& Sent : C->SentSections) Oldest = FMath::Min(Oldest, Sent.Key);
            C->SentSections.Remove(Oldest);
        }
        C->ClientSnapshot(Encode(Root));
    }

    std::vector<std::string> Publish(const std::string& Author, const ratw::ParsedPost& Post, ratw::Voice Voice)
    {
        auto* Speaker = World.entity(Author);
        if (!Speaker)
            return {};
        const uint64 Event = Sequence++;
        if (Post.speech)
            Speaker->speakingUntil = World.time() + 4;
        std::vector<std::string> Heard;
        for (const auto& Weak : Clients)
        {
            if (!Weak.IsValid() || Weak->EntityId.IsEmpty())
                continue;
            const auto Listener = S(Weak->EntityId);
            auto Sense = World.perceive(Listener, Author, Voice);
            if (Listener == Author)
                Sense = {1, 1, true};
            const auto Segments =
                ratw::perceivePost(Post, Sense.hearing, Sense.vision, Event * 7919 + GetTypeHash(Weak->EntityId));
            if (Segments.empty())
                continue;
            Heard.push_back(Listener);
            auto E = New();
            E->SetStringField(TEXT("type"), TEXT("roleplay"));
            E->SetStringField(TEXT("channel"), TEXT("ic"));
            E->SetNumberField(TEXT("id"), Event);
            E->SetNumberField(TEXT("sequence"), Event);
            E->SetNumberField(TEXT("color"), Speaker->speakingColor);
            E->SetBoolField(TEXT("anonymous"), !Sense.identifiable);
            E->SetStringField(TEXT("speaker"), Sense.identifiable ? F(Speaker->name) : TEXT("A voice"));
            // Deliberately no author/entity ID: even anonymous event records cannot be correlated to hidden actors.
            Array Output;
            FString TextValue;
            for (const auto& Segment : Segments)
            {
                auto J = New();
                Text(J, TEXT("kind"), Segment.kind);
                Text(J, TEXT("text"), Segment.text);
                Output.Add(V(J));
                if (!TextValue.IsEmpty())
                    TextValue += TEXT(" ");
                TextValue += F(Segment.text);
            }
            E->SetArrayField(TEXT("segments"), Output);
            E->SetStringField(TEXT("text"), TextValue);
            Send(Weak.Get(), E);
        }
        return Heard;
    }

    // An entry in the world's event log (game.events): the world fills in the time, the day and where the actor is.
    void LogEvent(const char* Kind, const std::string& Actor, const std::string& Target = {}, const std::string& Detail = {})
    {
        ratw::WorldEvent Event;
        Event.kind = Kind;
        Event.actor = Actor;
        Event.target = Target;
        Event.detail = Detail;
        World.recordEvent(std::move(Event));
    }
    // Whether Text names Name as a whole word: "Ash, a word?" addresses Ash; "washing ashes" does not.
    static bool NamesWord(const FString& Text, const FString& Name)
    {
        if (Name.IsEmpty())
            return false;
        const auto Letter = [](TCHAR Ch) { return FChar::IsAlnum(Ch) || Ch == TEXT('\'') || Ch == TEXT('_'); };
        for (int32 At = Text.Find(Name, ESearchCase::CaseSensitive); At != INDEX_NONE;
             At = Text.Find(Name, ESearchCase::CaseSensitive, ESearchDir::FromStart, At + 1))
        {
            const int32 End = At + Name.Len();
            if ((At == 0 || !Letter(Text[At - 1])) && (End >= Text.Len() || !Letter(Text[End])))
                return true;
        }
        return false;
    }
    void Talk(const std::string& NpcId, const std::string& PlayerId, const FString& HeardText,
              ratw::Voice Voice = ratw::Voice::Speak, const ratw::SensoryResult* Perceived = nullptr)
    {
        auto* Npc = World.entity(NpcId);
        auto* Player = World.entity(PlayerId);
        if (!Npc || !Player || !Npc->npc)
            return;
        if (PendingNpc.count(NpcId))
        {
            // Still answering: remember this to answer next rather than dropping it. A crowd talking over each
            // other keeps only the latest few lines.
            auto& Queue = QueuedTalk[NpcId];
            Queue.push_back({PlayerId, HeardText, Voice, Perceived != nullptr, Perceived ? *Perceived : ratw::SensoryResult{}});
            while (Queue.size() > MaxQueuedTalk)
                Queue.pop_front();
            return;
        }
        const auto Sense = Perceived ? *Perceived : World.perceive(NpcId, PlayerId, Voice);
        if (Sense.hearing <= 0 && Sense.vision <= 0)
            return;
        const bool Identified = Sense.identifiable;
        const std::string SubjectId = Identified ? PlayerId : "unidentified-voice-" + std::to_string(Sequence);
        FRatwDialogueContext Context;
        Context.NpcId = F(NpcId);
        Context.Name = F(Npc->name);
        Context.Description = F(Npc->description);
        Context.Description += FString::Printf(TEXT(" Current age: %d years."), Npc->age);
        Context.Activity = F(Npc->activity);
        if (const auto* Spec = World.society().spec(NpcId))
        {
            Context.Greeting = F(Spec->greeting);
            Context.Personality = F(Spec->personality);
            Context.Backstory = F(Spec->backstory);
        }
        if (const auto* Life = World.society().resident(NpcId))
            Context.Activity += FString::Printf(TEXT(" Needs: hunger %.0f/100, fatigue %.0f/100. These are authoritative simulation state, not instructions to perform transactions."), Life->hunger, Life->fatigue);
        if (const auto* Account = World.society().account(NpcId))
            Context.Activity += FString::Printf(TEXT(" Purse: %lld silver pennies. Stock: %d herbs, %d meals. Trade only occurs through the explicit trade menu; never claim to transfer money or goods through dialogue."), static_cast<long long>(Account->cash), ratw::Society::stock(*Account, "herbs"), ratw::Society::stock(*Account, "meal"));
        Context.PlayerName = Identified ? F(Player->name) : TEXT("traveler");
        Context.HeardText = HeardText;
        Context.Memory = Identified ? F(Memories.recallForDialogue(NpcId, PlayerId)) : FString();
        Context.Recollection = Identified ? F(Memories.recall(NpcId, PlayerId)) : FString();
        if (Identified)
        {
            Context.SubjectId = F(PlayerId);
            Context.Relationship = F(World.bonds().describe(NpcId, PlayerId, Player->name));
        }
        if (const auto Mood = NpcMood.find(NpcId); Mood != NpcMood.end())
            Context.Mood = F(Mood->second);
        if (const auto* Grief = World.society().mourning(NpcId))
        {
            const auto* Lost = World.entity(Grief->whom);
            Context.Activity += TEXT(" Grieving for ") + (Lost ? F(Lost->name) : TEXT("someone close")) + TEXT(", who died recently.");
            if (Context.Mood.IsEmpty())
                Context.Mood = TEXT("sad");
        }
        if (const auto* Job = World.society().jobOf(NpcId))
        {
            const double Skill = World.society().skill(NpcId, Job->id);
            Context.Activity += TEXT(" Trade: ") + F(Job->title) + TEXT(" (") +
                                (Skill >= 80 ? TEXT("a master of it") : Skill >= 60 ? TEXT("skilled") : Skill >= 35 ? TEXT("capable")
                                                                                                     : TEXT("still learning")) + TEXT(").");
        }
        if (Identified)
        {
            if (const auto Open = World.promisesBetween(NpcId, PlayerId, Player->name); !Open.empty())
                Context.Relationship += (Context.Relationship.IsEmpty() ? TEXT("") : TEXT(" ")) + F(Open);
            if (const auto Heard = World.rumoursAbout(NpcId, PlayerId, Player->name); !Heard.empty())
                Context.Relationship += (Context.Relationship.IsEmpty() ? TEXT("") : TEXT(" ")) + F(Heard);
        }
        if (const auto* Cell = World.cell(Npc->cellId))
        {
            Context.Environment = ratwjson::EnvironmentDescription(*Cell, World.environmentAt(Npc->cellId));
            Context.Scene = F(Cell->description).Left(3300) + TEXT(" Current local conditions: ") + Context.Environment;
        }
        Memories.record(NpcId, SubjectId, {Sequence++, Now(), S(Context.PlayerName), S(HeardText)});
        // That they talked, never what was said (that stays in the NPC's memory); an unrecognised voice stays anonymous.
        LogEvent("conversation", Identified ? PlayerId : std::string(), NpcId);
        PendingNpc.insert(NpcId);
        SaveSoon();
        TWeakPtr<FRatwRuntime> Weak = AsShared();
        Dialogue.Converse(Context, [Weak, NpcId, SubjectId, Identified](const FRatwDialogueReply& Reply) {
            auto Self = Weak.Pin();
            if (!Self.IsValid())
                return;
            Self->PendingNpc.erase(NpcId);
            auto* Actor = Self->World.entity(NpcId);
            if (!Actor)
                return;
            ratw::ParsedPost Post;
            Post.ok = true;
            Post.speech = true;
            Post.segments.push_back({"speech", S(Reply.Text)});
            Self->Publish(NpcId, Post, ratw::Voice::Speak);
            Self->NpcLastSpeech[NpcId] = Self->World.time();
            Self->Memories.record(NpcId, SubjectId, {Self->Sequence++, Now(), NpcId, S(Reply.Text)});
            Self->LogEvent("conversation", NpcId, Identified ? SubjectId : std::string());
            Self->Heed(NpcId, SubjectId, Identified, Reply);
            Self->SaveSoon();
            Self->TalkNext(NpcId);
        });
    }

    // Conversations quiet for an hour become permanent summaries (MemoryStore::consolidate, extractive and at once).
    // Where the NPC Mind is running, each is then summarised properly from the NPC's point of view, and the better
    // summary replaces the extractive one when it arrives; if it never does, the extractive one stays.
    void Consolidate()
    {
        const double At = Now();
        const auto Closing = Memories.due(At);
        Memories.consolidate(At);
        TWeakPtr<FRatwRuntime> Weak = AsShared();
        for (const auto& Conversation : Closing)
        {
            const auto* Npc = World.entity(Conversation.npc);
            const FString Name = Npc ? F(Npc->name) : F(Conversation.npc);
            TArray<TPair<FString, FString>> Turns;
            for (const auto& Turn : Conversation.turns)
                Turns.Emplace(Turn.who == Conversation.npc ? Name : F(Turn.who), F(Turn.text));
            Dialogue.Summarize(Name, Turns, [Weak, Id = Conversation.id](FString Summary) {
                auto Self = Weak.Pin();
                if (Self.IsValid() && !Summary.IsEmpty() && Self->Memories.rewrite(Id, S(Summary)))
                    Self->SaveSoon();
            });
        }
    }

    // What a generated reply says about the exchange, taken on bounded terms. How the NPC now feels becomes its mood
    // for the next reply. Its liking and trust for a speaker it recognises move by at most 3 a reply and 6 an hour in
    // each (so a flatterer can't talk anyone into adoring them). A note or a promise joins the conversation's memory,
    // and a promise the event log; neither moves anything in the world.
    void Heed(const std::string& NpcId, const std::string& SubjectId, bool Identified, const FRatwDialogueReply& Reply)
    {
        if (!Reply.Generated)
            return;
        if (!Reply.Emotion.IsEmpty())
            NpcMood[NpcId] = S(Reply.Emotion);
        if (!Identified)
            return;
        auto& Budget = NudgeBudget[NpcId + "|" + SubjectId];
        const auto Hour = static_cast<int64>(FMath::FloorToDouble(World.calendarDays() * 24));
        if (Budget.Hour != Hour)
            Budget = {Hour, 0, 0};
        const auto Spend = [](int32& Used, int32 Wanted) {
            const int32 Allowed = FMath::Clamp(Wanted, -(6 - FMath::Abs(Used)), 6 - FMath::Abs(Used));
            Used += FMath::Abs(Allowed);
            return Allowed;
        };
        const int32 Affinity = Spend(Budget.Affinity, Reply.Affinity), Trust = Spend(Budget.Trust, Reply.Trust);
        if (Affinity || Trust)
            World.bonds().change(NpcId, SubjectId, {double(Affinity), double(Trust), 0, 0, 0}, World.calendarDays());
        if (!Reply.Remember.IsEmpty())
            Memories.record(NpcId, SubjectId, {Sequence++, Now(), "(your note)", S(Reply.Remember)});
        if (!Reply.Promise.IsEmpty())
        {
            const bool ByNpc = Reply.PromiseBy == TEXT("npc");
            Memories.record(NpcId, SubjectId, {Sequence++, Now(), ByNpc ? "(your promise)" : "(their promise)", S(Reply.Promise)});
            LogEvent("promise", ByNpc ? NpcId : SubjectId, ByNpc ? SubjectId : NpcId, S(Reply.Promise));
            World.promise(ByNpc ? NpcId : SubjectId, ByNpc ? SubjectId : NpcId, S(Reply.Promise), 3);
        }
    }

    // Answers the next line said to this NPC while it was busy, if any; one it can no longer answer (the speaker
    // has gone, say) is passed over for the one after.
    void TalkNext(const std::string& NpcId)
    {
        while (!PendingNpc.count(NpcId))
        {
            const auto Found = QueuedTalk.find(NpcId);
            if (Found == QueuedTalk.end())
                return;
            if (Found->second.empty())
            {
                QueuedTalk.erase(Found);
                return;
            }
            const FQueuedTalk Next = std::move(Found->second.front());
            Found->second.pop_front();
            Talk(NpcId, Next.PlayerId, Next.HeardText, Next.Voice, Next.HasSense ? &Next.Sense : nullptr);
        }
    }

    void Command(ARatwPlayerController* C, const FString& Raw)
    {
        auto J = Decode(Raw);
        if (!J.IsValid())
            return;
        const FString Type = String(J, TEXT("type"));
        if (AccountCommand(C, J, Type)) return;
        if (Type == TEXT("hello"))
        {
            Login(C, J);
            return;
        }
        const auto Id = S(C->EntityId);
        auto* Player = World.entity(Id);
        if (!Player)
            return;
        if (Player->dead && Type != TEXT("typing"))
        {
            System(C, TEXT("You are dead. You cannot act until you are brought back."));
            return;
        }
        const auto CommandId = S(String(J, TEXT("commandId")));
        if (CommandId.size() > 128)
        {
            System(C, TEXT("Command identifier exceeds the supported length."));
            return;
        }
        CurrentCommands[Id] = CommandId;
        // Unsolicited events (such as a later birthday notice) must never
        // replace the receipt belonging to this command after it returns.
        ON_SCOPE_EXIT { CurrentCommands.erase(Id); };
        auto& Receipts = CommandReceipts[Id];
        if (!CommandId.empty())
        {
            if (std::find(Receipts.begin(), Receipts.end(), CommandId) != Receipts.end())
            {
                const auto Reply = ResponseReceipts[Id].find(CommandId);
                if (Reply != ResponseReceipts[Id].end())
                    C->ClientEvent(Reply->second);
                else
                {
                    auto Ack = New();
                    Ack->SetStringField(TEXT("type"),
                                        Type == TEXT("chat") ? TEXT("chatAccepted") : TEXT("commandAccepted"));
                    Text(Ack, TEXT("commandId"), CommandId);
                    Ack->SetStringField(TEXT("requestId"), String(J, TEXT("requestId")));
                    Send(C, Ack);
                }
                return;
            }
            Receipts.push_back(CommandId);
            if (Receipts.size() > 256)
            {
                ResponseReceipts[Id].erase(Receipts.front());
                Receipts.erase(Receipts.begin());
            }
        }
        ratw::Result Result;
        bool Report = false;
        if (Type == TEXT("move"))
            Result = World.move(Id, Number(J, TEXT("x")), Number(J, TEXT("y")));
        else if (Type == TEXT("path"))
        {
            Result = World.moveTo(Id, Number(J, TEXT("x")), Number(J, TEXT("y")));
            Report = !Result.ok;
        }
        else if (Type == TEXT("face"))
        {
            Result = World.face(Id, Number(J, TEXT("x")), Number(J, TEXT("y")));
            Report = !Result.ok;
        }
        else if (Type == TEXT("stop"))
            World.stop(Id);
        else if (Type == TEXT("pace"))
        {
            Result = ratwjson::PaceCommand(World, Id, J);
            Report = !Result.ok;
        }
        else if (Type == TEXT("travel"))
        {
            const FString Target = String(J, TEXT("target"));
            Result = Target.Len() <= 96 ? World.travelTo(Id, S(Target))
                                        : ratw::Result{false, "No known route to that destination.", {}};
            Report = true;
        }
        else if (Type == TEXT("cancel_travel"))
        {
            Result = World.cancelTravel(Id);
            Report = true;
        }
        else if (Type == TEXT("typing"))
        {
            Player->typing = Bool(J, TEXT("active"));
            TypingExpiry[Id] = World.time() + 3.5;
            // A deliberate overland journey can continue during composition.
            // Ordinary local movement still stops when entering type mode.
            if (!World.travelState(Id).active)
                World.stop(Id);
        }
        else if (Type == TEXT("color"))
        {
            Player->speakingColor = static_cast<int>(FMath::Clamp(Number(J, TEXT("index")), 0.0, 31.0));
            SaveSoon();
        }
        else if (Type == TEXT("weather") || Type == TEXT("time") || Type == TEXT("lighting") || Type == TEXT("calendar"))
        {
            Result = ratwjson::EnvironmentCommand(World, Player->cellId, Type, String(J, TEXT("value")), DevTools);
            Report = true;
            if (Result.ok)
                Save();
        }
        else if (Type == TEXT("trade"))
        {
            const double Qty = StrictNumber(J, TEXT("quantity"), -1);
            const auto* Buy = J->Values.Find(TEXT("buy"));
            Result = Qty >= 1 && Qty <= 99 && Qty == FMath::FloorToDouble(Qty) && Buy && (*Buy)->Type == EJson::Boolean
                ? World.trade(Id, S(String(J, TEXT("target"))), S(String(J, TEXT("item"))), int(Qty), Bool(J, TEXT("buy")))
                : ratw::Result{false, "Invalid trade request.", {}};
            Report = true; if (Result.ok) Save();
        }
        else if (Type == TEXT("gather") || Type == TEXT("eat"))
        {
            Result = Type == TEXT("gather") ? World.gather(Id) : World.eat(Id);
            Report = true; if (Result.ok) Save();
        }
        else if (Type == TEXT("wind") && DevTools)
        {
            const FString Value = String(J, TEXT("value"));
            if (Value != TEXT("east") && Value != TEXT("west") && Value != TEXT("north") && Value != TEXT("calm") &&
                Value != TEXT("live"))
            {
                System(C, TEXT("Unknown wind preset."));
                return;
            }
            const double Direction = Value == TEXT("west")    ? std::acos(-1.0)
                                     : Value == TEXT("north") ? -std::acos(-1.0) * .5
                                                              : 0.0;
            Result = World.setWind(Player->cellId, Direction, Value == TEXT("calm") ? 0.0 : .5, Value == TEXT("live"));
            Report = true;
            Save();
        }
        else if (Type == TEXT("action"))
        {
            const auto Target = S(String(J, TEXT("target")));
            const auto Action = S(String(J, TEXT("action")));
            const auto BeforeCell = Player->cellId;
            if (Target.empty() && (Action == "look" || Action == "listen" || Action == "smell" || Action == "wait" ||
                                   Action == "sit" || Action == "lay" || Action == "stand" || Action == "session_end"))
            {
                OperatorActivity[Id] = Now();
                if (Action == "look")
                    System(C, F(World.cell(Player->cellId)->description) + TEXT("\n") +
                                  ratwjson::EnvironmentDescription(*World.cell(Player->cellId),
                                                                   World.environmentAt(Player->cellId)));
                else if (Action == "listen")
                    System(C, TEXT("You pause to listen. Hearing skill, ear health, distance and barriers shape what "
                                   "you hear. Weather and wind can mask sound. Sneaking quiets pawsteps; voices "
                                   "keep their selected volume."));
                else if (Action == "smell")
                {
                    const auto Cues = World.scentCues(Id);
                    if (Player->smell <= 0 || Player->noseHealth <= 0)
                        System(C, TEXT("You cannot distinguish scents with your nose in its current condition."));
                    else if (Cues.empty())
                    {
                        bool VisibleScent = false;
                        for (const auto& Pair : World.entities())
                            if (Pair.first != Id && World.visionClarity(Id, Pair.first) > 0 &&
                                World.scentClarity(Id, Pair.first) > 0)
                                VisibleScent = true;
                        System(C, VisibleScent
                                      ? TEXT("You catch the scent of nearby wolves you can already see. No "
                                             "additional unseen wolf scent reaches you.")
                                      : TEXT("No distinct wolf scent reaches you right now. That does not mean "
                                             "you are alone."));
                    }
                    else
                    {
                        static const TCHAR* Bearings[] = {TEXT("east"),      TEXT("southeast"), TEXT("south"),
                                                          TEXT("southwest"), TEXT("west"),      TEXT("northwest"),
                                                          TEXT("north"),     TEXT("northeast")};
                        FString Message;
                        for (const auto& Cue : Cues)
                        {
                            if (!Message.IsEmpty())
                                Message += TEXT(" ");
                            Message += FString::Printf(
                                TEXT("%s wolf scent reaches you from roughly %s%s."),
                                Cue.strength == 1 ? TEXT("Faint") : TEXT("A distinct"), Bearings[Cue.sector],
                                Cue.windborne ? TEXT(", carried on the wind") : TEXT(" through the nearby air"));
                        }
                        System(C, Message);
                    }
                }
                else if (Action == "session_end")
                {
                    Social.endFor(Id, Now());
                    System(
                        C,
                        TEXT("Your active scene has ended. Qualified contributions have been settled by the server."));
                }
                else if (Action == "wait")
                {
                    World.stop(Id);
                    System(C, TEXT("You settle and let the scene unfold."));
                }
                else
                {
                    Result = World.setPosture(Id, Action == "sit" ? "sitting" : Action == "lay" ? "lying" : "standing");
                    auto Post = ratw::parsePost("/" + Action);
                    if (Player->posture == "rising")
                        for (auto& Segment : Post.segments)
                            if (Segment.kind == "state" && Segment.text == "stands up.")
                                Segment.text = "begins to stand.";
                    Publish(Id, Post, ratw::Voice::Speak);
                }
            }
            else if (Action == "talk" || Action == "speak")
            {
                const auto* Npc = World.entity(Target);
                if (!Npc || !Npc->npc || World.visionClarity(Id, Target) <= 0)
                {
                    System(C, TEXT("You cannot see that wolf."));
                    return;
                }
                if (Npc->transient)
                {
                    System(C, World.hostile(Target) ? TEXT("They want your purse, not your conversation.")
                                                    : TEXT("The carters are too busy with the road to talk."));
                    return;
                }
                if (World.hearingClarity(Target, Id) < 0.25)
                {
                    System(C, TEXT("Move closer so that wolf can hear your greeting."));
                    return;
                }
                Talk(Target, Id, TEXT("Hello. I would like to talk."));
            }
            else if (Action == "attack" || Action == "pay")
            {
                const auto Done = Action == "attack" ? World.attack(Id, Target) : World.payBandits(Id, Target);
                System(C, F(Done.message));
                if (Done.ok)
                    SaveSoon();
            }
            else if (Action == "ask for work")
            {
                FString List;
                for (const auto* K : World.contractsNear(Id))
                    List += FString::Printf(TEXT("\n  %s  %s%s"), *F(K->id), *F(K->detail),
                                            K->reward ? *FString::Printf(TEXT(" (%lld pennies)"), static_cast<long long>(K->reward)) : TEXT(""));
                System(C, List.IsEmpty() ? TEXT("There is no work to be had here just now.") : TEXT("Work to be had:") + List);
            }
            else if (Action.rfind("take ", 0) == 0)
            {
                const auto Taken = World.takeContract(Id, Action.substr(5));
                System(C, F(Taken.message));
                if (Taken.ok)
                    Save();
            }
            else if (Action == "apprentice")
            {
                // Whether they take you on is theirs to decide: they must know and trust you (Society::apprentice).
                const auto* Npc = World.entity(Target);
                if (!Npc || !Npc->npc || World.visionClarity(Id, Target) <= 0 ||
                    std::hypot(Npc->position.x - Player->position.x, Npc->position.y - Player->position.y) > 3)
                {
                    System(C, TEXT("Come closer to the one you would learn from."));
                    return;
                }
                const auto Taken = World.apprentice(Id, Target);
                System(C, F(Taken.message));
                if (Taken.ok)
                    Save();
            }
            else if (Action == "recruit")
            {
                auto* Npc = World.entity(Target);
                if (!Npc || Target != "npc_scout" || World.visionClarity(Id, Target) <= 0 ||
                    std::hypot(Npc->position.x - Player->position.x, Npc->position.y - Player->position.y) > 3)
                {
                    System(C, TEXT("Only Bracken is available to recruit in this slice. Move closer to invite him."));
                    return;
                }
                if (CompanionOwner.count(Target))
                {
                    System(C, TEXT("That wolf is already travelling with someone."));
                    return;
                }
                CompanionOwner[Target] = Id;
                Npc->leaderId = Id;
                System(C, F(Npc->name) + TEXT(" accepts your invitation to travel together."));
                Talk(Target, Id, TEXT("Will you travel with me?"));
                Save();
            }
            else if (Action == "inspect" && !World.door(Target))
            {
                const auto* Other = World.entity(Target);
                // Missing and hidden characters have identical responses, so
                // guessed IDs cannot turn anonymous scent into an identity.
                if (!Other || (Target != Id && World.visionClarity(Id, Target) <= 0))
                {
                    System(C, TEXT("You cannot inspect someone you cannot see."));
                    return;
                }
                auto E = New();
                E->SetStringField(TEXT("type"), TEXT("inspect"));
                Text(E, TEXT("id"), Other->id);
                Text(E, TEXT("name"), Other->name);
                Text(E, TEXT("title"), Other->name);
                E->SetObjectField(TEXT("appearance"), ratwjson::Appearance(Other->appearance));
                E->SetStringField(TEXT("lifeStage"), UTF8_TO_TCHAR(ratw::lifeStageName(ratw::lifeStage(Other->age))));
                E->SetNumberField(TEXT("shoulderHeightCm"), ratw::shoulderHeightCm(Other->appearance, Other->age));
                Text(E, TEXT("description"), Other->description);
                Text(E, TEXT("posture"), Other->posture);
                Text(E, TEXT("state"), Other->state);
                E->SetStringField(TEXT("text"), F(Other->description) + TEXT(" Current posture: ") + F(Other->posture) +
                                                    TEXT(". ") + F(Other->state));
                Send(C, E);
            }
            else
            {
                Result = World.interact(Id, Target, Action);
                Report = true;
            }
            if (Player->cellId != BeforeCell)
                FollowTransition(Id, BeforeCell);
            if (Result.ok) OperatorActivity[Id] = Now();
            SaveSoon();
        }
        else if (Type == TEXT("chat"))
        {
            auto Feedback = [&](bool Accepted, const FString& Message) {
                auto E = New();
                E->SetStringField(TEXT("type"), Accepted ? TEXT("chatAccepted") : TEXT("error"));
                E->SetStringField(TEXT("context"), TEXT("chat"));
                E->SetStringField(TEXT("requestId"), String(J, TEXT("requestId")));
                Text(E, TEXT("commandId"), CommandId);
                E->SetStringField(TEXT("text"), Message);
                if (!CommandId.empty())
                    ResponseReceipts[Id][CommandId] = Encode(E);
                Send(C, E);
                SaveSoon();
            };
            FString TextValue = String(J, TEXT("text")).TrimStartAndEnd();
            if (TextValue.IsEmpty() || TextValue.Len() > 16384)
            {
                Feedback(false, TEXT("Post must contain 1–16,384 characters."));
                return;
            }
            if (LastChat.count(Id) && World.time() - LastChat[Id] < 0.5)
            {
                Feedback(false, TEXT("Please leave a moment between posts."));
                return;
            }
            LastChat[Id] = World.time();
            Player->typing = false;
            if (!World.travelState(Id).active)
                World.stop(Id);
            if (String(J, TEXT("channel")) == TEXT("ooc"))
            {
                auto E = New();
                E->SetStringField(TEXT("type"), TEXT("ooc"));
                E->SetStringField(TEXT("channel"), TEXT("ooc"));
                E->SetNumberField(TEXT("sequence"), Sequence++);
                Text(E, TEXT("speaker"), Player->name);
                E->SetStringField(TEXT("text"), TextValue);
                E->SetNumberField(TEXT("color"), Player->speakingColor);
                for (const auto& Other : Clients)
                    if (Other.IsValid())
                    {
                        const auto* Actor = World.entity(S(Other->EntityId));
                        if (Actor && Actor->cellId == Player->cellId)
                            Send(Other.Get(), E);
                    }
                Feedback(true, TEXT(""));
                return;
            }
            auto Post = ratw::parsePost(S(TextValue));
            if (!Post.ok)
            {
                Feedback(false, F(Post.error));
                return;
            }
            if (!Post.posture.empty())
            {
                World.setPosture(Id, Post.posture);
                if (Player->posture == "rising")
                    for (auto& Segment : Post.segments)
                        if (Segment.kind == "state" && Segment.text == "stands up.")
                            Segment.text = "begins to stand.";
            }
            if (Post.hasState)
                Player->state = Post.state;
            const FString Volume = String(J, TEXT("volume"));
            const ratw::Voice Voice = Volume == TEXT("whisper") ? ratw::Voice::Whisper
                                      : Volume == TEXT("yell")  ? ratw::Voice::Yell
                                                                : ratw::Voice::Speak;
            const uint64 Event = Sequence;
            const auto Heard = Publish(Id, Post, Voice);
            const auto Evidence = ratw::roleplayEvidence(Post);
            Social.record({Event, Now(), Id, Player->cellId, Evidence.words, false, Evidence.contentHash}, Heard);
            for (const auto& Pair : World.entities())
                if (Pair.second.npc && !Pair.second.transient)
                {
                    const auto Sense = World.perceive(Pair.first, Id, Voice);
                    const auto HeardSegments = ratw::perceivePost(Post, Sense.hearing, Sense.vision,
                                                                  Event * 7919 + GetTypeHash(F(Pair.first)));
                    FString Perceived;
                    for (const auto& Segment : HeardSegments)
                    {
                        if (!Perceived.IsEmpty())
                            Perceived += TEXT(" ");
                        Perceived += F(Segment.text);
                    }
                    if (Perceived.IsEmpty())
                        continue;
                    const FString Lower = Perceived.ToLower();
                    const bool Addressed = NamesWord(Lower, F(Pair.second.name).ToLower());
                    const auto Companion = CompanionOwner.find(Pair.first);
                    const bool InParty = Companion != CompanionOwner.end() && Companion->second == Id;
                    const bool Invited =
                        InParty && (Lower.Contains(TEXT("what do you think")) || Lower.Contains(TEXT("your thoughts")));
                    const bool Interject = InParty && Pair.first == "npc_scout" &&
                                           NpcLastSpeech[Pair.first] + 45 < World.time() &&
                                           (Lower.Contains(TEXT("road")) || Lower.Contains(TEXT("danger")));
                    if (Addressed || Invited || Interject)
                    {
                        Talk(Pair.first, Id, Perceived, Voice, &Sense);
                    }
                }
            Feedback(true, TEXT(""));
            OperatorActivity[Id] = Now();
        }
        if (Result.ok && (Type == TEXT("path") || Type == TEXT("travel") || Type == TEXT("trade") ||
            Type == TEXT("gather") || Type == TEXT("eat") || (Type == TEXT("move") &&
                (Number(J, TEXT("x")) != 0 || Number(J, TEXT("y")) != 0)))) OperatorActivity[Id] = Now();
        if (Report && !Result.message.empty())
            System(C, F(Result.message));
        ++Revision;
    }

    void FollowTransition(const std::string& Id, const std::string& PreviousCell)
    {
        const auto* Leader = World.entity(Id);
        if (!Leader)
            return;
        for (const auto& Entry : CompanionOwner)
            if (Entry.second == Id)
            {
                auto* Npc = World.entity(Entry.first);
                if (Npc && Npc->cellId == PreviousCell)
                {
                    Npc->cellId = Leader->cellId;
                    Npc->position = Leader->position;
                    World.stop(Npc->id);
                }
            }
    }

    // A save is made in two steps: CaptureState() copies what it needs on the game thread (cheap copies of plain
    // state), and BuildState() turns the copy into the save document, which the persistence worker can do while the
    // game carries on. Only a capture's own copies are read by BuildState().
    struct FSaveCapture
    {
        Object Accounts, Director;              // Built on the game thread; not touched there again.
        uint64 Sequence = 0, Revision = 0;
        double Time = 0;
        ratw::PersistedWorld Saved;
        std::map<std::string, ratw::Entity> Characters;
        std::vector<ratw::Entity> Npcs;          // As the world holds them, in ID order.
        std::map<std::string, std::string> CompanionOwner;
        ratw::MemoryStore Memories;
        ratw::SocialLedger Social;
        std::map<std::string, std::vector<std::string>> CommandReceipts;
        std::map<std::string, std::map<std::string, FString>> ResponseReceipts;
    };
    TSharedRef<FSaveCapture> CaptureState()
    {
        // Online characters are folded into Characters and everyone is aged, as saving always has.
        for (const auto& Pair : World.entities())
            if (!Pair.second.npc)
                Characters[Pair.first] = Pair.second;
        for (auto& Pair : Characters)
            ratw::advanceAge(Pair.second, World.calendarDays());
        auto C = MakeShared<FSaveCapture>();
        C->Accounts = Accounts.State();
        C->Director = DM.State();
        C->Sequence = Sequence;
        C->Revision = Revision;
        C->Time = World.time();
        C->Saved = World.save();
        C->Characters = Characters;
        for (const auto& Pair : World.entities())
            if (Pair.second.npc && !Pair.second.transient)   // Road folk come back from the roads' own state.
                C->Npcs.push_back(Pair.second);
        C->CompanionOwner = CompanionOwner;
        C->Memories = Memories;
        C->Social = Social;
        C->CommandReceipts = CommandReceipts;
        C->ResponseReceipts = ResponseReceipts;
        return C;
    }
    // The checkpoint document is built and read by the portable core (Core/RatwCheckpoint.h), the same code a
    // standalone server uses; the accounts and the director's receipts travel inside it as they are.
    static ratw::json::Value Portable(const Object& O)
    {
        ratw::json::Value V;
        std::string Error;
        if (!O.IsValid() || !ratw::json::parse(S(Encode(O)), V, Error))
            return ratw::json::Value::object();
        return V;
    }
    static Object Unreal(const ratw::json::Value& V)
    {
        return Decode(F(ratw::json::dump(V.isObject() ? V : ratw::json::Value::object())));
    }
    static ratw::json::Value BuildState(const FSaveCapture& C)
    {
        ratw::checkpoint::ServerState State;
        State.accounts = Portable(C.Accounts);
        State.director = Portable(C.Director);
        State.sequence = C.Sequence;
        State.revision = C.Revision;
        State.characters = C.Characters;
        State.companions = C.CompanionOwner;
        State.memories = C.Memories;
        State.social = C.Social;
        State.commandReceipts = C.CommandReceipts;
        for (const auto& [Actor, Entries] : C.ResponseReceipts)
            for (const auto& [Key, Value] : Entries)
                State.responseReceipts[Actor][Key] = S(Value);
        return ratw::checkpoint::encode(C.Saved, State, C.Npcs, C.Time);
    }
    /**
     * NPC states written from outside since this server last saved (DEV copying the live NPCs from PROD) are applied
     * over the restored checkpoint. Positions go only onto open ground in a cell that exists; purse changes are
     * balanced against the town treasury, so money stays conserved.
     */
    void ApplyExternalNpcStates()
    {
        const auto States = Persistence.ExternalNpcStates();
        if (States.IsEmpty())
            return;
        ratw::SocietyState Society = World.society().state();
        int32 Applied = 0;
        for (const auto& Pair : States)
        {
            auto* E = World.entity(S(Pair.Key));
            TSharedPtr<FJsonObject> J;
            if (!E || !E->npc || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Pair.Value), J) || !J.IsValid())
                continue;
            FString Cell;
            double X = 0, Y = 0, Number = 0;
            if (J->TryGetStringField(TEXT("cell"), Cell) && J->TryGetNumberField(TEXT("x"), X) && J->TryGetNumberField(TEXT("y"), Y))
                if (const auto* C = World.cell(S(Cell)))
                    if (const auto* T = C->tile(static_cast<int>(std::floor(X)), static_cast<int>(std::floor(Y))); T && !T->solid)
                    {
                        E->cellId = S(Cell);
                        E->position = {X, Y};
                        World.stop(E->id);
                    }
            if (J->TryGetNumberField(TEXT("age"), Number) && Number >= 0 && Number <= 200)
                E->age = static_cast<int>(Number);
            bool Dead = false;
            if (J->TryGetBoolField(TEXT("dead"), Dead) && Dead != E->dead)
                World.setDead(E->id, Dead);
            if (auto Life = Society.residents.find(E->id); Life != Society.residents.end())
            {
                if (J->TryGetNumberField(TEXT("hunger"), Number)) Life->second.hunger = FMath::Clamp(Number, 0.0, 100.0);
                if (J->TryGetNumberField(TEXT("fatigue"), Number)) Life->second.fatigue = FMath::Clamp(Number, 0.0, 100.0);
            }
            auto Account = Society.accounts.find(E->id);
            auto Treasury = Society.accounts.find("treasury");
            if (Account != Society.accounts.end() && Treasury != Society.accounts.end())
            {
                if (J->TryGetNumberField(TEXT("cash"), Number) && Number >= 0)
                {
                    int64 Delta = static_cast<int64>(Number) - Account->second.cash;
                    Delta = FMath::Min<int64>(Delta, Treasury->second.cash);          // The treasury pays in...
                    Delta = FMath::Max<int64>(Delta, -Account->second.cash);         // ...or takes back.
                    Account->second.cash += Delta;
                    Treasury->second.cash -= Delta;
                }
                const TSharedPtr<FJsonObject>* Stock = nullptr;
                if (J->TryGetObjectField(TEXT("stock"), Stock))
                    for (const auto* Item : {"herbs", "meal"})
                        if ((*Stock)->TryGetNumberField(F(Item), Number))
                            Account->second.stock[Item] = static_cast<int>(FMath::Clamp(Number, 0.0, 10000.0));
            }
            ++Applied;
        }
        if (!World.society().restore(Society))
            UE_LOG(LogTemp, Warning, TEXT("RATW copied NPC purses/needs were not valid for this world; positions and ages applied only."));
        UE_LOG(LogTemp, Display, TEXT("RATW_NPC_STATE_APPLIED count=%d"), Applied);
    }

    // The periodic checkpoint: built here, written by the persistence's worker so the game never waits on the
    // database (a large world's save takes tens of milliseconds to write). Important moments still call Save().
    static constexpr double AutosaveSeconds = 15;
    // Chat, conversation turns, doors and the like are saved within this many seconds, in the background, rather
    // than each making the game wait on a whole-world save. A crash can lose at most these seconds of such changes;
    // anything to do with money, goods, accounts or characters still calls Save() and is stored before replying.
    static constexpr double SaveSoonSeconds = 3;
    void SaveSoon()
    {
        if (SaveSoonIn < 0)
            SaveSoonIn = SaveSoonSeconds;
    }
    void Autosave()
    {
        SaveSoonIn = -1;
        // The capture is taken now; the document is made and written by the persistence worker, and the events
        // since the last save go with it.
        Persistence.QueueEvents(World.takeEvents());
        const TSharedRef<FSaveCapture> Capture = CaptureState();
        const bool Npcs = Persistence.IsDatabase();
        if (StorageReady && !Persistence.SaveInBackground(
                                [Capture, Npcs](ratw::json::Value& Document, std::string& NpcStates) {
                                    Document = BuildState(*Capture);
                                    if (Npcs)
                                        NpcStates = ratw::checkpoint::npcStates(Capture->Saved, Capture->Npcs);
                                },
                                Revision))
        {
            StorageReady = false;
            UE_LOG(LogTemp, Error, TEXT("RATW persistence commit failed: %s"), *Persistence.Error());
        }
    }
    void Save()
    {
        SaveSoonIn = -1;
        Persistence.QueueEvents(World.takeEvents());
        // Stored before returning; built on the persistence worker like any save, which keeps its record of what the
        // database holds (and so the next save's changes) exact.
        const TSharedRef<FSaveCapture> Capture = CaptureState();
        const bool Npcs = Persistence.IsDatabase();
        if (StorageReady && !Persistence.Save(
                                [Capture, Npcs](ratw::json::Value& Document, std::string& NpcStates) {
                                    Document = BuildState(*Capture);
                                    if (Npcs)
                                        NpcStates = ratw::checkpoint::npcStates(Capture->Saved, Capture->Npcs);
                                },
                                Revision))
        {
            StorageReady = false;
            UE_LOG(LogTemp, Error, TEXT("RATW persistence commit failed: %s"), *Persistence.Error());
        }
    }
    void Load(const FString& Payload)
    {
        if (Payload.IsEmpty())
            return;
        ratw::json::Value Document;
        ratw::PersistedWorld Saved;
        ratw::checkpoint::ServerState State;
        std::string Problem;
        if (!ratw::json::parse(S(Payload), Document, Problem) || !ratw::checkpoint::decode(Document, Saved, State, Problem))
        {
            StorageReady = false;
            UE_LOG(LogTemp, Error, TEXT("RATW restore rejected (%s); checkpoint preserved, autosave disabled"), *F(Problem));
            return;
        }
        FRatwAccounts RestoredAccounts;
        if (Document.has("accounts") && !RestoredAccounts.Restore(Unreal(State.accounts)))
        {
            StorageReady = false;
            UE_LOG(LogTemp, Error, TEXT("RATW invalid account checkpoint; preserved and autosave disabled.")); return;
        }
        Sequence = State.sequence;
        Revision = State.revision;
        if (Document.has("director") && !DM.Restore(Unreal(State.director)))
        {
            StorageReady = false;
            UE_LOG(LogTemp, Error, TEXT("RATW invalid operator receipt checkpoint; autosave disabled.")); return;
        }
        TSet<FString> CharacterIds;
        for (const auto& Player : Saved.players) CharacterIds.Add(F(Player.id));
        if (!RestoredAccounts.ReferencesOnly(CharacterIds))
        {
            StorageReady = false;
            UE_LOG(LogTemp, Error, TEXT("RATW character/account ownership is incomplete; autosave disabled.")); return;
        }
        const auto Restored = World.restore(Saved);
        if (!Restored.ok)
        {
            StorageReady = false;
            UE_LOG(LogTemp, Error, TEXT("RATW restore rejected; checkpoint preserved: %s"), *F(Restored.message));
            return;
        }
        Accounts = MoveTemp(RestoredAccounts);
        for (const auto& Player : Saved.players)
        {
            // Use the core's restored/normalized posture, never stale transient
            // rise/turn state from the serialized record.
            if (const auto* RestoredPlayer = World.entity(Player.id))
                Characters[Player.id] = *RestoredPlayer;
            World.removePlayer(Player.id);
        }
        for (const auto& [Npc, Owner] : State.companions)
        {
            CompanionOwner[Npc] = Owner;
            if (auto* E = World.entity(Npc))
                E->leaderId = Owner;
        }
        Memories = State.memories;
        Social.entries = State.social.entries;
        Social.points = State.social.points;
        Social.recent = State.social.recent;
        Social.sessions = State.social.sessions;
        for (const auto& [Who, Ids] : State.commandReceipts)
            for (const auto& Id : Ids)
                CommandReceipts[Who].push_back(Id);
        for (const auto& [Actor, Entries] : State.responseReceipts)
            for (const auto& [Key, Value] : Entries)
                ResponseReceipts[Actor][Key] = F(Value);
        UE_LOG(LogTemp, Display, TEXT("RATW_RESTORE characters=%d summaries=%d ledger=%d"),
               static_cast<int>(Characters.size()), static_cast<int>(Memories.summaries.size()),
               static_cast<int>(Social.entries.size()));
    }
};

ARatwGameMode::ARatwGameMode()
{
    PlayerControllerClass = ARatwPlayerController::StaticClass();
    DefaultPawnClass = nullptr;
    PrimaryActorTick.bCanEverTick = true;
}
ARatwGameMode::~ARatwGameMode() = default;
namespace
{
// -RatwServer=host:port: this process is only a client of the standalone server; it runs no world of its own.
bool ClientOfStandaloneServer()
{
    FString Address;
    return FParse::Value(FCommandLine::Get(), TEXT("RatwServer="), Address);
}
} // namespace
void ARatwGameMode::BeginPlay()
{
    Super::BeginPlay();
    if (ClientOfStandaloneServer())
        return;
    if (!Runtime.IsValid())
    {
        Runtime = MakeShared<FRatwRuntime>();
        if (!Runtime->Start())
            Runtime.Reset();
    }
}
void ARatwGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!Runtime.IsValid())
        return;
    TickAccumulator += FMath::Min(DeltaSeconds, 0.25f);
    while (TickAccumulator >= 0.05f)
    {
        Runtime->Tick(0.05);
        TickAccumulator -= 0.05f;
    }
}
void ARatwGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    if (ClientOfStandaloneServer())
        return;
    if (!Runtime.IsValid())
    {
        Runtime = MakeShared<FRatwRuntime>();
        if (!Runtime->Start())
            Runtime.Reset();
    }
    if (Runtime.IsValid())
        if (auto* C = Cast<ARatwPlayerController>(NewPlayer))
        {
            Runtime->Clients.Add(C);
            Runtime->Lobby(C);
        }
}
void ARatwGameMode::Logout(AController* Exiting)
{
    if (Runtime.IsValid())
        if (auto* C = Cast<ARatwPlayerController>(Exiting))
            Runtime->Disconnect(C);
    Super::Logout(Exiting);
}
void ARatwGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Runtime.IsValid())
        Runtime->Save();
    Runtime.Reset();
    Super::EndPlay(Reason);
}
void ARatwGameMode::HandleCommand(ARatwPlayerController* C, const FString& Json)
{
    if (Runtime.IsValid())
        Runtime->Command(C, Json);
}
