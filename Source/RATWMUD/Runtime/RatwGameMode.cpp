#include "Runtime/RatwGameMode.h"
#include "Runtime/RatwPlayerController.h"
#include "Runtime/RatwPersistence.h"
#include "Runtime/RatwSnapshotCodec.h"
#include "Core/RatwGame.h"
#include "Core/RatwMotionCore.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "HAL/PlatformMisc.h"
#include <map>
#include <memory>
#include <string>

// The Unreal server is the portable game server (Core/RatwGame.h) behind Unreal's networking: each player controller is
// a connection to it, and what the game sends goes back by RPC. Every rule lives in the portable core, the same code the
// standalone server (Server/ratw_server.cpp) runs, so the two cannot disagree.
namespace
{
FString F(const std::string& Value) { return FString(UTF8_TO_TCHAR(Value.c_str())); }
std::string S(const FString& Value) { return std::string(TCHAR_TO_UTF8(*Value)); }

// A world not from the database keeps its save in a private SQLite file (FRatwPersistence), as it always has.
class FRatwFileStore final : public ratw::game::Store
{
  public:
    FRatwPersistence Persistence;
    bool Opened = false;
    bool database() const override { return false; }
    std::string load() override { return Opened ? S(Persistence.Load()) : std::string("{\"schema\":-1}"); }
    bool save(ratw::DbStore::Build Build, std::uint64_t Revision) override { return Opened && Persistence.Save(MoveTemp(Build), Revision); }
    bool saveInBackground(ratw::DbStore::Build Build, std::uint64_t Revision) override
    {
        return Opened && Persistence.SaveInBackground(MoveTemp(Build), Revision);
    }
    bool flush() override { return true; }
    void queueEvents(std::vector<ratw::WorldEvent>) override {}
    std::vector<std::pair<std::string, std::string>> externalNpcStates() override { return {}; }
    std::string error() const override { return S(Persistence.Error()); }
};

// One player controller as a connection to the game.
class FRatwLink final : public ratw::game::Connection
{
  public:
    TWeakObjectPtr<ARatwPlayerController> Controller;
    void event(const std::string& Json) override
    {
        if (auto* C = Controller.Get())
            C->ClientEvent(F(Json));
    }
    void snapshot(const std::string& Json) override
    {
        if (auto* C = Controller.Get())
            C->ClientSnapshot(F(Json));
    }
    void motion(const ratw::json::Value& Frame) override
    {
        auto* C = Controller.Get();
        if (!C)
            return;
        const auto Packed = ratw::motion::pack(Frame);
        TArray<uint8> Raw(Packed.data(), int32(Packed.size())), Compressed;
        int32 RawBytes = 0;
        if (ratwwire::EncodeBytes(Raw, Compressed, RawBytes))
            C->ClientCompressedMotion(Compressed, RawBytes);
    }
    bool allowsLocalCredentials() const override
    {
        const auto* C = Controller.Get();
        return C && C->AllowsLocalCredentials();
    }
};
} // namespace

class FRatwRuntime
{
  public:
    std::unique_ptr<ratw::game::Game> Game;
    std::map<ARatwPlayerController*, std::unique_ptr<FRatwLink>> Links;

    bool Start()
    {
        const TCHAR* Line = FCommandLine::Get();
        ratw::game::Options Options;
        FString Manifest, Database, Value;
        const bool HasManifest = FParse::Value(Line, TEXT("RatwWorld="), Manifest);
        const bool Custom = HasManifest || FParse::Param(Line, TEXT("RatwWorld"));
        const bool Town = FParse::Param(Line, TEXT("RatwTown"));
        const bool Live = FParse::Value(Line, TEXT("RatwDatabase="), Database);
        if (int(Custom) + int(Town) + int(Live) > 1)
        {
            UE_LOG(LogTemp, Error, TEXT("RATW_WORLD_REJECTED: choose one of -RatwDatabase, -RatwWorld or -RatwTown."));
            return false;
        }
        Options.devTools = FParse::Param(Line, TEXT("RatwDevTools"));
        Options.devIdentity = FParse::Param(Line, TEXT("RatwDevIdentity"));
        Options.fullSnapshots = FParse::Param(Line, TEXT("RatwFullSnapshots"));
        if (FParse::Value(Line, TEXT("RatwDialogueEndpoint="), Value))
            Options.dialogueEndpoint = S(Value);
        if (FParse::Value(Line, TEXT("RatwDMDirectory="), Value))
            Options.directorDirectory = S(Value);
        FString Path = FPaths::ProjectSavedDir() / TEXT("ratw-world.sqlite");
        if (Live)
        {
            Options.database = S(Database.ToLower());
            Options.conninfo = S(FPlatformMisc::GetEnvironmentVariable(TEXT("RATW_DATABASE_URL")));
        }
        else if (Town)
        {
            // Greyfen Crossing is an Atlas Workshop world bundled with the project.
            Options.worldFile = S(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Data/Worlds/Greyfen/world.ratw")));
            Options.requireStorage = false;
            Path = FPaths::ProjectSavedDir() / TEXT("ratw-town.sqlite");
        }
        else if (Custom)
        {
            if (Manifest.IsEmpty() || FPaths::IsRelative(Manifest))
            {
                UE_LOG(LogTemp, Error, TEXT("RATW_WORLD_REJECTED: -RatwWorld requires an absolute manifest path."));
                return false;
            }
            Manifest = FPaths::ConvertRelativePathToFull(Manifest);
            Options.worldFile = S(Manifest);
            Path = FPaths::ProjectSavedDir() / TEXT("Atlas") / FMD5::HashAnsiString(*Manifest) / TEXT("ratw-world.sqlite");
        }
        else
        {
            for (const TCHAR* CellId : {TEXT("tavern"), TEXT("exterior"), TEXT("loft")})
            {
                const FString File = FPaths::ProjectDir() / TEXT("Data/Cells") / (FString(CellId) + TEXT(".cell"));
                if (FPaths::FileExists(File))
                    Options.cellFiles.push_back(S(FPaths::ConvertRelativePathToFull(File)));
            }
            Options.requireStorage = false;
        }
        FParse::Value(Line, TEXT("RatwSave="), Path);
        Path = FPaths::ConvertRelativePathToFull(Path);
        if (!Live)
            Options.savePath = S(Path);
        Game = std::make_unique<ratw::game::Game>(Options);
        Game->log = [](const char* Level, const std::string& Text) {
            const std::string L = Level;
            if (L == "error")
            {
                UE_LOG(LogTemp, Error, TEXT("%s"), *F(Text));
            }
            else if (L == "warning")
            {
                UE_LOG(LogTemp, Warning, TEXT("%s"), *F(Text));
            }
            else
            {
                UE_LOG(LogTemp, Display, TEXT("%s"), *F(Text));
            }
        };
        if (!Live)
        {
            auto Store = std::make_unique<FRatwFileStore>();
            Store->Opened = Store->Persistence.Open(Path);
            if (!Store->Opened)
                UE_LOG(LogTemp, Error, TEXT("RATW persistence open failed: %s"), *Store->Persistence.Error());
            Game->useStore(std::move(Store));
        }
        std::string Problem;
        if (!Game->start(Problem))
        {
            UE_LOG(LogTemp, Error, TEXT("RATW_WORLD_REJECTED: %s"), *F(Problem));
            Game.reset();
            return false;
        }
        return true;
    }

    FRatwLink* LinkOf(ARatwPlayerController* C)
    {
        const auto Found = Links.find(C);
        return Found == Links.end() ? nullptr : Found->second.get();
    }
    // The controller mirrors what the game knows of its connection (its character, account and motion session).
    static void Mirror(ARatwPlayerController* C, const FRatwLink& Link)
    {
        C->EntityId = F(Link.entityId);
        C->AccountUsername = F(Link.accountUsername);
        C->DevelopmentIdentity = F(Link.developmentIdentity);
        C->MotionSession = F(Link.motionSession);
        C->MotionCell = F(Link.motionCell);
        C->MotionGeneration = Link.motionGeneration;
    }
    void MirrorAll()
    {
        for (const auto& [C, Link] : Links)
            if (Link->Controller.IsValid())
                Mirror(Link->Controller.Get(), *Link);
    }
    void Connect(ARatwPlayerController* C)
    {
        if (!C || Links.count(C))
            return;
        auto Link = std::make_unique<FRatwLink>();
        Link->Controller = C;
        Link->id = C->GetUniqueID();
        auto* Raw = Link.get();
        Links[C] = std::move(Link);
        Game->connect(Raw);
        Mirror(C, *Raw);
    }
    void Disconnect(ARatwPlayerController* C)
    {
        if (auto* Link = LinkOf(C))
        {
            Game->disconnect(Link);
            Links.erase(C);
        }
    }
    void Command(ARatwPlayerController* C, const FString& Json)
    {
        if (auto* Link = LinkOf(C))
        {
            Game->command(Link, S(Json));
            MirrorAll();
        }
    }
    void Acknowledge(ARatwPlayerController* C, double Revision, bool Missing)
    {
        if (auto* Link = LinkOf(C))
            Game->acknowledge(Link, Revision, Missing);
    }
    void Tick(double Dt)
    {
        // Controllers that went away without a logout are let go first.
        for (auto It = Links.begin(); It != Links.end();)
            if (!It->second->Controller.IsValid())
            {
                Game->disconnect(It->second.get());
                It = Links.erase(It);
            }
            else
                ++It;
        Game->tick(Dt);
        MirrorAll();
        if (Game->exitRequested() >= 0)
            FPlatformMisc::RequestExitWithStatus(false, uint8(Game->exitRequested()));
    }
    void Save()
    {
        if (Game)
            Game->save();
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
bool ARatwGameMode::EnsureRuntime()
{
    if (Runtime.IsValid() || Failed || ClientOfStandaloneServer())
        return Runtime.IsValid();
    Runtime = MakeShared<FRatwRuntime>();
    if (!Runtime->Start())
    {
        Runtime.Reset();
        Failed = true;
        FPlatformMisc::RequestExitWithStatus(false, 2);
    }
    return Runtime.IsValid();
}
void ARatwGameMode::BeginPlay()
{
    Super::BeginPlay();
    EnsureRuntime();
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
    if (EnsureRuntime())
        if (auto* C = Cast<ARatwPlayerController>(NewPlayer))
            Runtime->Connect(C);
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
void ARatwGameMode::HandleAcknowledge(ARatwPlayerController* C, double Revision, bool Missing)
{
    if (Runtime.IsValid())
        Runtime->Acknowledge(C, Revision, Missing);
}
