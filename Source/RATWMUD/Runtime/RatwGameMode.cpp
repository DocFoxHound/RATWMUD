#include "Runtime/RatwGameMode.h"
#include "Runtime/RatwPlayerController.h"
#include "Runtime/RatwPersistence.h"
#include "Runtime/RatwDialogueProvider.h"
#include "Runtime/RatwSocialCore.h"
#include "Runtime/RatwJson.h"
#include "Runtime/RatwSocietyJson.h"
#include "Runtime/RatwDMBridge.h"
#include "Runtime/RatwAccounts.h"
#include "Runtime/RatwMotion.h"
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
    std::map<std::string, std::vector<std::string>> CommandReceipts;
    std::map<std::string, std::map<std::string, FString>> ResponseReceipts;
    std::map<std::string, std::string> CurrentCommands;
    uint64 Sequence = 1, Revision = 0;
    double SnapshotAccumulator = 0, SaveAccumulator = 0, AmbientAccumulator = 0;
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
        if (Custom)
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
            UE_LOG(LogTemp, Display, TEXT("RATW_WORLD_IMPORTED cells=%d fixtures=%d"),
                   static_cast<int32>(World.cells().size()), static_cast<int32>(World.doors().size()));
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
                              : FPaths::ProjectSavedDir() / TEXT("ratw-world.sqlite");
        if (!Custom) for (const auto& Pair : World.cells()) World.cell(Pair.first)->region = "demo_reach";
        DevTools = FParse::Param(FCommandLine::Get(), TEXT("RatwDevTools"));
        DevIdentity = FParse::Param(FCommandLine::Get(), TEXT("RatwDevIdentity"));
        FParse::Value(FCommandLine::Get(), TEXT("RatwSave="), Path);
        StorageReady = Persistence.Open(FPaths::ConvertRelativePathToFull(Path));
        if (StorageReady)
            Load(Persistence.Load());
        else
            UE_LOG(LogTemp, Error, TEXT("RATW persistence open failed: %s"), *Persistence.Error());
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
        Memories.consolidate(Now());
        UE_LOG(LogTemp, Display, TEXT("RATW authoritative world ready; 20Hz; SQLite=%s; dialogue=%s"), *Path,
               *Dialogue.Label());
        return true;
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
                    C->ClientMotion(Encode(Motion));
                }
        SaveAccumulator += Dt;
        AmbientAccumulator += Dt;
        if (SnapshotAccumulator >= 0.2)
        {
            SnapshotAccumulator = 0;
            MovementSounds();
            for (const auto& C : Clients)
                if (C.IsValid() && !C->EntityId.IsEmpty())
                {
                    if (auto* E = World.entity(S(C->EntityId)); E && E->ageNoticePending > 0)
                    {
                        System(C.Get(), FString::Printf(TEXT("A birthday has passed. You are now %d years old (%d year%s gained). Your character sheet reflects annual growth and age-related changes."), E->age, E->ageNoticePending, E->ageNoticePending == 1 ? TEXT("") : TEXT("s")));
                        E->ageNoticePending = 0;
                    }
                    Snapshot(C.Get());
                }
            for (const auto& C : Clients)
                if (C.IsValid())
                    if (auto* E = World.entity(S(C->EntityId)))
                        E->transitioned = false;
        }
        if (SaveAccumulator >= 5)
        {
            SaveAccumulator = 0;
            Memories.consolidate(Now());
            Social.tick(Now());
            Save();
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
        Array Tiles;
        for (int I = 0; I < static_cast<int>(View.cell.tiles.size()); ++I)
        {
            const bool Visible = I < static_cast<int>(View.visibleTiles.size()) && View.visibleTiles[I];
            const bool Remembered = I < static_cast<int>(View.rememberedTiles.size()) && View.rememberedTiles[I];
            if (!Visible && !Remembered)
                continue;
            auto T = New();
            T->SetNumberField(TEXT("x"), I % View.cell.width);
            T->SetNumberField(TEXT("y"), I / View.cell.width);
            T->SetStringField(TEXT("glyph"), FString::Chr(View.cell.tiles[I].glyph));
            T->SetNumberField(TEXT("height"), View.cell.tiles[I].height);
            T->SetBoolField(TEXT("visible"), Visible);
            T->SetBoolField(TEXT("remembered"), Remembered);
            Tiles.Add(V(T));
        }
        Cell->SetArrayField(TEXT("tiles"), Tiles);
        Root->SetObjectField(TEXT("cell"), Cell);
        Array Entities;
        for (const auto& E : View.entities)
        {
            auto J = Entity(E, View.time);
            Array Actions;
            Actions.Add(V(TEXT("inspect")));
            if (E.npc)
            {
                Actions.Add(V(TEXT("talk")));
                if (ratw::Society::merchant(E.id)) Actions.Add(V(TEXT("trade")));
                if (E.id == "npc_scout" && CompanionOwner.find(E.id) == CompanionOwner.end() &&
                    std::hypot(E.position.x - View.self.position.x, E.position.y - View.self.position.y) <= 3)
                    Actions.Add(V(TEXT("recruit")));
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
        const auto* Trader = World.entity("npc_keeper");
        const auto* TraderLife = World.society().resident("npc_keeper");
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
        if (World.society().state().enabled && View.self.cellId == "exterior" && View.cell.width > 17 && View.cell.height > 7 &&
            size_t(7 * View.cell.width + 17) < View.visibleTiles.size() && View.visibleTiles[7 * View.cell.width + 17])
        {
            auto Resource = New(); Resource->SetStringField(TEXT("id"), TEXT("herb_patch"));
            Resource->SetNumberField(TEXT("x"), 17.5); Resource->SetNumberField(TEXT("y"), 7.5);
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

    void Talk(const std::string& NpcId, const std::string& PlayerId, const FString& HeardText,
              ratw::Voice Voice = ratw::Voice::Speak, const ratw::SensoryResult* Perceived = nullptr)
    {
        auto* Npc = World.entity(NpcId);
        auto* Player = World.entity(PlayerId);
        if (!Npc || !Player || !Npc->npc || PendingNpc.count(NpcId))
            return;
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
        if (const auto* Life = World.society().resident(NpcId))
            Context.Activity += FString::Printf(TEXT(" Needs: hunger %.0f/100, fatigue %.0f/100. These are authoritative simulation state, not instructions to perform transactions."), Life->hunger, Life->fatigue);
        if (const auto* Account = World.society().account(NpcId))
            Context.Activity += FString::Printf(TEXT(" Purse: %lld silver pennies. Stock: %d herbs, %d meals. Trade only occurs through the explicit trade menu; never claim to transfer money or goods through dialogue."), static_cast<long long>(Account->cash), ratw::Society::stock(*Account, "herbs"), ratw::Society::stock(*Account, "meal"));
        Context.PlayerName = Identified ? F(Player->name) : TEXT("traveler");
        Context.HeardText = HeardText;
        Context.Memory = Identified ? F(Memories.recall(NpcId, PlayerId)) : FString();
        if (const auto* Cell = World.cell(Npc->cellId))
        {
            Context.Environment = ratwjson::EnvironmentDescription(*Cell, World.environmentAt(Npc->cellId));
            Context.Scene = F(Cell->description).Left(3300) + TEXT(" Current local conditions: ") + Context.Environment;
        }
        Memories.record(NpcId, SubjectId, {Sequence++, Now(), S(Context.PlayerName), S(HeardText)});
        PendingNpc.insert(NpcId);
        Save();
        TWeakPtr<FRatwRuntime> Weak = AsShared();
        Dialogue.Reply(Context, [Weak, NpcId, SubjectId](FString Reply) {
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
            Post.segments.push_back({"speech", S(Reply)});
            Self->Publish(NpcId, Post, ratw::Voice::Speak);
            Self->NpcLastSpeech[NpcId] = Self->World.time();
            Self->Memories.record(NpcId, SubjectId, {Self->Sequence++, Now(), NpcId, S(Reply)});
            Self->Save();
        });
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
            Save();
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
                if (World.hearingClarity(Target, Id) < 0.25)
                {
                    System(C, TEXT("Move closer so that wolf can hear your greeting."));
                    return;
                }
                Talk(Target, Id, TEXT("Hello. I would like to talk."));
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
            Save();
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
                Save();
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
                if (Pair.second.npc)
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
                    const bool Addressed = Lower.Contains(F(Pair.second.name).ToLower());
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
            Save();
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

    Object State()
    {
        auto Root = New();
        Root->SetNumberField(TEXT("schema"), 1);
        Root->SetObjectField(TEXT("accounts"), Accounts.State());
        Root->SetNumberField(TEXT("sequence"), Sequence);
        Root->SetNumberField(TEXT("revision"), Revision);
        Root->SetObjectField(TEXT("director"), DM.State());
        const auto Saved = World.save();
        Root->SetNumberField(TEXT("time"), Saved.time);
        Root->SetNumberField(TEXT("clockOffsetHours"), Saved.clockOffsetHours);
        Root->SetNumberField(TEXT("calendarDays"), Saved.calendarDays);
        Root->SetObjectField(TEXT("society"), ratwjson::Society(Saved.society));
        auto WeatherModes = New();
        for (const auto& Pair : Saved.seasonalWeather) WeatherModes->SetBoolField(F(Pair.first), Pair.second);
        Root->SetObjectField(TEXT("seasonalWeather"), WeatherModes);
        for (const auto& Pair : World.entities())
            if (!Pair.second.npc)
                Characters[Pair.first] = Pair.second;
        Array Players;
        for (auto& Pair : Characters)
        {
            ratw::advanceAge(Pair.second, World.calendarDays());
            Players.Add(V(PersistEntity(Pair.second, World.time())));
        }
        Root->SetArrayField(TEXT("players"), Players);
        auto Doors = New();
        for (const auto& Pair : Saved.doorStates)
            Doors->SetBoolField(F(Pair.first), Pair.second);
        Root->SetObjectField(TEXT("doors"), Doors);
        Array Seen;
        for (const auto& Observer : Saved.memories)
            for (const auto& Pair : Observer.second)
            {
                const auto& M = Pair.second;
                auto J = New();
                Text(J, TEXT("observer"), Observer.first);
                Text(J, TEXT("id"), M.cellId);
                Text(J, TEXT("name"), M.name);
                J->SetNumberField(TEXT("knowledge"), static_cast<int>(M.knowledge));
                J->SetNumberField(TEXT("width"), M.width);
                J->SetNumberField(TEXT("height"), M.height);
                J->SetNumberField(TEXT("x"), M.worldX);
                J->SetNumberField(TEXT("y"), M.worldY);
                J->SetNumberField(TEXT("z"), M.worldZ);
                Text(J, TEXT("glyphs"), std::string(M.glyphs.begin(), M.glyphs.end()));
                FString Bits;
                for (bool B : M.observed)
                    Bits += B ? TEXT('1') : TEXT('0');
                J->SetStringField(TEXT("observed"), Bits);
                Seen.Add(V(J));
            }
        Root->SetArrayField(TEXT("mapMemories"), Seen);
        Array Npcs;
        for (const auto& Pair : World.entities())
            if (Pair.second.npc)
                Npcs.Add(V(PersistEntity(Pair.second, World.time())));
        Root->SetArrayField(TEXT("npcs"), Npcs);
        auto Companions = New();
        for (const auto& Pair : CompanionOwner)
            if (!Pair.second.empty())
                Text(Companions, *F(Pair.first), Pair.second);
        Root->SetObjectField(TEXT("companions"), Companions);
        Root->SetNumberField(TEXT("nextConversation"), Memories.nextConversation);
        Array Active;
        for (const auto& Pair : Memories.active)
        {
            const auto& M = Pair.second;
            auto J = New();
            Text(J, TEXT("key"), Pair.first);
            Text(J, TEXT("id"), M.id);
            Text(J, TEXT("npc"), M.npc);
            Text(J, TEXT("subject"), M.subject);
            Text(J, TEXT("older"), M.olderContext);
            J->SetNumberField(TEXT("started"), M.started);
            J->SetNumberField(TEXT("lastActivity"), M.lastActivity);
            Array Turns;
            for (const auto& T : M.turns)
            {
                auto K = New();
                K->SetNumberField(TEXT("event"), T.event);
                K->SetNumberField(TEXT("at"), T.at);
                Text(K, TEXT("who"), T.who);
                Text(K, TEXT("text"), T.text);
                Turns.Add(V(K));
            }
            J->SetArrayField(TEXT("turns"), Turns);
            Active.Add(V(J));
        }
        Root->SetArrayField(TEXT("activeMemory"), Active);
        Array Summaries;
        for (const auto& M : Memories.summaries)
        {
            auto J = New();
            Text(J, TEXT("id"), M.id);
            Text(J, TEXT("npc"), M.npc);
            Text(J, TEXT("subject"), M.subject);
            Text(J, TEXT("text"), M.text);
            J->SetNumberField(TEXT("started"), M.started);
            J->SetNumberField(TEXT("consolidated"), M.consolidated);
            Array Sources;
            for (auto Id : M.sourceEvents)
                Sources.Add(MakeShared<FJsonValueNumber>(static_cast<double>(Id)));
            J->SetArrayField(TEXT("sourceEvents"), Sources);
            Summaries.Add(V(J));
        }
        Root->SetArrayField(TEXT("summaries"), Summaries);
        Array Ledger;
        for (const auto& L : Social.entries)
        {
            auto J = New();
            J->SetNumberField(TEXT("event"), L.event);
            J->SetNumberField(TEXT("at"), L.at);
            Text(J, TEXT("actor"), L.actor);
            Text(J, TEXT("partner"), L.partner);
            Text(J, TEXT("reason"), L.reason);
            Text(J, TEXT("session"), L.session);
            J->SetNumberField(TEXT("amount"), L.amount);
            Ledger.Add(V(J));
        }
        Root->SetArrayField(TEXT("ledger"), Ledger);
        Array Recent;
        for (const auto& Pair : Social.recent)
        {
            const auto& P = Pair.second;
            auto J = New();
            J->SetNumberField(TEXT("event"), P.event);
            J->SetNumberField(TEXT("at"), P.at);
            Text(J, TEXT("actor"), P.actor);
            Text(J, TEXT("cell"), P.cell);
            J->SetNumberField(TEXT("words"), P.words);
            Recent.Add(V(J));
        }
        Root->SetArrayField(TEXT("socialRecent"), Recent);
        auto Weather = New();
        for (const auto& Pair : Saved.weather)
            Weather->SetNumberField(F(Pair.first), static_cast<int>(Pair.second));
        Root->SetObjectField(TEXT("weather"), Weather);
        auto Winds = New();
        for (const auto& Pair : Saved.winds)
            Winds->SetObjectField(F(Pair.first), ratwjson::Wind(Pair.second));
        Root->SetObjectField(TEXT("winds"), Winds);
        auto Lighting = New();
        for (const auto& Pair : Saved.lighting)
            Lighting->SetObjectField(F(Pair.first), ratwjson::Lighting(Pair.second));
        Root->SetObjectField(TEXT("lighting"), Lighting);
        auto Receipts = New();
        for (const auto& Pair : CommandReceipts)
        {
            Array List;
            for (const auto& Id : Pair.second)
                List.Add(V(F(Id)));
            Receipts->SetArrayField(F(Pair.first), List);
        }
        Root->SetObjectField(TEXT("commandReceipts"), Receipts);
        Array Sessions;
        for (const auto& Pair : Social.sessions)
        {
            const auto& Session = Pair.second;
            auto J = New();
            Text(J, TEXT("id"), Session.id);
            Text(J, TEXT("cell"), Session.cell);
            J->SetNumberField(TEXT("started"), Session.started);
            J->SetNumberField(TEXT("last"), Session.last);
            J->SetNumberField(TEXT("ended"), Session.ended);
            Array Members;
            for (const auto& Member : Session.members)
            {
                auto K = New();
                Text(K, TEXT("actor"), Member.first);
                K->SetNumberField(TEXT("turns"), Member.second.turns);
                K->SetNumberField(TEXT("words"), Member.second.words);
                K->SetNumberField(TEXT("replies"), Member.second.replies);
                K->SetNumberField(TEXT("last"), Member.second.last);
                K->SetNumberField(TEXT("joined"), Member.second.joined);
                Members.Add(V(K));
            }
            J->SetArrayField(TEXT("members"), Members);
            Sessions.Add(V(J));
        }
        Root->SetArrayField(TEXT("socialSessions"), Sessions);
        auto OlderEvents = New();
        for (const auto& Pair : Memories.active)
        {
            Array Events;
            for (auto Event : Pair.second.olderEvents)
                Events.Add(MakeShared<FJsonValueNumber>(static_cast<double>(Event)));
            OlderEvents->SetArrayField(F(Pair.first), Events);
        }
        Root->SetObjectField(TEXT("olderMemoryEvents"), OlderEvents);
        auto ResponseCache = New();
        for (const auto& Actor : ResponseReceipts)
        {
            auto Values = New();
            for (const auto& Entry : Actor.second)
                Values->SetStringField(F(Entry.first), Entry.second);
            ResponseCache->SetObjectField(F(Actor.first), Values);
        }
        Root->SetObjectField(TEXT("responseReceipts"), ResponseCache);
        auto Audiences = New();
        for (const auto& Scene : Social.sessions)
            for (const auto& Member : Scene.second.members)
            {
                Array List;
                for (const auto& Id : Member.second.lastAudience)
                    List.Add(V(F(Id)));
                Audiences->SetArrayField(F(Scene.first + "|" + Member.first), List);
            }
        Root->SetObjectField(TEXT("socialAudiences"), Audiences);
        return Root;
    }
    void Save()
    {
        if (StorageReady && !Persistence.Save(Encode(State()), Revision))
        {
            StorageReady = false;
            UE_LOG(LogTemp, Error, TEXT("RATW persistence commit failed: %s"), *Persistence.Error());
        }
    }
    void Load(const FString& Payload)
    {
        if (Payload.IsEmpty())
            return;
        auto Root = Decode(Payload);
        if (!Root.IsValid() || Number(Root, TEXT("schema")) != 1)
        {
            StorageReady = false;
            UE_LOG(LogTemp, Error, TEXT("RATW invalid save schema; checkpoint preserved, autosave disabled"));
            return;
        }
        FRatwAccounts RestoredAccounts;
        if (Root->HasField(TEXT("accounts")) && !RestoredAccounts.Restore(Child(Root, TEXT("accounts"))))
        {
            StorageReady = false;
            UE_LOG(LogTemp, Error, TEXT("RATW invalid account checkpoint; preserved and autosave disabled.")); return;
        }
        Sequence = static_cast<uint64>(Number(Root, TEXT("sequence"), 1));
        Revision = static_cast<uint64>(Number(Root, TEXT("revision")));
        if (Root->HasField(TEXT("director")) && !DM.Restore(Child(Root, TEXT("director"))))
        {
            StorageReady = false;
            UE_LOG(LogTemp, Error, TEXT("RATW invalid operator receipt checkpoint; autosave disabled.")); return;
        }
        ratw::PersistedWorld Saved;
        Saved.time = Number(Root, TEXT("time"));
        Saved.clockOffsetHours = ratwjson::ReadClockOffset(Root);
        Saved.calendarDays = Root->HasField(TEXT("calendarDays")) ? StrictNumber(Root, TEXT("calendarDays"), -2) : -1;
        Saved.hasSociety = Root->HasField(TEXT("society"));
        if (Saved.hasSociety) Saved.society = ratwjson::ReadSociety(Child(Root, TEXT("society")));
        auto Modes = Child(Root, TEXT("seasonalWeather"));
        if (Root->HasField(TEXT("seasonalWeather")) && !Modes.IsValid()) Saved.calendarDays = -2;
        if (Modes.IsValid()) for (const auto& Pair : Modes->Values)
        {
            if (Pair.Value->Type != EJson::Boolean) Saved.calendarDays = -2;
            else Saved.seasonalWeather[S(Pair.Key)] = Pair.Value->AsBool();
        }
        for (const auto& Value : Items(Root, TEXT("players")))
            Saved.players.push_back(ReadEntity(Value->AsObject()));
        TSet<FString> CharacterIds;
        for (const auto& Player : Saved.players) CharacterIds.Add(F(Player.id));
        if (!RestoredAccounts.ReferencesOnly(CharacterIds))
        {
            StorageReady = false;
            UE_LOG(LogTemp, Error, TEXT("RATW character/account ownership is incomplete; autosave disabled.")); return;
        }
        auto Doors = Child(Root, TEXT("doors"));
        if (Doors.IsValid())
            for (const auto& Pair : Doors->Values)
                Saved.doorStates[S(Pair.Key)] = Pair.Value->AsBool();
        for (const auto& Value : Items(Root, TEXT("mapMemories")))
        {
            auto J = Value->AsObject();
            ratw::CellMemory M;
            M.cellId = S(String(J, TEXT("id")));
            M.name = S(String(J, TEXT("name")));
            M.knowledge = static_cast<ratw::Knowledge>(static_cast<int>(Number(J, TEXT("knowledge"))));
            M.width = static_cast<int>(Number(J, TEXT("width")));
            M.height = static_cast<int>(Number(J, TEXT("height")));
            M.worldX = Number(J, TEXT("x"));
            M.worldY = Number(J, TEXT("y"));
            M.worldZ = Number(J, TEXT("z"));
            const auto Glyphs = S(String(J, TEXT("glyphs")));
            M.glyphs.assign(Glyphs.begin(), Glyphs.end());
            const FString Bits = String(J, TEXT("observed"));
            for (TCHAR B : Bits)
                M.observed.push_back(B == TEXT('1'));
            Saved.memories[S(String(J, TEXT("observer")))][M.cellId] = M;
        }
        for (const auto& Value : Items(Root, TEXT("npcs")))
            Saved.npcs.push_back(ReadEntity(Value->AsObject()));
        auto Weather = Child(Root, TEXT("weather"));
        if (Root->HasField(TEXT("weather")) && !Weather.IsValid())
        {
            StorageReady = false;
            UE_LOG(LogTemp, Error, TEXT("RATW restore rejected; invalid weather record, checkpoint preserved."));
            return;
        }
        if (Weather.IsValid())
            for (const auto& Pair : Weather->Values)
                Saved.weather[S(Pair.Key)] = ratwjson::ReadWeather(Pair.Value);
        auto Winds = Child(Root, TEXT("winds"));
        if (Root->HasField(TEXT("winds")) && !Winds.IsValid())
        {
            StorageReady = false;
            UE_LOG(LogTemp, Error, TEXT("RATW restore rejected; invalid wind record, checkpoint preserved."));
            return;
        }
        if (Winds.IsValid())
            for (const auto& Pair : Winds->Values)
                Saved.winds[S(Pair.Key)] =
                    ratwjson::ReadWind(Pair.Value->Type == EJson::Object ? Pair.Value->AsObject() : Object());
        auto Lighting = Child(Root, TEXT("lighting"));
        if (Root->HasField(TEXT("lighting")) && !Lighting.IsValid())
        {
            StorageReady = false;
            UE_LOG(LogTemp, Error, TEXT("RATW restore rejected; invalid lighting record, checkpoint preserved."));
            return;
        }
        if (Lighting.IsValid())
            for (const auto& Pair : Lighting->Values)
                Saved.lighting[S(Pair.Key)] = ratwjson::ReadLighting(
                    Pair.Value.IsValid() && Pair.Value->Type == EJson::Object ? Pair.Value->AsObject() : Object());
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
        auto Companions = Child(Root, TEXT("companions"));
        if (Companions.IsValid())
            for (const auto& Pair : Companions->Values)
            {
                CompanionOwner[S(Pair.Key)] = S(Pair.Value->AsString());
                if (auto* Npc = World.entity(S(Pair.Key)))
                    Npc->leaderId = CompanionOwner[S(Pair.Key)];
            }
        Memories.nextConversation = static_cast<uint64>(Number(Root, TEXT("nextConversation"), 1));
        for (const auto& Value : Items(Root, TEXT("activeMemory")))
        {
            auto J = Value->AsObject();
            ratw::ActiveMemory M;
            M.id = S(String(J, TEXT("id")));
            M.npc = S(String(J, TEXT("npc")));
            M.subject = S(String(J, TEXT("subject")));
            M.olderContext = S(String(J, TEXT("older")));
            M.started = Number(J, TEXT("started"));
            M.lastActivity = Number(J, TEXT("lastActivity"));
            for (const auto& Turn : Items(J, TEXT("turns")))
            {
                auto K = Turn->AsObject();
                M.turns.push_back({static_cast<uint64>(Number(K, TEXT("event"))), Number(K, TEXT("at")),
                                   S(String(K, TEXT("who"))), S(String(K, TEXT("text")))});
            }
            Memories.active[S(String(J, TEXT("key")))] = M;
        }
        for (const auto& Value : Items(Root, TEXT("summaries")))
        {
            auto J = Value->AsObject();
            ratw::MemorySummary M;
            M.id = S(String(J, TEXT("id")));
            M.npc = S(String(J, TEXT("npc")));
            M.subject = S(String(J, TEXT("subject")));
            M.text = S(String(J, TEXT("text")));
            M.started = Number(J, TEXT("started"));
            M.consolidated = Number(J, TEXT("consolidated"));
            for (const auto& Source : Items(J, TEXT("sourceEvents")))
                M.sourceEvents.push_back(static_cast<uint64>(Source->AsNumber()));
            Memories.summaries.push_back(M);
        }
        for (const auto& Value : Items(Root, TEXT("ledger")))
        {
            auto J = Value->AsObject();
            ratw::LedgerEntry L;
            L.event = static_cast<uint64>(Number(J, TEXT("event")));
            L.at = Number(J, TEXT("at"));
            L.actor = S(String(J, TEXT("actor")));
            L.partner = S(String(J, TEXT("partner")));
            L.reason = S(String(J, TEXT("reason")));
            L.session = S(String(J, TEXT("session")));
            L.amount = static_cast<int>(Number(J, TEXT("amount")));
            Social.entries.push_back(L);
            Social.points[L.actor] += L.amount;
        }
        for (const auto& Value : Items(Root, TEXT("socialRecent")))
        {
            auto J = Value->AsObject();
            ratw::SocialPost P;
            P.event = static_cast<uint64>(Number(J, TEXT("event")));
            P.at = Number(J, TEXT("at"));
            P.actor = S(String(J, TEXT("actor")));
            P.cell = S(String(J, TEXT("cell")));
            P.words = static_cast<int>(Number(J, TEXT("words")));
            Social.recent[P.actor] = P;
        }
        auto Receipts = Child(Root, TEXT("commandReceipts"));
        if (Receipts.IsValid())
            for (const auto& Pair : Receipts->Values)
                for (const auto& Receipt : Pair.Value->AsArray())
                    CommandReceipts[S(Pair.Key)].push_back(S(Receipt->AsString()));
        for (const auto& Value : Items(Root, TEXT("socialSessions")))
        {
            auto J = Value->AsObject();
            ratw::SocialSession Scene;
            Scene.id = S(String(J, TEXT("id")));
            Scene.cell = S(String(J, TEXT("cell")));
            Scene.started = Number(J, TEXT("started"));
            Scene.last = Number(J, TEXT("last"));
            Scene.ended = Number(J, TEXT("ended"));
            for (const auto& Value2 : Items(J, TEXT("members")))
            {
                auto K = Value2->AsObject();
                ratw::Contribution C;
                C.turns = static_cast<int>(Number(K, TEXT("turns")));
                C.words = static_cast<int>(Number(K, TEXT("words")));
                C.replies = static_cast<int>(Number(K, TEXT("replies")));
                C.last = Number(K, TEXT("last"));
                C.joined = Number(K, TEXT("joined"));
                Scene.members[S(String(K, TEXT("actor")))] = C;
            }
            Social.sessions[Scene.id] = Scene;
        }
        auto OlderEvents = Child(Root, TEXT("olderMemoryEvents"));
        if (OlderEvents.IsValid())
            for (const auto& Pair : OlderEvents->Values)
                for (const auto& Event : Pair.Value->AsArray())
                    Memories.active[S(Pair.Key)].olderEvents.push_back(static_cast<uint64>(Event->AsNumber()));
        auto ResponseCache = Child(Root, TEXT("responseReceipts"));
        if (ResponseCache.IsValid())
            for (const auto& Actor : ResponseCache->Values)
            {
                auto Values = Actor.Value->AsObject();
                if (Values.IsValid())
                    for (const auto& Entry : Values->Values)
                        ResponseReceipts[S(Actor.Key)][S(Entry.Key)] = Entry.Value->AsString();
            }
        auto Audiences = Child(Root, TEXT("socialAudiences"));
        for (auto& Scene : Social.sessions)
            for (auto& Member : Scene.second.members)
                for (const auto& Id : Items(Audiences, *F(Scene.first + "|" + Member.first)))
                    Member.second.lastAudience.push_back(S(Id->AsString()));
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
void ARatwGameMode::BeginPlay()
{
    Super::BeginPlay();
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
