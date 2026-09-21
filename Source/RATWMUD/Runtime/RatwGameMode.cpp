#include "Runtime/RatwGameMode.h"
#include "Runtime/RatwPlayerController.h"
#include "Runtime/RatwPersistence.h"
#include "Runtime/RatwDialogueProvider.h"
#include "Runtime/RatwSocialCore.h"
#include "Runtime/RatwJson.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
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
    TArray<TWeakObjectPtr<ARatwPlayerController>> Clients;
    std::map<std::string, ratw::Entity> Characters;
    std::map<std::string, double> TypingExpiry, LastChat, NpcLastSpeech;
    std::map<std::string, std::string> CompanionOwner;
    std::set<std::string> PendingNpc;
    std::map<std::string, std::vector<std::string>> CommandReceipts;
    std::map<std::string, std::map<std::string, FString>> ResponseReceipts;
    std::map<std::string, std::string> CurrentCommands;
    uint64 Sequence = 1, Revision = 0;
    double SnapshotAccumulator = 0, SaveAccumulator = 0, AmbientAccumulator = 0;
    bool StorageReady = false, DevTools = false;

    static double Now()
    {
        return static_cast<double>(FDateTime::UtcNow().ToUnixTimestamp());
    }
    void Start()
    {
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
        FString Path = FPaths::ProjectSavedDir() / TEXT("ratw-world.sqlite");
        DevTools = FParse::Param(FCommandLine::Get(), TEXT("RatwDevTools"));
        FParse::Value(FCommandLine::Get(), TEXT("RatwSave="), Path);
        StorageReady = Persistence.Open(FPaths::ConvertRelativePathToFull(Path));
        if (StorageReady)
            Load(Persistence.Load());
        else
            UE_LOG(LogTemp, Error, TEXT("RATW persistence open failed: %s"), *Persistence.Error());
        FString Endpoint;
        FParse::Value(FCommandLine::Get(), TEXT("RatwDialogueEndpoint="), Endpoint);
        Dialogue.Configure(Endpoint);
        Memories.consolidate(Now());
        UE_LOG(LogTemp, Display, TEXT("RATW authoritative world ready; 20Hz; SQLite=%s; dialogue=%s"), *Path,
               *Dialogue.Label());
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
    void Login(ARatwPlayerController* C, const Object& Command)
    {
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
        for (const auto& Other : Clients)
            if (Other.IsValid() && Other.Get() != C && S(Other->EntityId) == Actor)
            {
                System(C, TEXT("That development character is already connected. Launch with a different -RatwIdentity "
                               "value."));
                return;
            }
        FString Name = String(Command, TEXT("name"), Id).Left(32).TrimStartAndEnd();
        if (Name.IsEmpty())
            Name = Id;
        auto& Player = World.addPlayer(Actor, S(Name));
        const auto Saved = Characters.find(Actor);
        if (Saved != Characters.end())
            Player = Saved->second;
        else
        {
            Player.description = "A road-worn quadrupedal wolf with a small shoulder satchel. Their coat and history "
                                 "are yours to imagine.";
            Player.speakingColor = static_cast<int>(Characters.size() * 9) % 32;
        }
        Player.input = {};
        Player.velocity = {};
        Player.path.clear();
        Player.typing = false;
        Player.speakingUntil = 0;
        C->EntityId = F(Actor);
        C->DevelopmentIdentity = Id;
        Characters[Actor] = Player;
        World.observe(Actor);
        ++Revision;
        System(C, TEXT("Connected to the Bent Bough. Enter to write; Shift+Enter for a new line; Escape preserves your "
                       "draft. Dialogue is authored offline unless a local provider is configured."));
        Snapshot(C);
        Save();
        UE_LOG(LogTemp, Display, TEXT("RATW_LOGIN %s connected=%d"), *C->EntityId, Clients.Num());
    }

    void Disconnect(ARatwPlayerController* C)
    {
        if (auto* E = World.entity(S(C->EntityId)))
        {
            E->typing = false;
            World.stop(E->id);
            Characters[E->id] = *E;
            World.removePlayer(E->id);
        }
        Clients.RemoveAll([C](const TWeakObjectPtr<ARatwPlayerController>& P) { return !P.IsValid() || P.Get() == C; });
        Save();
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
        SaveAccumulator += Dt;
        AmbientAccumulator += Dt;
        if (SnapshotAccumulator >= 0.2)
        {
            SnapshotAccumulator = 0;
            for (const auto& C : Clients)
                if (C.IsValid() && !C->EntityId.IsEmpty())
                    Snapshot(C.Get());
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
                if (Pair.second.npc && Pair.second.cellId == "tavern" && NpcLastSpeech[Pair.first] + 30 < World.time())
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
        auto Self = Entity(View.self, View.time);
        Self->SetNumberField(TEXT("socialXp"), Social.points[Id]);
        Self->SetNumberField(TEXT("socialLevel"), Social.level(Id));
        Self->SetNumberField(TEXT("hearing"), View.self.hearing * View.self.earHealth);
        Self->SetNumberField(TEXT("vision"), View.self.vision * View.self.eyeHealth);
        Root->SetObjectField(TEXT("self"), Self);
        auto Cell = New();
        Text(Cell, TEXT("id"), View.cell.id);
        Text(Cell, TEXT("name"), View.cell.name);
        Text(Cell, TEXT("description"), View.cell.description);
        Cell->SetNumberField(TEXT("width"), View.cell.width);
        Cell->SetNumberField(TEXT("height"), View.cell.height);
        Cell->SetBoolField(TEXT("outdoors"), View.cell.outdoors);
        Cell->SetStringField(TEXT("weather"), UTF8_TO_TCHAR(ratw::weatherName(View.cell.weather)));
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
        {
            auto J = New();
            Text(J, TEXT("id"), M.id);
            Text(J, TEXT("name"), M.name);
            J->SetNumberField(TEXT("width"), M.width);
            J->SetNumberField(TEXT("height"), M.height);
            J->SetNumberField(TEXT("x"), M.worldX);
            J->SetNumberField(TEXT("y"), M.worldY);
            J->SetNumberField(TEXT("z"), M.worldZ);
            J->SetStringField(TEXT("knowledge"), UTF8_TO_TCHAR(ratw::knowledgeName(M.knowledge)));
            J->SetBoolField(TEXT("current"), M.current);
            J->SetBoolField(TEXT("visible"), M.visible);
            Text(J, TEXT("glyphs"), std::string(M.rememberedGlyphs.begin(), M.rememberedGlyphs.end()));
            Map.Add(V(J));
        }
        Root->SetArrayField(TEXT("worldMap"), Map);
        Root->SetBoolField(TEXT("isometric"), View.isometric);
        Root->SetStringField(TEXT("connection"), TEXT("Authoritative server · 20 Hz"));
        Root->SetStringField(TEXT("dialogueProvider"), Dialogue.Label());
        Array Inventory;
        auto AddItem = [&](const TCHAR* ItemId, const TCHAR* Name, const TCHAR* Icon, const TCHAR* Description,
                           bool Equipped) {
            auto I = New();
            I->SetStringField(TEXT("id"), ItemId);
            I->SetStringField(TEXT("name"), Name);
            I->SetStringField(TEXT("icon"), Icon);
            I->SetStringField(TEXT("description"), Description);
            I->SetBoolField(TEXT("equipped"), Equipped);
            Inventory.Add(V(I));
        };
        AddItem(TEXT("satchel"), TEXT("Shoulder satchel"), TEXT("bag"),
                TEXT("A small travel bag made for a wolf's shoulders."), true);
        AddItem(TEXT("herbs"), TEXT("Dry herbs"), TEXT("herb"), TEXT("A carefully wrapped bundle of bitter leaves."),
                false);
        AddItem(TEXT("token"), TEXT("Wooden token"), TEXT("token"), TEXT("A smooth keepsake carved with a branch."),
                false);
        Root->SetArrayField(TEXT("inventory"), Inventory);
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
        Context.Activity = F(Npc->activity);
        Context.PlayerName = Identified ? F(Player->name) : TEXT("traveler");
        Context.HeardText = HeardText;
        Context.Memory = Identified ? F(Memories.recall(NpcId, PlayerId)) : FString();
        if (const auto* Cell = World.cell(Npc->cellId))
            Context.Scene = F(Cell->description);
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
        else if (Type == TEXT("typing"))
        {
            Player->typing = Bool(J, TEXT("active"));
            TypingExpiry[Id] = World.time() + 3.5;
            World.stop(Id);
        }
        else if (Type == TEXT("color"))
        {
            Player->speakingColor = static_cast<int>(FMath::Clamp(Number(J, TEXT("index")), 0.0, 31.0));
            Save();
        }
        else if (Type == TEXT("weather") && DevTools)
        {
            const auto Weather = String(J, TEXT("value"));
            World.setWeather(Player->cellId, Weather == TEXT("rain")   ? ratw::Weather::Rain
                                             : Weather == TEXT("fog")  ? ratw::Weather::Fog
                                             : Weather == TEXT("snow") ? ratw::Weather::Snow
                                                                       : ratw::Weather::Clear);
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
                if (Action == "look")
                    System(C, F(World.cell(Player->cellId)->description));
                else if (Action == "listen")
                    System(C, TEXT("You pause to listen. Voices carry according to distance, hearing, and the barriers "
                                   "between you."));
                else if (Action == "smell")
                    System(
                        C,
                        World.cell(Player->cellId)->weather == ratw::Weather::Rain
                            ? TEXT("Rain thins old scent trails. Wet timber and fresh tracks close by remain distinct.")
                            : TEXT("The near air carries woodsmoke, woolen bedding, and the overlapping scents of "
                                   "wolves."));
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
                    Player->posture = Action == "sit" ? "sitting" : Action == "lay" ? "lying" : "standing";
                    auto Post = ratw::parsePost("/" + Action);
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
            else if (Action == "inspect" && World.entity(Target))
            {
                const auto* Other = World.entity(Target);
                if (Target != Id && World.visionClarity(Id, Target) <= 0)
                {
                    System(C, TEXT("You cannot inspect someone you cannot see."));
                    return;
                }
                auto E = New();
                E->SetStringField(TEXT("type"), TEXT("inspect"));
                Text(E, TEXT("title"), Other->name);
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
            const auto Post = ratw::parsePost(S(TextValue));
            if (!Post.ok)
            {
                Feedback(false, F(Post.error));
                return;
            }
            if (!Post.posture.empty())
                Player->posture = Post.posture;
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
        }
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
        Root->SetNumberField(TEXT("sequence"), Sequence);
        Root->SetNumberField(TEXT("revision"), Revision);
        const auto Saved = World.save();
        Root->SetNumberField(TEXT("time"), Saved.time);
        for (const auto& Pair : World.entities())
            if (!Pair.second.npc)
                Characters[Pair.first] = Pair.second;
        Array Players;
        for (const auto& Pair : Characters)
            Players.Add(V(PersistEntity(Pair.second, World.time())));
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
        Sequence = static_cast<uint64>(Number(Root, TEXT("sequence"), 1));
        Revision = static_cast<uint64>(Number(Root, TEXT("revision")));
        ratw::PersistedWorld Saved;
        Saved.time = Number(Root, TEXT("time"));
        for (const auto& Value : Items(Root, TEXT("players")))
            Saved.players.push_back(ReadEntity(Value->AsObject()));
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
        if (Weather.IsValid())
            for (const auto& Pair : Weather->Values)
                Saved.weather[S(Pair.Key)] = static_cast<ratw::Weather>(static_cast<int>(Pair.Value->AsNumber()));
        const auto Restored = World.restore(Saved);
        if (!Restored.ok)
        {
            StorageReady = false;
            UE_LOG(LogTemp, Error, TEXT("RATW restore rejected; checkpoint preserved: %s"), *F(Restored.message));
            return;
        }
        for (const auto& Player : Saved.players)
        {
            Characters[Player.id] = Player;
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
        Runtime->Start();
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
        Runtime->Start();
    }
    if (auto* C = Cast<ARatwPlayerController>(NewPlayer))
        Runtime->Clients.Add(C);
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
