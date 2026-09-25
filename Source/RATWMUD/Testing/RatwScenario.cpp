#include "Testing/RatwScenario.h"
#include "Testing/RatwCharacterScenario.h"
#include "Runtime/RatwPlayerController.h"
#include "Runtime/RatwJson.h"
#include "UI/SRatwGame.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

namespace
{
FTSTicker::FDelegateHandle ScenarioHandle;
FString Scenario, Role, Output;
double Started = -1;
int32 Step = 0;
TWeakObjectPtr<ARatwPlayerController> Controller;
double InitialX = 0, FinalX = 0;
int32 MaxEntities = 0;
double StepAt = 0;
TSharedPtr<FJsonObject> DialogueEvidence;
TSharedPtr<FJsonValue> DialogueInventoryBefore;
double DialogueXpBefore = 0, DialogueLevelBefore = 0, DialogueTurnsBefore = 0;
double DialogueBaselineSequence = 0;
double MovementStartX = 0;
bool SawPartialTurn = false, SawSitPause = false, SawCrouch = false;

TSharedPtr<FJsonObject> Parse(const FString& Text)
{
    TSharedPtr<FJsonObject> Result;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Result);
    return Result;
}

void Capture(const FString& Filename)
{
    IFileManager::Get().MakeDirectory(*Output, true);
    FScreenshotRequest::RequestScreenshot(Output / Filename, true, false, false, FIntRect(), true);
    UE_LOG(LogTemp, Display, TEXT("RATW_CAPTURE %s"), *(Output / Filename));
}

void Finish(bool Success, const FString& Detail)
{
    auto Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("scenario"), Scenario);
    Result->SetStringField(TEXT("role"), Role);
    Result->SetBoolField(TEXT("passed"), Success);
    Result->SetStringField(TEXT("detail"), Detail);
    Result->SetNumberField(TEXT("initialX"), InitialX);
    Result->SetNumberField(TEXT("finalX"), FinalX);
    Result->SetNumberField(TEXT("maxVisibleEntities"), MaxEntities);
    if (DialogueEvidence.IsValid())
        Result->SetObjectField(TEXT("dialogueEvidence"), DialogueEvidence);
    TArray<TSharedPtr<FJsonValue>> Events;
    if (Controller.IsValid())
    {
        Result->SetNumberField(TEXT("motionFrames"), Controller->GetMotionFrameCount());
        Result->SetNumberField(TEXT("fullSnapshots"), Controller->GetSnapshotCount());
        for (const auto& Event : Controller->GetReceivedEvents())
            if (auto Object = Parse(Event))
                Events.Add(MakeShared<FJsonValueObject>(Object));
        if (auto Snapshot = Parse(Controller->GetLatestSnapshotJson()))
            Result->SetObjectField(TEXT("lastSnapshot"), Snapshot);
    }
    Result->SetArrayField(TEXT("events"), Events);
    FString Json;
    FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Json));
    IFileManager::Get().MakeDirectory(*Output, true);
    FFileHelper::SaveStringToFile(Json, *(Output / (Scenario + TEXT("-") + Role + TEXT(".json"))),
                                  FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Display, TEXT("RATW_SCENARIO %s %s"), Success ? TEXT("PASS") : TEXT("FAIL"), *Detail);
    FPlatformMisc::RequestExitWithStatus(false, Success ? 0 : 1);
}

bool Tick(float)
{
    static const double Deadline = FPlatformTime::Seconds() + 100;
    if (!Controller.IsValid())
    {
        if (GEngine)
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
                if (UWorld* World = Context.World())
                    if (World->IsGameWorld())
                        if (auto* Player = Cast<ARatwPlayerController>(World->GetFirstPlayerController()))
                            if (Player->IsLocalController())
                                Controller = Player;
        if (!Controller.IsValid())
        {
            if (FPlatformTime::Seconds() > Deadline)
            {
                Finish(false, TEXT("No local player controller"));
                return false;
            }
            return true;
        }
    }
    if (Scenario.StartsWith(TEXT("characters-")))
        return TickRatwCharacterScenario(Controller.Get(), Scenario, Role, Output);
    auto Snapshot = Parse(Controller->GetLatestSnapshotJson());
    const TSharedPtr<FJsonObject>* Self = nullptr;
    if (!Snapshot.IsValid() || !Snapshot->TryGetObjectField(TEXT("self"), Self))
    {
        if (FPlatformTime::Seconds() > Deadline)
        {
            Finish(false, TEXT("No authoritative snapshot"));
            return false;
        }
        return true;
    }
    if (Started < 0)
    {
        Started = FPlatformTime::Seconds();
        (*Self)->TryGetNumberField(TEXT("x"), InitialX);
        UE_LOG(LogTemp, Display, TEXT("RATW_SCENARIO_READY %s"), *Role);
    }
    (*Self)->TryGetNumberField(TEXT("x"), FinalX);
    const TArray<TSharedPtr<FJsonValue>>* Entities;
    if (Snapshot->TryGetArrayField(TEXT("entities"), Entities))
        MaxEntities = FMath::Max(MaxEntities, Entities->Num());
    const double Elapsed = FPlatformTime::Seconds() - Started;
    if (Elapsed > 120)
    {
        Finish(false, FString::Printf(TEXT("Timed out at step %d"), Step));
        return false;
    }
    auto Send = [](const TCHAR* Json) { Controller->SubmitCommand(Json); };
    auto Widget = Controller->GetGameWidget();
    double Y = 0;
    (*Self)->TryGetNumberField(TEXT("y"), Y);
    const auto Near = [&](double X, double TargetY) {
        return FMath::Abs(FinalX - X) < 0.3 && FMath::Abs(Y - TargetY) < 0.3;
    };
    const auto Next = [&]() {
        ++Step;
        StepAt = Elapsed;
    };
    FString CurrentCell;
    (*Self)->TryGetStringField(TEXT("cell"), CurrentCell);

    if (Scenario == TEXT("dm-observer"))
    {
        if (Snapshot->HasField(TEXT("director")) || Snapshot->HasField(TEXT("characters")) || Snapshot->HasField(TEXT("accounts")))
        { Finish(false, TEXT("Private operator data leaked into a player snapshot")); return false; }
        if (Step == 0 && Elapsed > 1)
        {
            Send(TEXT("{\"type\":\"economy_transfer\",\"from\":\"treasury\",\"to\":\"player-ash\",\"coins\":500,\"commandId\":\"public-dm-probe\"}"));
            Send(TEXT("{\"type\":\"action\",\"action\":\"look\",\"target\":\"\",\"commandId\":\"dm-observer-active\"}"));
            Next();
        }
        bool Done = false;
        for (const auto& Event : Controller->GetReceivedEvents()) Done |= Event.Contains(TEXT("DM_SMOKE_DONE"));
        if (Done)
        {
            const bool Good = ratwjson::Number(*Self, TEXT("cash")) == 23;
            if (!Good && Elapsed - StepAt < 110) return true;
            Finish(Good, Good ? TEXT("Separate operator notice reaches the real client; private finite transfer applies once; public command cannot mint money; player snapshot excludes operator records")
                              : TEXT("Operator transfer or public command boundary mismatch"));
            return false;
        }
        return true;
    }

    if (Scenario == TEXT("age-register") || Scenario == TEXT("age-advance") || Scenario == TEXT("age-return"))
    {
        using namespace ratwjson;
        if (Scenario == TEXT("age-advance") && Step == 0 && Elapsed > 1)
        {
            const TArray<TSharedPtr<FJsonValue>>* Nearby = nullptr;
            if (!Snapshot->TryGetArrayField(TEXT("entities"), Nearby)) return true;
            bool AshPresent = false;
            for (const auto& Other : *Nearby)
                AshPresent |= String(Other->AsObject(), TEXT("id")) == TEXT("player-ash");
            if (AshPresent)
            {
                if (Elapsed < 12) return true;
                Finish(false, TEXT("Logged-out Ash is still present before the calendar advance")); return false;
            }
            Send(TEXT("{\"type\":\"calendar\",\"value\":\"year\",\"commandId\":\"offline-aging-year-probe\"}")); Next();
        }
        else if (Elapsed > 1 && Scenario != TEXT("age-advance"))
        {
            int Notices = 0;
            for (const auto& Event : Controller->GetReceivedEvents()) if (Event.Contains(TEXT("A birthday has passed"))) ++Notices;
            const bool Returning = Scenario == TEXT("age-return");
            const bool Good = Number(*Self, TEXT("age")) == (Returning ? 19 : 18) &&
                Number(*Self, TEXT("strength")) == (Returning ? 51 : 50) && Number(*Self, TEXT("cash")) == 20 &&
                Notices == (Returning ? 1 : 0);
            if (!Good && Elapsed < 12) return true;
            Finish(Good, Good ? (Returning ? TEXT("Offline character ages once on the running shared server, receives one notice, and keeps the same purse")
                                          : TEXT("Fresh character registered at age eighteen with a single finite grant"))
                              : TEXT("Offline birthday or finite welcome grant mismatch")); return false;
        }
        else if (Scenario == TEXT("age-advance") && Step == 1 && Elapsed - StepAt > 1 &&
                 (Number(*Self, TEXT("age")) == 19 || Elapsed - StepAt > 12))
        {
            if (Number(*Self, TEXT("age")) != 19)
            { Finish(false, TEXT("Second character could not advance the shared calendar")); return false; }
            Send(TEXT("{\"type\":\"calendar\",\"value\":\"year\",\"commandId\":\"offline-aging-year-probe\"}")); Next();
        }
        else if (Scenario == TEXT("age-advance") && Step == 2 && Elapsed - StepAt > 1)
        {
            int Notices = 0, CalendarReplies = 0;
            for (const auto& Event : Controller->GetReceivedEvents())
            {
                Notices += Event.Contains(TEXT("A birthday has passed")) ? 1 : 0;
                CalendarReplies += Event.Contains(TEXT("Shared calendar advanced.")) ? 1 : 0;
            }
            const bool Good = Number(*Self, TEXT("age")) == 19 && Notices == 1 && CalendarReplies == 2;
            if (!Good && Elapsed - StepAt < 12) return true;
            Finish(Good, Good ? TEXT("Calendar advances with Ash absent; a retried command replays its own receipt, not a second birthday or reward")
                              : TEXT("Command retry changed age, duplicated a birthday, or lost its original receipt")); return false;
        }
        return true;
    }

    if (Scenario == TEXT("society-restore"))
    {
        if (Elapsed < 1) return true;
        FString PreviousText;
        FFileHelper::LoadFileToString(PreviousText, *(Output / TEXT("society-ash.json")));
        const auto Previous = ratwjson::Child(Parse(PreviousText), TEXT("lastSnapshot"));
        const auto OldSelf = ratwjson::Child(Previous, TEXT("self"));
        const auto Date = ratwjson::Child(ratwjson::Child(ratwjson::Child(Snapshot, TEXT("cell")), TEXT("environment")), TEXT("calendar"));
        bool BirthdayRepeated = false;
        for (const auto& Event : Controller->GetReceivedEvents()) BirthdayRepeated |= Event.Contains(TEXT("A birthday has passed"));
        const bool Good = OldSelf.IsValid() && ratwjson::Number(*Self, TEXT("age")) == ratwjson::Number(OldSelf, TEXT("age")) &&
            ratwjson::Number(*Self, TEXT("cash")) == ratwjson::Number(OldSelf, TEXT("cash")) &&
            ratwjson::Number(*Self, TEXT("strength")) == ratwjson::Number(OldSelf, TEXT("strength")) &&
            ratwjson::Number(Date, TEXT("year")) == 2 && !BirthdayRepeated && ratwjson::Bool(Snapshot, TEXT("persistenceHealthy"));
        Finish(Good, Good ? TEXT("Calendar, birthday reward, finite purse and sent-notice state survive graceful process restart")
                          : TEXT("Saved calendar, purse or birthday notice changed on restart"));
        return false;
    }
    if (Scenario == TEXT("society"))
    {
        using namespace ratwjson;
        static double Cash = 0, Price = 0, Age = 0, Strength = 0;
        static int Meals = 0, Herbs = 0;
        const auto Count = [&](const TCHAR* Id) { for (const auto& Value : Items(Snapshot, TEXT("inventory"))) if (String(Value->AsObject(), TEXT("id")) == Id) return int(Number(Value->AsObject(), TEXT("quantity"))); return 0; };
        const auto Check = [&](bool Good, const TCHAR* Detail) { if (!Good) Finish(false, Detail); return Good; };
        const auto Shot = [&](const TCHAR* Name) { if (FParse::Param(FCommandLine::Get(), TEXT("RatwCaptureSociety"))) Capture(Name); };
        const auto Merchant = Child(Snapshot, TEXT("merchant"));
        const auto Date = Child(Child(Child(Snapshot, TEXT("cell")), TEXT("environment")), TEXT("calendar"));
        if (Step == 0 && Elapsed > 1)
        {
            if (!Check(Number(*Self, TEXT("age")) == 18 && Number(*Self, TEXT("cash")) == 20, TEXT("Fresh character did not receive finite welcome allocation and starting age"))) return false;
            Send(TEXT("{\"type\":\"time\",\"value\":\"day\"}"));
            Send(TEXT("{\"type\":\"path\",\"x\":10.5,\"y\":6.5}")); Next();
        }
        else if (Step == 1 && Near(10.5, 6.5) && Merchant.IsValid())
        {
            if (Widget) Widget->SetPresentationPage(TEXT("trade"));
            Cash = Number(*Self, TEXT("cash")); Meals = Count(TEXT("meal")); Herbs = Count(TEXT("herbs"));
            for (const auto& Item : Items(Merchant, TEXT("items"))) if (String(Item->AsObject(), TEXT("id")) == TEXT("meal")) Price = Number(Item->AsObject(), TEXT("buyPrice"));
            Next();
        }
        else if (Step == 2 && Elapsed - StepAt > 1)
        {
            Shot(TEXT("29-finite-merchant.png"));
            Send(TEXT("{\"type\":\"trade\",\"target\":\"npc_keeper\",\"item\":\"meal\",\"quantity\":1,\"buy\":true}")); Next();
        }
        else if (Step == 3 && Elapsed - StepAt > 1 && (Count(TEXT("meal")) == Meals + 1 || Elapsed - StepAt > 12))
        {
            if (!Check(Count(TEXT("meal")) == Meals + 1 && Number(*Self, TEXT("cash")) == Cash - Price, TEXT("Buying did not atomically move real cash and a meal"))) return false;
            Cash = Number(*Self, TEXT("cash"));
            for (const auto& Item : Items(Merchant, TEXT("items"))) if (String(Item->AsObject(), TEXT("id")) == TEXT("herbs")) Price = Number(Item->AsObject(), TEXT("sellPrice"));
            Send(TEXT("{\"type\":\"trade\",\"target\":\"npc_keeper\",\"item\":\"herbs\",\"quantity\":1,\"buy\":false}")); Next();
        }
        else if (Step == 4 && Elapsed - StepAt > 1 && (Count(TEXT("herbs")) == Herbs - 1 || Elapsed - StepAt > 12))
        {
            if (!Check(Count(TEXT("herbs")) == Herbs - 1 && Number(*Self, TEXT("cash")) == Cash + Price, TEXT("Selling did not atomically move real cash and herbs"))) return false;
            Cash = Number(*Self, TEXT("cash"));
            Send(TEXT("{\"type\":\"trade\",\"target\":\"npc_keeper\",\"item\":\"junk\",\"quantity\":1,\"buy\":false}"));
            Send(TEXT("{\"type\":\"eat\"}")); Next();
        }
        else if (Step == 5 && Elapsed - StepAt > 1 && (Count(TEXT("meal")) == Meals || Elapsed - StepAt > 12))
        {
            if (!Check(Count(TEXT("meal")) == Meals && Number(*Self, TEXT("cash")) == Cash, TEXT("Eating or refusal of unwanted goods violated inventory/cash"))) return false;
            Age = Number(*Self, TEXT("age")); Strength = Number(*Self, TEXT("strength"));
            Send(TEXT("{\"type\":\"calendar\",\"value\":\"year\"}")); Next();
        }
        else if (Step == 6 && Elapsed - StepAt > 1 && (Number(*Self, TEXT("age")) == Age + 1 || Elapsed - StepAt > 12))
        {
            bool Notified = false;
            for (const auto& Event : Controller->GetReceivedEvents()) Notified |= Event.Contains(TEXT("A birthday has passed"));
            if (!Check(Number(*Self, TEXT("age")) == Age + 1 && Number(*Self, TEXT("strength")) == Strength + 1 && Number(Date, TEXT("year")) == 2 && Notified, TEXT("Annual age, stats, calendar or notification failed"))) return false;
            if (Widget) Widget->SetPresentationPage(TEXT("character")); Next();
        }
        else if (Step == 7 && Elapsed - StepAt > 1)
        {
            Shot(TEXT("30-birthday-character.png"));
            Next();
        }
        else if (Step == 8 && Elapsed - StepAt > 1)
        {
            if (Widget) Widget->SetPresentationPage(TEXT("local"));
            Shot(TEXT("32-birthday-notice.png"));
            Send(TEXT("{\"type\":\"path\",\"x\":20.5,\"y\":11.5}")); Next();
        }
        else if (Step == 9 && Near(20.5, 11.5))
        {
            if (!Check(!Merchant.IsValid(), TEXT("Remote trader inventory leaked after leaving reach"))) return false;
            Send(TEXT("{\"type\":\"trade\",\"target\":\"npc_keeper\",\"item\":\"meal\",\"quantity\":1,\"buy\":true}"));
            Send(TEXT("{\"type\":\"time\",\"value\":\"night\"}")); Next();
        }
        else if (Step == 10 && Elapsed - StepAt > 8)
        {
            if (!Check(Number(*Self, TEXT("cash")) == Cash && Number(Date, TEXT("moonIllumination")) > .1 && Bool(Snapshot, TEXT("persistenceHealthy")), TEXT("Remote trade guard, lunar progression or persistence failed"))) return false;
            Shot(TEXT("31-nightly-rest.png")); Next();
        }
        else if (Step == 11 && Elapsed - StepAt > 1)
        {
            Finish(true, TEXT("Real client: finite purchase/sale, refused junk and remote trade, consumed meal, annual reward/notice, moon and saved night")); return false;
        }
        return true;
    }

    if (Scenario == TEXT("lighting-restore"))
    {
        if (Elapsed < 1)
            return true;
        const auto Env = ratwjson::Child(ratwjson::Child(Snapshot, TEXT("cell")), TEXT("environment"));
        const bool Preserved = CurrentCell == TEXT("room_1") && ratwjson::String(Env, TEXT("phase")) == TEXT("night") &&
                               ratwjson::Number(Env, TEXT("illumination")) < .1 &&
                               ratwjson::Number(Env, TEXT("artificialLight")) == 0 &&
                               ratwjson::Number(Env, TEXT("daylightAccess")) == 0;
        Finish(Preserved, Preserved
                              ? TEXT("Unlit room, daylight access and night clock survived a separate process restart")
                              : TEXT("Restart changed the persisted indoor light profile"));
        return false;
    }
    if (Scenario == TEXT("lighting"))
    {
        const auto Cell = ratwjson::Child(Snapshot, TEXT("cell"));
        const auto Env = ratwjson::Child(Cell, TEXT("environment"));
        const double Light = ratwjson::Number(Env, TEXT("illumination"), -1);
        const double Glow = ratwjson::Number(Env, TEXT("glowStrength"), -1);
        const FString Phase = ratwjson::String(Env, TEXT("phase"));
        static int32 DayTiles = 0;
        int32 Visible = 0;
        for (const auto& Tile : ratwjson::Items(Cell, TEXT("tiles")))
            Visible += ratwjson::Bool(Tile->AsObject(), TEXT("visible")) ? 1 : 0;
        const auto Shot = [&](const TCHAR* Name) {
            if (FParse::Param(FCommandLine::Get(), TEXT("RatwCaptureLighting")))
                Capture(Name);
        };
        const auto Check = [&](bool Good, const TCHAR* Detail) {
            if (!Good)
                Finish(false, Detail);
            return Good;
        };
        if (Step == 0 && Elapsed > 1)
        {
            Send(TEXT("{\"type\":\"time\",\"value\":\"day\"}"));
            Next();
        }
        else if (Step == 1 && Elapsed - StepAt > 1.2)
        {
            if (!Check(CurrentCell == TEXT("room_1") && Light == 1 && Glow == 0 && Visible > 80,
                       TEXT("Authored daylit tavern did not load with clear visibility and no glow")))
                return false;
            DayTiles = Visible;
            Shot(TEXT("23-tavern-day.png"));
            Next();
        }
        else if (Step == 2 && Elapsed - StepAt > .7)
        {
            Send(TEXT("{\"type\":\"time\",\"value\":\"night\"}"));
            Next();
        }
        else if (Step == 3 && Elapsed - StepAt > 1.2)
        {
            if (!Check(Phase == TEXT("night") && Light == 1 && Glow > .9 && Visible == DayTiles &&
                           ratwjson::String(Env, TEXT("lightingTone")) == TEXT("warm"),
                       TEXT("Night tavern must glow warmly without losing daytime visibility")))
                return false;
            Shot(TEXT("24-tavern-warm-night.png"));
            Next();
        }
        else if (Step == 4 && Elapsed - StepAt > .7)
        {
            Send(TEXT("{\"type\":\"lighting\",\"value\":\"unlit\"}"));
            Next();
        }
        else if (Step == 5 && Elapsed - StepAt > 1.2)
        {
            if (!Check(Light < .1 && Glow == 0 && Visible < DayTiles / 4 &&
                           ratwjson::Number(Env, TEXT("hearing")) == 1 && ratwjson::Number(Env, TEXT("scent")) == 1,
                       TEXT("An unlit interior must restrict sight without disabling hearing or smell")))
                return false;
            Shot(TEXT("25-tavern-unlit-night.png"));
            Next();
        }
        else if (Step == 6 && Elapsed - StepAt > .7)
        {
            Send(TEXT("{\"type\":\"time\",\"value\":\"day\"}"));
            Next();
        }
        else if (Step == 7 && Elapsed - StepAt > 1.2)
        {
            if (!Check(Phase == TEXT("day") && Light < .1 && Glow == 0 && Visible < DayTiles / 4,
                       TEXT("A sealed unlit interior should remain dark at noon")))
                return false;
            Shot(TEXT("26-unlit-cellar-day.png"));
            Next();
        }
        else if (Step == 8 && Elapsed - StepAt > .7)
        {
            Send(TEXT("{\"type\":\"lighting\",\"value\":\"daylit\"}"));
            Next();
        }
        else if (Step == 9 && Elapsed - StepAt > 1.2)
        {
            if (!Check(Light == 1 && Glow == 0 && Visible == DayTiles &&
                           ratwjson::Number(Env, TEXT("artificialLight")) == 0,
                       TEXT("Daylight access should relight a windowed room without artificial light")))
                return false;
            Send(TEXT("{\"type\":\"time\",\"value\":\"night\"}"));
            Next();
        }
        else if (Step == 10 && Elapsed - StepAt > 1.2)
        {
            if (!Check(Light < .1 && Visible < DayTiles / 4, TEXT("A daylight-only room should darken after sunset")))
                return false;
            Send(TEXT("{\"type\":\"lighting\",\"value\":\"cool\"}"));
            Next();
        }
        else if (Step == 11 && Elapsed - StepAt > 1.2)
        {
            if (!Check(Light == 1 && Glow > .9 && ratwjson::String(Env, TEXT("lightingTone")) == TEXT("cool"),
                       TEXT("Cool artificial lighting should restore sight with a different atmosphere")))
                return false;
            Shot(TEXT("27-tavern-cool-night.png"));
            Next();
        }
        else if (Step == 12 && Elapsed - StepAt > .7)
        {
            Send(TEXT("{\"type\":\"path\",\"x\":10.5,\"y\":3.5}"));
            Next();
        }
        else if (Step == 13 && Near(10.5, 3.5))
        {
            Send(TEXT("{\"type\":\"action\",\"target\":\"link_tavern_b\",\"action\":\"open\"}"));
            Next();
        }
        else if (Step == 14 && CurrentCell == TEXT("cell_1") && Elapsed - StepAt > 1.2)
        {
            if (!Check(Phase == TEXT("night") && Light < .3 && Glow == 0,
                       TEXT("Leaving the lit interior should restore outdoor night conditions")))
                return false;
            Shot(TEXT("28-outdoor-night-edges.png"));
            Next();
        }
        else if (Step == 15 && Elapsed - StepAt > .7)
        {
            Send(TEXT("{\"type\":\"action\",\"target\":\"link_tavern_a\",\"action\":\"enter\"}"));
            Next();
        }
        else if (Step == 16 && CurrentCell == TEXT("room_1") && Elapsed - StepAt > .7)
        {
            Send(TEXT("{\"type\":\"lighting\",\"value\":\"unlit\"}"));
            Next();
        }
        else if (Step == 17 && Elapsed - StepAt > 1.5)
        {
            Finish(Light < .1, TEXT("Daylit/no-glow, warm night, unlit day/night, daylight-only and cool interiors; "
                                    "outdoor night transition verified"));
            return false;
        }
        return true;
    }
    if (Scenario == TEXT("weather-restore"))
    {
        if (Elapsed < 1)
            return true;
        const auto Cell = ratwjson::Child(Snapshot, TEXT("cell"));
        const auto Environment = ratwjson::Child(Cell, TEXT("environment"));
        const bool Restored = CurrentCell == TEXT("room_1") &&
                              ratwjson::String(Environment, TEXT("phase")) == TEXT("night") &&
                              ratwjson::String(Cell, TEXT("weather")) == TEXT("rain") &&
                              ratwjson::Number(Environment, TEXT("sight")) == 1;
        Finish(Restored,
               Restored
                   ? TEXT("Shared night clock and weather survived process restart; indoor shelter remains neutral")
                   : TEXT("Weather/clock/shelter did not survive process restart"));
        return false;
    }
    if (Scenario == TEXT("weather"))
    {
        const auto Cell = ratwjson::Child(Snapshot, TEXT("cell"));
        const auto Environment = ratwjson::Child(Cell, TEXT("environment"));
        const FString Weather = ratwjson::String(Cell, TEXT("weather"));
        const FString Phase = ratwjson::String(Environment, TEXT("phase"));
        static int32 DayVisible = 0;
        static double DayHearing = 0;
        int32 Visible = 0;
        for (const auto& Tile : ratwjson::Items(Cell, TEXT("tiles")))
            Visible += ratwjson::Bool(Tile->AsObject(), TEXT("visible")) ? 1 : 0;
        const auto Shot = [&](const TCHAR* Name) {
            if (FParse::Param(FCommandLine::Get(), TEXT("RatwCaptureWeather")))
                Capture(Name);
        };
        const auto Check = [&](bool Pass, const TCHAR* Detail) {
            if (!Pass)
                Finish(false, Detail);
            return Pass;
        };
        if (Step == 0 && Elapsed > 1)
        {
            Send(TEXT("{\"type\":\"time\",\"value\":\"day\"}"));
            Send(TEXT("{\"type\":\"weather\",\"value\":\"clear\"}"));
            Send(TEXT("{\"type\":\"wind\",\"value\":\"east\"}"));
            Next();
        }
        else if (Step == 1 && Elapsed - StepAt > 1.5)
        {
            if (!Check(Weather == TEXT("clear") && Phase == TEXT("day") &&
                           ratwjson::Number(Environment, TEXT("sight")) == 1 && Visible > 100,
                       TEXT("Clear-day environment did not reach client")))
                return false;
            DayVisible = Visible;
            DayHearing = ratwjson::Number(Environment, TEXT("hearing"));
            Shot(TEXT("17-weather-day.png"));
            Next();
        }
        else if (Step == 2 && Elapsed - StepAt > .8)
        {
            Send(TEXT("{\"type\":\"weather\",\"value\":\"rain\"}"));
            Next();
        }
        else if (Step == 3 && Elapsed - StepAt > 1.5)
        {
            if (!Check(Weather == TEXT("rain") && ratwjson::Number(Environment, TEXT("sight")) < 1 &&
                           ratwjson::Number(Environment, TEXT("hearing")) < DayHearing &&
                           ratwjson::Number(Environment, TEXT("scent")) < 1 &&
                           ratwjson::Number(Environment, TEXT("movement")) < 1,
                       TEXT("Rain did not modify all four environment channels")))
                return false;
            Shot(TEXT("18-weather-rain.png"));
            Next();
        }
        else if (Step == 4 && Elapsed - StepAt > .8)
        {
            Send(TEXT("{\"type\":\"weather\",\"value\":\"snow\"}"));
            Next();
        }
        else if (Step == 5 && Elapsed - StepAt > 1.5)
        {
            if (!Check(Weather == TEXT("snow") && ratwjson::Number(Environment, TEXT("sight")) < 1 &&
                           ratwjson::Number(Environment, TEXT("hearing")) < DayHearing &&
                           ratwjson::Number(Environment, TEXT("scent")) < 1 &&
                           ratwjson::Number(Environment, TEXT("movement")) < .85,
                       TEXT("Snow did not modify senses and movement")))
                return false;
            Shot(TEXT("19-weather-snow.png"));
            Next();
        }
        else if (Step == 6 && Elapsed - StepAt > .8)
        {
            Send(TEXT("{\"type\":\"weather\",\"value\":\"fog\"}"));
            Next();
        }
        else if (Step == 7 && Elapsed - StepAt > 1.5)
        {
            if (!Check(Weather == TEXT("fog") && Visible < DayVisible * .7 &&
                           ratwjson::Number(Environment, TEXT("sight")) <= .4,
                       TEXT("Fog did not reduce authoritative visible terrain")))
                return false;
            Shot(TEXT("20-weather-fog.png"));
            Next();
        }
        else if (Step == 8 && Elapsed - StepAt > .8)
        {
            Send(TEXT("{\"type\":\"weather\",\"value\":\"clear\"}"));
            Send(TEXT("{\"type\":\"time\",\"value\":\"night\"}"));
            Next();
        }
        else if (Step == 9 && Elapsed - StepAt > 1.5)
        {
            if (!Check(Phase == TEXT("night") && Visible < DayVisible * .5 &&
                           ratwjson::Number(Environment, TEXT("illumination")) < .4 &&
                           FMath::Abs(ratwjson::Number(Environment, TEXT("hearing")) - DayHearing) < .001 &&
                           ratwjson::Number(Environment, TEXT("scent")) == 1,
                       TEXT("Night must hide distant terrain without silencing hearing or scent")))
                return false;
            Shot(TEXT("21-weather-night.png"));
            Next();
        }
        else if (Step == 10 && Elapsed - StepAt > .8)
        {
            Send(TEXT("{\"type\":\"action\",\"target\":\"link_shelter_a\",\"action\":\"open\"}"));
            Next();
        }
        else if (Step == 11 && CurrentCell == TEXT("room_1") && Elapsed - StepAt > .8)
        {
            Send(TEXT("{\"type\":\"weather\",\"value\":\"rain\"}"));
            Next();
        }
        else if (Step == 12 && Elapsed - StepAt > 1.5)
        {
            bool Neutral = !ratwjson::Bool(Cell, TEXT("outdoors")) && Phase == TEXT("night") && Weather == TEXT("rain");
            for (const TCHAR* Key :
                 {TEXT("illumination"), TEXT("sight"), TEXT("hearing"), TEXT("scent"), TEXT("movement")})
                Neutral &= ratwjson::Number(Environment, Key) == 1;
            if (!Check(Neutral, TEXT("Indoor shelter inherited outdoor weather/night penalties")))
                return false;
            Shot(TEXT("22-weather-shelter.png"));
            Next();
        }
        else if (Step == 13 && Elapsed - StepAt > 1.5)
        {
            Finish(true, TEXT("Day/rain/snow/fog/night changed authoritative conditions and visible terrain; interior "
                              "stayed sheltered"));
            return false;
        }
        return true;
    }
    if (Scenario == TEXT("travel"))
    {
        const auto Travel = ratwjson::Child(Snapshot, TEXT("travel"));
        const double Stamina = ratwjson::Number(*Self, TEXT("stamina"), -1);
        const double Speed = ratwjson::Number(*Self, TEXT("currentSpeed"));
        static bool SawMiddle = false;
        static double RestStamina = 0, StopX = 0, StopY = 0;
        static FString StopCell;
        SawMiddle |= CurrentCell == TEXT("cell_2");
        if (Step == 0 && Elapsed > 1)
        {
            Send(TEXT("{\"type\":\"pace\",\"pace\":10}"));
            Send(TEXT("{\"type\":\"travel\",\"target\":\"cell_3\"}"));
            Next();
        }
        else if (Step == 1 && Elapsed - StepAt > 2.2)
        {
            if (Stamina < 0 || Stamina >= 95 || Speed <= 2.6 || !ratwjson::Bool(Travel, TEXT("active")))
            {
                Finish(false,
                       FString::Printf(TEXT("Sprint route did not move and drain stamina: stamina=%.2f speed=%.2f"),
                                       Stamina, Speed));
                return false;
            }
            if (FParse::Param(FCommandLine::Get(), TEXT("RatwCaptureTravel")))
                Capture(TEXT("15-travel-pace.png"));
            Send(TEXT("{\"type\":\"typing\",\"active\":true}"));
            Send(TEXT("{\"type\":\"chat\",\"text\":\"The trail feels familiar.\",\"channel\":\"ic\"}"));
            Next();
        }
        else if (Step == 2 && !ratwjson::Bool(Travel, TEXT("active")) && CurrentCell != TEXT("cell_3"))
        {
            Finish(false, TEXT("Typing or speaking unexpectedly cancelled the deliberate world journey"));
            return false;
        }
        else if (Step == 2 && CurrentCell == TEXT("cell_3") && !ratwjson::Bool(Travel, TEXT("active")))
        {
            if (!SawMiddle || ratwjson::Items(Snapshot, TEXT("travelMap")).Num() != 4)
            {
                Finish(false, TEXT("Journey skipped an intermediate cell or lost the visited travel index"));
                return false;
            }
            RestStamina = Stamina;
            if (Widget.IsValid())
                Widget->SetPresentationPage(TEXT("travel"));
            Next();
        }
        else if (Step == 3 && Elapsed - StepAt > 1)
        {
            if (Stamina <= RestStamina || Speed > .001)
            {
                Finish(false, TEXT("Completed travel did not stop and recover stamina"));
                return false;
            }
            if (FParse::Param(FCommandLine::Get(), TEXT("RatwCaptureTravel")))
                Capture(TEXT("16-known-routes.png"));
            Next();
        }
        else if (Step == 4 && Elapsed - StepAt > 1)
        {
            if (Widget.IsValid())
                Widget->SetPresentationPage(TEXT("local"));
            Send(TEXT("{\"type\":\"travel\",\"target\":\"cell_1\"}"));
            Next();
        }
        else if (Step == 5 && Elapsed - StepAt > 1)
        {
            Send(TEXT("{\"type\":\"stop\"}"));
            Next();
        }
        else if (Step == 6 && Elapsed - StepAt > .6)
        {
            StopX = FinalX;
            StopY = Y;
            StopCell = CurrentCell;
            if (ratwjson::Bool(Travel, TEXT("active")))
            {
                Finish(false, TEXT("Manual stop failed to cancel the travel intention"));
                return false;
            }
            Next();
        }
        else if (Step == 7 && Elapsed - StepAt > 1)
        {
            if (!Near(StopX, StopY) || CurrentCell != StopCell)
            {
                Finish(false, TEXT("Cancelled route resumed without another command"));
                return false;
            }
            Send(TEXT("{\"type\":\"pace\",\"pace\":5}"));
            Send(TEXT("{\"type\":\"travel\",\"target\":\"room_1\"}"));
            Next();
        }
        else if (Step == 8 && ratwjson::Bool(Travel, TEXT("paused")))
        {
            if (CurrentCell != TEXT("cell_2") || ratwjson::String(Travel, TEXT("nextDoor")) != TEXT("link_shelter_a"))
            {
                Finish(false, TEXT("Remembered shelter route failed to approach its closed doorway"));
                return false;
            }
            Next();
        }
        else if (Step == 9 && Elapsed - StepAt > 1)
        {
            if (CurrentCell != TEXT("cell_2") || !ratwjson::Bool(Travel, TEXT("paused")))
            {
                Finish(false, TEXT("Travel opened a closed door without an explicit action"));
                return false;
            }
            Send(TEXT("{\"type\":\"action\",\"target\":\"link_shelter_a\",\"action\":\"open\"}"));
            Next();
        }
        else if (Step == 10 && CurrentCell == TEXT("room_1") && !ratwjson::Bool(Travel, TEXT("active")))
        {
            Finish(true, TEXT("Sprint consumed stamina; known travel crossed three local cells and recovered at rest; "
                              "manual stop cancelled; closed door waited for explicit Open"));
            return false;
        }
    }
    else if (Scenario == TEXT("atlas"))
    {
        static FString StartingCell;
        if (Step == 0 && Elapsed > 1)
        {
            StartingCell = CurrentCell;
            Send(TEXT("{\"type\":\"path\",\"x\":33.5,\"y\":18.5}"));
            Next();
        }
        else if (Step == 1 && CurrentCell != StartingCell)
        {
            const auto* Cell = Snapshot->Values.Find(TEXT("cell"));
            const auto Local = Cell ? (*Cell)->AsObject() : nullptr;
            double Width = 0, Height = 0;
            if (Local)
            {
                Local->TryGetNumberField(TEXT("width"), Width);
                Local->TryGetNumberField(TEXT("height"), Height);
            }
            if (!Near(.5, 18.5) || Width != 32 || Height != 24)
            {
                Finish(false, TEXT("Authored seam did not preserve its connection point or separate-cell dimensions"));
                return false;
            }
            Next();
        }
        else if (Step == 2 && Elapsed - StepAt > 1)
        {
            if (!Near(.5, 18.5))
            {
                Finish(false, TEXT("Movement did not stop after crossing the authored seam"));
                return false;
            }
            if (FParse::Param(FCommandLine::Get(), TEXT("RatwCaptureAtlas")))
                Capture(TEXT("13-authored-cell-runtime.png"));
            Next();
        }
        else if (Step == 3 && Elapsed - StepAt > 1)
        {
            Send(TEXT("{\"type\":\"path\",\"x\":10.5,\"y\":11.5}"));
            Next();
        }
        else if (Step == 4 && Near(10.5, 11.5))
        {
            if (CurrentCell == TEXT("room_1"))
            {
                Finish(false, TEXT("Closed authored door auto-opened"));
                return false;
            }
            Send(TEXT("{\"type\":\"action\",\"target\":\"link_inn_entry_a\",\"action\":\"open\"}"));
            Next();
        }
        else if (Step == 5 && CurrentCell == TEXT("room_1"))
        {
            if (!Near(7.5, 9.5))
            {
                Finish(false, TEXT("Off-map interior arrival differs from the authored anchor"));
                return false;
            }
            if (FParse::Param(FCommandLine::Get(), TEXT("RatwCaptureAtlas")))
                Capture(TEXT("14-authored-interior-runtime.png"));
            Next();
        }
        else if (Step == 6 && Elapsed - StepAt > 1)
        {
            Finish(Near(7.5, 9.5), TEXT("Authored atlas imported; boundary crossed and stopped; explicit door entered "
                                        "a separate off-map interior"));
            return false;
        }
    }
    else if (Scenario == TEXT("scent-source"))
    {
        // Hold the independently connected upwind test player in place until
        // the orchestrator finishes the observer and tears down its processes.
        if (Elapsed > 100)
        {
            Finish(true, TEXT("Held the upwind source fixture"));
            return false;
        }
    }
    else if (Scenario == TEXT("scent"))
    {
        const TSharedPtr<FJsonObject>* Senses = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Cues = nullptr;
        bool WestCue = false;
        if (Snapshot->TryGetObjectField(TEXT("senses"), Senses) && (*Senses)->TryGetArrayField(TEXT("scentCues"), Cues))
            for (const auto& Value : *Cues)
                if (const auto Cue = Value->AsObject())
                {
                    double Sector = -1;
                    Cue->TryGetNumberField(TEXT("sector"), Sector);
                    if (Cue->Values.Num() != 3 || Sector < 0 || Sector > 7 || Cue->HasField(TEXT("id")) ||
                        Cue->HasField(TEXT("x")) || Cue->HasField(TEXT("name")))
                    {
                        Finish(false, TEXT("Scent cue leaked more than the coarse anonymous contract"));
                        return false;
                    }
                    WestCue |= Sector == 4;
                }
        if (Snapshot->TryGetArrayField(TEXT("entities"), Entities))
            for (const auto& Value : *Entities)
                if (const auto Entity = Value->AsObject())
                {
                    FString Id;
                    Entity->TryGetStringField(TEXT("id"), Id);
                    if (Id == TEXT("player-bracken"))
                    {
                        Finish(false, TEXT("Unseen scent source was included as an exact map entity"));
                        return false;
                    }
                }
        if (Step == 0 && Elapsed > 2 && WestCue)
        {
            Send(TEXT("{\"type\":\"action\",\"action\":\"smell\"}"));
            Send(TEXT("{\"type\":\"action\",\"action\":\"inspect\",\"target\":\"player-bracken\"}"));
            Send(TEXT("{\"type\":\"action\",\"action\":\"inspect\",\"target\":\"player-does-not-exist\"}"));
            Next();
        }
        else if (Step == 1 && Elapsed - StepAt > 2)
        {
            bool Smelled = false;
            int32 InspectDenials = 0;
            for (const auto& Raw : Controller->GetReceivedEvents())
                if (const auto Event = Parse(Raw))
                {
                    FString Text, Type;
                    Event->TryGetStringField(TEXT("type"), Type);
                    Event->TryGetStringField(TEXT("text"), Text);
                    Smelled |= Text.Contains(TEXT("roughly west"));
                    if (Text == TEXT("You cannot inspect someone you cannot see."))
                        ++InspectDenials;
                    if (Type == TEXT("inspect"))
                    {
                        Finish(false, TEXT("Scent incorrectly permitted inspection"));
                        return false;
                    }
                }
            if (!Smelled || InspectDenials != 2)
            {
                Finish(false, TEXT("Live smell narration or hidden-inspection rejection missing"));
                return false;
            }
            if (FParse::Param(FCommandLine::Get(), TEXT("RatwCaptureScent")))
                Capture(TEXT("12-upwind-scent.png"));
            Next();
        }
        else if (Step == 2 && Elapsed - StepAt > 1)
        {
            Send(TEXT("{\"type\":\"wind\",\"value\":\"west\"}"));
            Next();
        }
        else if (Step == 3 && Elapsed - StepAt > 2)
        {
            if (WestCue)
            {
                Finish(false, TEXT("Reversed wind failed to remove the distant westward scent cue"));
                return false;
            }
            Next();
        }
        else if (Step == 4 && Elapsed - StepAt > 1)
        {
            Send(TEXT("{\"type\":\"wind\",\"value\":\"east\"}"));
            Next();
        }
        else if (Step == 5 && Elapsed - StepAt > 2)
        {
            Finish(WestCue, TEXT("Two-client upwind scent, anonymous sectors, hidden entity/inspection protection, "
                                 "and wind reversal verified"));
            return false;
        }
    }
    else if (Scenario == TEXT("movement"))
    {
        FString Posture;
        double Facing = 0;
        (*Self)->TryGetStringField(TEXT("posture"), Posture);
        (*Self)->TryGetNumberField(TEXT("facing"), Facing);
        if (Step == 0 && Elapsed > 1)
        {
            Send(TEXT("{\"type\":\"face\",\"x\":0,\"y\":12.5}"));
            Next();
        }
        else if (Step == 1)
        {
            SawPartialTurn |= FMath::Abs(Facing) > 0.1 && FMath::Abs(Facing) < 2.9;
            if (Elapsed - StepAt > 1.5)
            {
                if (!SawPartialTurn || FMath::Abs(FMath::Abs(Facing) - PI) > 0.05)
                {
                    Finish(false, TEXT("Stationary facing did not replicate intermediate angles and final target"));
                    return false;
                }
                Send(TEXT("{\"type\":\"action\",\"action\":\"sit\"}"));
                Next();
            }
        }
        else if (Step == 2 && Posture == TEXT("sitting"))
        {
            MovementStartX = FinalX;
            Send(TEXT("{\"type\":\"move\",\"x\":1,\"y\":0}"));
            Next();
        }
        else if (Step == 3)
        {
            Send(TEXT("{\"type\":\"move\",\"x\":1,\"y\":0}"));
            if (Elapsed - StepAt > 0.2 && Elapsed - StepAt < 0.5)
                SawSitPause |= Posture == TEXT("rising") && FMath::Abs(FinalX - MovementStartX) < 0.01;
            if (Elapsed - StepAt > 1.4)
            {
                if (!SawSitPause || FinalX - MovementStartX < 1.0 || Posture != TEXT("standing"))
                {
                    Finish(false, TEXT("Repeated movement did not preserve sit-rise pause and resume walking"));
                    return false;
                }
                Send(TEXT("{\"type\":\"stop\"}"));
                Send(TEXT("{\"type\":\"chat\",\"channel\":\"ic\",\"text\":\"/lay\"}"));
                Next();
            }
        }
        else if (Step == 4 && Posture == TEXT("lying"))
        {
            MovementStartX = FinalX;
            Send(TEXT("{\"type\":\"move\",\"x\":-1,\"y\":0}"));
            Next();
        }
        else if (Step == 5)
        {
            SawCrouch |= Posture == TEXT("crouching");
            if (Elapsed - StepAt > 1.65)
            {
                const double Travel = MovementStartX - FinalX;
                if (!SawCrouch || Travel < 0.55 || Travel > 1.4)
                {
                    Finish(false, TEXT("Lying movement did not become a slow crouch"));
                    return false;
                }
                Send(TEXT("{\"type\":\"stop\"}"));
                Next();
            }
        }
        else if (Step == 6 && Elapsed - StepAt > 0.7)
        {
            if (FParse::Param(FCommandLine::Get(), TEXT("RatwCaptureMovement")))
                Capture(TEXT("10-crouch-movement.png"));
            Next();
        }
        else if (Step == 7 && Elapsed - StepAt > 1)
        {
            if (FParse::Param(FCommandLine::Get(), TEXT("RatwCaptureMovement")) && Widget)
            {
                const auto Geometry = Widget->GetCachedGeometry();
                const auto Size = Geometry.GetLocalSize();
                const double Scale = FMath::Min(Size.X / 1600.0, Size.Y / 1000.0);
                const auto Offset = (Size - FVector2D(1600, 1000) * Scale) * 0.5;
                const auto Point = Geometry.LocalToAbsolute(Offset + FVector2D(1250, 410) * Scale);
                const TSet<FKey> NoButtons;
                Widget->OnFocusReceived(Geometry, FFocusEvent(EFocusCause::SetDirectly, 0));
                Widget->OnMouseMove(Geometry, FPointerEvent(0, Point, Point, NoButtons, EKeys::Invalid, 0,
                                                            FModifierKeysState(false, false, false, false, true, false,
                                                                               false, false, false)));
                Capture(TEXT("11-alt-facing-preview.png"));
            }
            Next();
        }
        else if (Step == 8 && Elapsed - StepAt > 1)
        {
            if (Widget)
                Widget->OnKeyUp(Widget->GetCachedGeometry(),
                                FKeyEvent(EKeys::LeftAlt, FModifierKeysState(), 0, false, 0, 0));
            Send(TEXT("{\"type\":\"chat\",\"channel\":\"ic\",\"text\":\"/stand\"}"));
            Next();
        }
        else if (Step == 9 && Elapsed - StepAt > 1)
        {
            Finish(Posture == TEXT("standing"), TEXT("Gradual replicated facing, sit-rise delay under refreshed input, "
                                                     "lying-to-sneak speed, and slash stand verified"));
            return false;
        }
    }
    else if (Scenario == TEXT("gallery"))
    {
        if (Step == 0 && Elapsed > 1)
        {
            Send(TEXT("{\"type\":\"chat\",\"channel\":\"ic\",\"volume\":\"speak\",\"text\":\"\\\"The road has been "
                      "quiet since the rain.\\\" /sigh \\\"I could use a warm place by the hearth.\\\"\"}"));
            ++Step;
        }
        else if (Step == 1 && Elapsed > 5)
        {
            Capture(TEXT("01-tavern-local.png"));
            ++Step;
        }
        else if (Step == 2 && Elapsed > 7)
        {
            if (Widget)
                Widget->SetPresentationPage(TEXT("character"));
            ++Step;
        }
        else if (Step == 3 && Elapsed > 9)
        {
            Capture(TEXT("02-character-sheet.png"));
            ++Step;
        }
        else if (Step == 4 && Elapsed > 11)
        {
            if (Widget)
                Widget->SetPresentationPage(TEXT("inventory"));
            ++Step;
        }
        else if (Step == 5 && Elapsed > 13)
        {
            Capture(TEXT("03-inventory.png"));
            ++Step;
        }
        else if (Step == 6 && Elapsed > 15)
        {
            if (Widget)
                Widget->SetPresentationPage(TEXT("world"));
            ++Step;
        }
        else if (Step == 7 && Elapsed > 17)
        {
            Capture(TEXT("04-world-map.png"));
            ++Step;
        }
        else if (Step == 8 && Elapsed > 19)
        {
            if (Widget)
                Widget->SetPresentationPage(TEXT("settings"));
            ++Step;
        }
        else if (Step == 9 && Elapsed > 21)
        {
            Capture(TEXT("05-settings.png"));
            ++Step;
        }
        else if (Step == 10 && Elapsed > 23)
        {
            if (Widget)
                Widget->SetPresentationPage(TEXT("text-first"));
            ++Step;
        }
        else if (Step == 11 && Elapsed > 25)
        {
            Capture(TEXT("09-text-first-layout.png"));
            ++Step;
        }
        else if (Step == 12 && Elapsed > 27)
        {
            Finish(true, TEXT("Gallery captured from running Unreal game"));
            return false;
        }
    }
    else if (Scenario == TEXT("network"))
    {
        if (Step == 0 && Elapsed > 2)
        {
            Send(TEXT("{\"type\":\"move\",\"x\":1,\"y\":0}"));
            ++Step;
        }
        else if (Step == 1 && Elapsed > 2.5)
        {
            Send(TEXT("{\"type\":\"move\",\"x\":0,\"y\":0}"));
            ++Step;
        }
        else if (Step == 2 && Elapsed > 4)
        {
            Send(TEXT("{\"type\":\"chat\",\"channel\":\"ic\",\"volume\":\"speak\",\"text\":\"\\\"I followed the river "
                      "all the way here, and now the hearth feels like home.\\\" /sigh\"}"));
            ++Step;
        }
        else if (Step == 3 && Elapsed > 6)
        {
            Send(TEXT("{\"type\":\"chat\",\"channel\":\"ooc\",\"volume\":\"speak\",\"text\":\"Network test: both "
                      "clients connected.\"}"));
            ++Step;
        }
        else if (Step == 4 && Elapsed > 8)
        {
            Send(TEXT("{\"type\":\"face\",\"x\":1,\"y\":1}"));
            ++Step;
        }
        else if (Step == 5 && Elapsed > 9)
        {
            auto Long = MakeShared<FJsonObject>();
            Long->SetStringField(TEXT("type"), TEXT("chat"));
            Long->SetStringField(TEXT("channel"), TEXT("ic"));
            FString Prose = TEXT("Long-form network proof: ");
            while (Prose.Len() < 16000)
                Prose += TEXT("Rain writes a patient rhythm along the rafters while the wolves share their stories. ");
            Long->SetStringField(TEXT("text"), Prose.Left(16000));
            FString Command;
            FJsonSerializer::Serialize(Long, TJsonWriterFactory<>::Create(&Command));
            Controller->SubmitCommand(Command);
            ++Step;
        }
        else if (Step == 6 && Elapsed > 11)
        {
            Capture(TEXT("network-") + Role + TEXT(".png"));
            ++Step;
        }
        else if (Step == 7 && Elapsed > 15)
        {
            bool bSawOther = false, bSawOOC = false, bLong = false;
            for (const auto& EventText : Controller->GetReceivedEvents())
                if (auto Event = Parse(EventText))
                {
                    FString Speaker, Channel;
                    Event->TryGetStringField(TEXT("speaker"), Speaker);
                    Event->TryGetStringField(TEXT("channel"), Channel);
                    FString OwnName;
                    (*Self)->TryGetStringField(TEXT("name"), OwnName);
                    const FString Expected = Role == TEXT("ash") ? TEXT("Bracken") : TEXT("Ash");
                    bSawOther |= Speaker == Expected && Channel == TEXT("ic");
                    bSawOOC |= Speaker == Expected && Channel == TEXT("ooc");
                    FString Text;
                    Event->TryGetStringField(TEXT("text"), Text);
                    bLong |= Speaker == Expected && Text.Len() >= 16000 && Channel == TEXT("ic");
                }
            const bool bMoved = FMath::Abs(FinalX - InitialX) > 0.1;
            const bool bMotionRate = Controller->GetMotionFrameCount() > 100 &&
                Controller->GetMotionFrameCount() > Controller->GetSnapshotCount() * 3;
            const bool bPass = bMoved && bSawOther && bSawOOC && bLong && bMotionRate;
            Finish(bPass,
                   FString::Printf(TEXT("moved=%d otherIC=%d OOC=%d longPost=%d motion20Hz=%d"), bMoved, bSawOther, bSawOOC, bLong, bMotionRate));
            return false;
        }
    }
    else if (Scenario == TEXT("walkthrough"))
    {
        if (Step == 0 && Elapsed > 1)
        {
            Send(TEXT("{\"type\":\"path\",\"x\":22.5,\"y\":6.5}"));
            Next();
        }
        else if (Step == 1 && Near(22.5, 6.5))
        {
            Send(TEXT("{\"type\":\"action\",\"target\":\"door_pantry\",\"action\":\"open\"}"));
            Next();
        }
        else if (Step == 2 && Elapsed - StepAt > 0.5)
        {
            Send(TEXT("{\"type\":\"path\",\"x\":27.5,\"y\":5.5}"));
            Next();
        }
        else if (Step == 3 && Near(27.5, 5.5))
        {
            if (Widget)
                Widget->SetPresentationPage(TEXT("world"));
            Next();
        }
        else if (Step == 4 && Elapsed - StepAt > 2)
        {
            Capture(TEXT("06-visible-vertical-world.png"));
            Next();
        }
        else if (Step == 5 && Elapsed - StepAt > 1)
        {
            Send(TEXT("{\"type\":\"action\",\"target\":\"stairs_up\",\"action\":\"enter\"}"));
            if (Widget)
                Widget->SetPresentationPage(TEXT("local"));
            Next();
        }
        else if (Step == 6 && CurrentCell == TEXT("loft") && Elapsed - StepAt > 2)
        {
            Capture(TEXT("07-quiet-loft.png"));
            Next();
        }
        else if (Step == 7 && Elapsed - StepAt > 1)
        {
            Send(TEXT("{\"type\":\"action\",\"target\":\"stairs_down\",\"action\":\"enter\"}"));
            Next();
        }
        else if (Step == 8 && CurrentCell == TEXT("tavern"))
        {
            Send(TEXT("{\"type\":\"path\",\"x\":16.5,\"y\":22.5}"));
            Next();
        }
        else if (Step == 9 && Near(16.5, 22.5))
        {
            Send(TEXT("{\"type\":\"action\",\"target\":\"door_main\",\"action\":\"open\"}"));
            Next();
        }
        else if (Step == 10 && CurrentCell == TEXT("exterior"))
        {
            Send(TEXT("{\"type\":\"path\",\"x\":16.5,\"y\":9.5}"));
            Next();
        }
        else if (Step == 11 && Near(16.5, 9.5) && Elapsed - StepAt > 2)
        {
            Capture(TEXT("08-rain-in-juniper-yard.png"));
            Next();
        }
        else if (Step == 12 && Elapsed - StepAt > 2)
        {
            Finish(true, TEXT("Pathing, explicit same-cell door, visible vertical map, loft round-trip, yard "
                              "transition and rain captured"));
            return false;
        }
    }
    else if (Scenario == TEXT("dialogue-live") || Scenario == TEXT("dialogue-recall"))
    {
        // These scenarios send exactly one ordinary IC post. They never call
        // Talk directly, inject an NPC reply, or bypass the perception pipeline.
        if (Step == 0 && Elapsed > 1)
        {
            if (CurrentCell != TEXT("tavern"))
            {
                Finish(false, TEXT("Live dialogue fixture must start in the tavern"));
                return false;
            }
            const TArray<TSharedPtr<FJsonValue>>* Inventory = nullptr;
            if (!Snapshot->TryGetArrayField(TEXT("inventory"), Inventory) ||
                !(*Self)->TryGetNumberField(TEXT("socialXp"), DialogueXpBefore) ||
                !(*Self)->TryGetNumberField(TEXT("socialLevel"), DialogueLevelBefore))
            {
                Finish(false, TEXT("Live dialogue fixture lacks authoritative inventory/XP fields"));
                return false;
            }
            DialogueInventoryBefore = MakeShared<FJsonValueArray>(*Inventory);
            const TSharedPtr<FJsonObject>* Memory = nullptr;
            if (Snapshot->TryGetObjectField(TEXT("memory"), Memory))
                (*Memory)->TryGetNumberField(TEXT("activeTurns"), DialogueTurnsBefore);
            DialogueEvidence = MakeShared<FJsonObject>();
            DialogueEvidence->SetArrayField(TEXT("inventoryBefore"), *Inventory);
            DialogueEvidence->SetNumberField(TEXT("socialXpBefore"), DialogueXpBefore);
            DialogueEvidence->SetNumberField(TEXT("socialLevelBefore"), DialogueLevelBefore);
            DialogueEvidence->SetNumberField(TEXT("activeTurnsBefore"), DialogueTurnsBefore);
            FString Label;
            Snapshot->TryGetStringField(TEXT("dialogueProvider"), Label);
            DialogueEvidence->SetStringField(TEXT("providerLabel"), Label);
            Send(TEXT("{\"type\":\"path\",\"x\":11.5,\"y\":6.5}"));
            Next();
        }
        else if (Step == 1 && Near(11.5, 6.5))
        {
            // The authored hearth position has clear floor between the player
            // and Rowan. Verify the real replicated NPC remains within three
            // tiles, far inside ordinary clear-hearing distance, before posting.
            bool bRowanNearby = false;
            if (Snapshot->TryGetArrayField(TEXT("entities"), Entities))
                for (const auto& Value : *Entities)
                    if (auto Entity = Value->AsObject())
                    {
                        FString Id;
                        double X = 0, EntityY = 0;
                        Entity->TryGetStringField(TEXT("id"), Id);
                        Entity->TryGetNumberField(TEXT("x"), X);
                        Entity->TryGetNumberField(TEXT("y"), EntityY);
                        if (Id == TEXT("npc_keeper") && FMath::Square(X - FinalX) + FMath::Square(EntityY - Y) <= 9.0)
                            bRowanNearby = true;
                    }
            if (!bRowanNearby)
            {
                if (Elapsed - StepAt > 20)
                {
                    Finish(false, TEXT("Rowan was not at the clear-hearing hearth fixture"));
                    return false;
                }
                return true;
            }
            for (const auto& EventText : Controller->GetReceivedEvents())
                if (auto Event = Parse(EventText))
                {
                    double Sequence = 0;
                    if (Event->TryGetNumberField(TEXT("sequence"), Sequence))
                        DialogueBaselineSequence = FMath::Max(DialogueBaselineSequence, Sequence);
                }
            const FString Prompt =
                Scenario == TEXT("dialogue-live")
                    ? TEXT("Rowan, remember that my sister is called Willow and I promised to bring her blue river "
                           "stones.")
                    : TEXT("Rowan, what is my sister called, and what did I promise to bring her?");
            auto Command = MakeShared<FJsonObject>();
            Command->SetStringField(TEXT("type"), TEXT("chat"));
            Command->SetStringField(TEXT("channel"), TEXT("ic"));
            Command->SetStringField(TEXT("volume"), TEXT("speak"));
            Command->SetStringField(TEXT("requestId"), Scenario);
            Command->SetStringField(TEXT("text"), Prompt);
            FString CommandJson;
            FJsonSerializer::Serialize(Command, TJsonWriterFactory<>::Create(&CommandJson));
            DialogueEvidence->SetStringField(TEXT("prompt"), Prompt);
            DialogueEvidence->SetNumberField(TEXT("postsSent"), 1);
            DialogueEvidence->SetNumberField(TEXT("baselineSequence"), DialogueBaselineSequence);
            Send(TEXT("{\"type\":\"move\",\"x\":0,\"y\":0}"));
            Controller->SubmitCommand(CommandJson);
            Next();
        }
        else if (Step == 2)
        {
            for (const auto& EventText : Controller->GetReceivedEvents())
                if (auto Event = Parse(EventText))
                {
                    FString Type, Speaker, Channel, Text, RequestId;
                    double Sequence = 0;
                    Event->TryGetStringField(TEXT("type"), Type);
                    Event->TryGetStringField(TEXT("speaker"), Speaker);
                    Event->TryGetStringField(TEXT("channel"), Channel);
                    Event->TryGetStringField(TEXT("text"), Text);
                    Event->TryGetStringField(TEXT("requestId"), RequestId);
                    Event->TryGetNumberField(TEXT("sequence"), Sequence);
                    if (Type == TEXT("error") && RequestId == Scenario)
                    {
                        Finish(false, TEXT("Live dialogue post rejected by the server"));
                        return false;
                    }
                    if (Type == TEXT("chatAccepted") && RequestId == Scenario)
                        DialogueEvidence->SetBoolField(TEXT("postAccepted"), true);
                    if (Step == 2 && Type == TEXT("roleplay") && Speaker == TEXT("Rowan") && Channel == TEXT("ic") &&
                        Sequence > DialogueBaselineSequence && !Text.IsEmpty())
                    {
                        // The external bridge's audit can hash this exact text
                        // to distinguish generation from silent authored fallback.
                        DialogueEvidence->SetStringField(TEXT("replyText"), Text);
                        DialogueEvidence->SetNumberField(TEXT("replySequence"), Sequence);
                        DialogueEvidence->SetNumberField(TEXT("replySeconds"), Elapsed - StepAt);
                        Next();
                    }
                }
            if (Step == 2 && Elapsed - StepAt > 15)
            {
                Finish(false, TEXT("No Rowan speech event arrived within fifteen seconds"));
                return false;
            }
        }
        else if (Step == 3 && Elapsed - StepAt > 0.5)
        {
            const TArray<TSharedPtr<FJsonValue>>* Inventory = nullptr;
            double Xp = -1, Level = -1, Turns = 0;
            (*Self)->TryGetNumberField(TEXT("socialXp"), Xp);
            (*Self)->TryGetNumberField(TEXT("socialLevel"), Level);
            const TSharedPtr<FJsonObject>* Memory = nullptr;
            if (Snapshot->TryGetObjectField(TEXT("memory"), Memory))
                (*Memory)->TryGetNumberField(TEXT("activeTurns"), Turns);
            const bool bInventoryUnchanged =
                Snapshot->TryGetArrayField(TEXT("inventory"), Inventory) &&
                FJsonValue::CompareEqual(*DialogueInventoryBefore, FJsonValueArray(*Inventory));
            const bool bXpUnchanged = Xp == DialogueXpBefore && Level == DialogueLevelBefore;
            const bool bMemoryAdvanced = Turns >= DialogueTurnsBefore + 2;
            bool bAccepted = false;
            DialogueEvidence->TryGetBoolField(TEXT("postAccepted"), bAccepted);
            DialogueEvidence->SetBoolField(TEXT("inventoryUnchanged"), bInventoryUnchanged);
            DialogueEvidence->SetBoolField(TEXT("xpUnchanged"), bXpUnchanged);
            DialogueEvidence->SetNumberField(TEXT("socialXpAfter"), Xp);
            DialogueEvidence->SetNumberField(TEXT("socialLevelAfter"), Level);
            DialogueEvidence->SetNumberField(TEXT("activeTurnsAfter"), Turns);
            DialogueEvidence->SetBoolField(TEXT("memoryAdvanced"), bMemoryAdvanced);
            if (Inventory)
                DialogueEvidence->SetArrayField(TEXT("inventoryAfter"), *Inventory);
            if (!bMemoryAdvanced && Elapsed - StepAt < 5)
                return true;
            const bool bPass = bAccepted && bInventoryUnchanged && bXpUnchanged && bMemoryAdvanced;
            Finish(bPass,
                   FString::Printf(TEXT("Rowan reply received; accepted=%d inventoryUnchanged=%d xpUnchanged=%d "
                                        "memoryAdvanced=%d. External provider audit must verify generated provenance."),
                                   bAccepted, bInventoryUnchanged, bXpUnchanged, bMemoryAdvanced));
            return false;
        }
    }
    else if (Scenario == TEXT("persist-write"))
    {
        if (Step == 0 && Elapsed > 1)
        {
            Send(TEXT("{\"type\":\"color\",\"index\":21}"));
            Send(TEXT(
                "{\"type\":\"chat\",\"channel\":\"ic\",\"text\":\"/me watches the rain with patient curiosity\"}"));
            Send(TEXT("{\"type\":\"path\",\"x\":18.5,\"y\":12.5}"));
            Next();
        }
        else if (Step == 1 && Near(18.5, 12.5) && Elapsed - StepAt > 1)
        {
            Send(TEXT("{\"type\":\"action\",\"target\":\"npc_keeper\",\"action\":\"talk\"}"));
            Next();
        }
        else if (Step == 2 && Elapsed - StepAt > 6)
        {
            Finish(true, TEXT("Authored character state and NPC conversation persisted"));
            return false;
        }
    }
    else if (Scenario == TEXT("persist-read") || Scenario == TEXT("persist-aged"))
    {
        if (Elapsed > 2)
        {
            FString State;
            (*Self)->TryGetStringField(TEXT("state"), State);
            double Color = 0;
            (*Self)->TryGetNumberField(TEXT("color"), Color);
            const TSharedPtr<FJsonObject>* Memory = nullptr;
            double Active = 0, Summaries = 0;
            if (Snapshot->TryGetObjectField(TEXT("memory"), Memory))
            {
                (*Memory)->TryGetNumberField(TEXT("activeTurns"), Active);
                (*Memory)->TryGetNumberField(TEXT("summaries"), Summaries);
            }
            const bool bMemory = Scenario == TEXT("persist-aged") ? Summaries >= 1 && Active == 0 : Active >= 2;
            const bool bPass =
                State == TEXT("watches the rain with patient curiosity") && Color == 21 && Near(18.5, 12.5) && bMemory;
            Finish(bPass, FString::Printf(TEXT("color=%.0f position=%.2f,%.2f active=%.0f summaries=%.0f state=%s"),
                                          Color, FinalX, Y, Active, Summaries, *State));
            return false;
        }
    }
    else
    {
        Finish(false, TEXT("Unknown scenario"));
        return false;
    }
    return true;
}
} // namespace

void RegisterRatwScenario()
{
    if (!FParse::Value(FCommandLine::Get(), TEXT("RatwScenario="), Scenario))
        return;
    Role = TEXT("ash");
    FParse::Value(FCommandLine::Get(), TEXT("RatwIdentity="), Role);
    Output = FPaths::ProjectDir() / TEXT("artifacts/screenshots");
    FParse::Value(FCommandLine::Get(), TEXT("RatwCaptureDir="), Output);
    ScenarioHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick));
}

void UnregisterRatwScenario()
{
    if (ScenarioHandle.IsValid())
        FTSTicker::GetCoreTicker().RemoveTicker(ScenarioHandle);
}
