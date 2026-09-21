#include "Runtime/RatwPlayerController.h"
#include "Runtime/RatwGameMode.h"
#include "Runtime/RatwSnapshotCodec.h"
#include "UI/SRatwGame.h"
#include "Dom/JsonObject.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Guid.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

ARatwPlayerController::ARatwPlayerController()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
}

void ARatwPlayerController::BeginPlay()
{
    Super::BeginPlay();
    // A packaged Game target can run an isolated listen host with no local wolf.
    // The source-engine Server target remains the production deployment choice.
    if (HasAuthority() && FParse::Param(FCommandLine::Get(), TEXT("RatwHeadlessHost")))
        return;
    if (!IsLocalController())
        return;
    if (GetWorld()->GetGameViewport() && FSlateApplication::IsInitialized())
    {
        SAssignNew(GameWidget, SRatwGame).OnCommand(TFunction<void(const FString&)>([this](const FString& Json) {
            SubmitCommand(Json);
        }));
        GetWorld()->GetGameViewport()->AddViewportWidgetContent(GameWidget.ToSharedRef());
        FInputModeUIOnly Mode;
        Mode.SetWidgetToFocus(GameWidget);
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(Mode);
    }
    FString Identity = TEXT("ash");
    FParse::Value(FCommandLine::Get(), TEXT("RatwIdentity="), Identity);
    FString Name = Identity;
    FParse::Value(FCommandLine::Get(), TEXT("RatwName="), Name);
    TSharedRef<FJsonObject> Hello = MakeShared<FJsonObject>();
    Hello->SetStringField(TEXT("type"), TEXT("hello"));
    Hello->SetStringField(TEXT("id"), Identity);
    Hello->SetStringField(TEXT("name"), Name);
    Hello->SetStringField(TEXT("commandId"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens));
    FString Json;
    FJsonSerializer::Serialize(Hello, TJsonWriterFactory<>::Create(&Json));
    ServerCommand(Json);
    if (!PendingSnapshot.IsEmpty())
        ApplySnapshotJson(PendingSnapshot);
}

void ARatwPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
    if (GameWidget.IsValid() && GetWorld() && GetWorld()->GetGameViewport())
        GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(GameWidget.ToSharedRef());
    GameWidget.Reset();
    Super::EndPlay(Reason);
}

void ARatwPlayerController::SubmitCommand(const FString& Json)
{
    TSharedPtr<FJsonObject> Command;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Command) || !Command.IsValid())
        return;
    if (!Command->HasField(TEXT("commandId")))
        Command->SetStringField(TEXT("commandId"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens));
    FString Encoded;
    FJsonSerializer::Serialize(Command.ToSharedRef(),
                               TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Encoded));
    ServerCommand(Encoded);
}
void ARatwPlayerController::ServerCommand_Implementation(const FString& Json)
{
    if (Json.Len() > 65536)
        return;
    if (auto* Mode = GetWorld()->GetAuthGameMode<ARatwGameMode>())
        Mode->HandleCommand(this, Json);
}

void ARatwPlayerController::ClientSnapshot(const FString& Json)
{
    TArray<uint8> Compressed;
    int32 RawBytes = 0;
    if (ratwwire::Encode(Json, Compressed, RawBytes))
        ClientCompressedSnapshot(Compressed, RawBytes);
    else
        UE_LOG(LogTemp, Error, TEXT("RATW snapshot exceeds bounded network envelope"));
}

void ARatwPlayerController::ClientCompressedSnapshot_Implementation(const TArray<uint8>& Compressed, int32 RawBytes)
{
    FString Json;
    if (ratwwire::Decode(Compressed, RawBytes, Json))
        ApplySnapshotJson(Json);
}

void ARatwPlayerController::ApplySnapshotJson(const FString& Json)
{
    PendingSnapshot = Json;
    if (!GameWidget.IsValid())
        return;
    TSharedPtr<FJsonObject> Object;
    if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) && Object.IsValid())
        GameWidget->ApplySnapshot(Object);
}

void ARatwPlayerController::ClientEvent(const FString& Json)
{
    TArray<uint8> Compressed;
    int32 RawBytes = 0;
    if (ratwwire::Encode(Json, Compressed, RawBytes))
        ClientCompressedEvent(Compressed, RawBytes);
    else
        UE_LOG(LogTemp, Error, TEXT("RATW event exceeds bounded network envelope"));
}

void ARatwPlayerController::ClientCompressedEvent_Implementation(const TArray<uint8>& Compressed, int32 RawBytes)
{
    FString Json;
    if (!ratwwire::Decode(Compressed, RawBytes, Json))
        return;
    ReceivedEvents.Add(Json);
    if (ReceivedEvents.Num() > 256)
        ReceivedEvents.RemoveAt(0);
    TSharedPtr<FJsonObject> Object;
    if (GameWidget.IsValid() && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) &&
        Object.IsValid())
        GameWidget->ReceiveEvent(Object);
    // Do not silently create plaintext roleplay archives in ordinary client logs.
    UE_LOG(LogTemp, Verbose, TEXT("RATW_EVENT received %d UTF-16 units"), Json.Len());
}
