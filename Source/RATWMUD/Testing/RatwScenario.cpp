#include "Testing/RatwScenario.h"
#include "Runtime/RatwPlayerController.h"
#include "UI/SRatwGame.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
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
    TArray<TSharedPtr<FJsonValue>> Events;
    if (Controller.IsValid())
    {
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

    if (Scenario == TEXT("gallery"))
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
            const bool bPass = bMoved && bSawOther && bSawOOC && bLong;
            Finish(bPass,
                   FString::Printf(TEXT("moved=%d otherIC=%d OOC=%d longPost=%d"), bMoved, bSawOther, bSawOOC, bLong));
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
