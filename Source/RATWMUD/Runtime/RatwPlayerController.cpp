#include "Runtime/RatwPlayerController.h"
#include "Runtime/RatwGameMode.h"
#include "Runtime/RatwSnapshotCodec.h"
#include "Runtime/RatwMotion.h"
#include "Runtime/RatwAccounts.h"
#include "Runtime/RatwRemoteLink.h"
#include "UI/SRatwGame.h"
#include "UI/SRatwFrontDoor.h"
#include "Dom/JsonObject.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Engine/NetConnection.h"
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
    FString ServerAddress;
    if (FParse::Value(FCommandLine::Get(), TEXT("RatwServer="), ServerAddress))
    {
        Remote = MakeShared<FRatwRemoteLink>();
        FString Problem;
        if (!Remote->Connect(ServerAddress, Problem))
        {
            UE_LOG(LogTemp, Error, TEXT("RATW standalone server unreachable: %s"), *Problem);
            PendingLobby = MakeShared<FJsonObject>();
            PendingLobby->SetStringField(TEXT("type"), TEXT("lobby"));
            PendingLobby->SetStringField(TEXT("stage"), TEXT("login"));
            PendingLobby->SetBoolField(TEXT("ok"), false);
            PendingLobby->SetStringField(TEXT("message"), TEXT("The world server could not be reached: ") + Problem);
            PendingLobby->SetArrayField(TEXT("characters"), {});
        }
        else
            UE_LOG(LogTemp, Display, TEXT("RATW connected to the standalone server at %s"), *ServerAddress);
    }
    const bool Development = FParse::Param(FCommandLine::Get(), TEXT("RatwDevIdentity"));
    if (!Development)
    {
        if (!PendingLobby.IsValid())
        {
            PendingLobby = MakeShared<FJsonObject>();
            PendingLobby->SetStringField(TEXT("type"), TEXT("lobby"));
            PendingLobby->SetStringField(TEXT("stage"), TEXT("login"));
            PendingLobby->SetBoolField(TEXT("ok"), true);
            PendingLobby->SetStringField(TEXT("message"), TEXT("Local development accounts only. Use a unique test password; native transport is unencrypted."));
            PendingLobby->SetArrayField(TEXT("characters"), {});
        }
        ShowLobby(PendingLobby);
        return;
    }
    ShowGame();
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
    SendToServer(Json);
    if (!PendingSnapshot.IsEmpty())
        ApplySnapshotJson(PendingSnapshot);
}

void ARatwPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
    if (GameWidget.IsValid() && GetWorld() && GetWorld()->GetGameViewport())
        GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(GameWidget.ToSharedRef());
    GameWidget.Reset();
    if (FrontDoorWidget.IsValid() && GetWorld() && GetWorld()->GetGameViewport())
        GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(FrontDoorWidget.ToSharedRef());
    FrontDoorWidget.Reset(); PendingLobby.Reset(); PendingSnapshot.Empty(); ReceivedEvents.Empty();
    Super::EndPlay(Reason);
}

void ARatwPlayerController::SubmitCommand(const FString& Json)
{
    TSharedPtr<FJsonObject> Command;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Command) || !Command.IsValid())
        return;
    FString Type;
    Command->TryGetStringField(TEXT("type"), Type);
    if ((Type == TEXT("auth_login") || Type == TEXT("auth_register")) && !AllowsLocalCredentials())
    {
        auto Denied = MakeShared<FJsonObject>();
        Denied->SetStringField(TEXT("type"), TEXT("lobby"));
        Denied->SetStringField(TEXT("stage"), TEXT("login"));
        Denied->SetBoolField(TEXT("ok"), false);
        Denied->SetStringField(TEXT("message"), TEXT("No credentials were sent. Account login requires standalone or loopback networking on this computer; remote native transport is unencrypted."));
        Denied->SetArrayField(TEXT("characters"), {});
        ShowLobby(Denied);
        return;
    }
    if (!Command->HasField(TEXT("commandId")))
        Command->SetStringField(TEXT("commandId"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens));
    FString Encoded;
    FJsonSerializer::Serialize(Command.ToSharedRef(),
                               TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Encoded));
    SendToServer(Encoded);
}

void ARatwPlayerController::SendToServer(const FString& Json)
{
    if (Remote.IsValid())
        Remote->SendCommand(Json);
    else
        ServerCommand(Json);
}

void ARatwPlayerController::SendAck(double Revision, bool Missing)
{
    if (Remote.IsValid())
        Remote->SendAck(Revision, Missing);
    else
        ServerSnapshotAck(Revision, Missing);
}

void ARatwPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    if (!Remote.IsValid())
        return;
    const bool WasOpen = Remote->IsOpen();
    Remote->Poll([this](ratw::link::Kind Kind, const TArray<uint8>& Raw, int32 WireBytes) {
        if (Kind == ratw::link::Motion)
        {
            if (EnteredWorld)
                if (const auto Frame = ratwmotion::Unpack(Raw))
                    ApplyMotionFrame(Frame);
            return;
        }
        FString Json;
        {
            const FUTF8ToTCHAR Text(reinterpret_cast<const ANSICHAR*>(Raw.GetData()), Raw.Num());
            Json = FString(Text.Length(), Text.Get());
        }
        if (Kind == ratw::link::Snapshot)
        {
            SnapshotBytesReceived += WireBytes;
            ApplySnapshotJson(Json);
        }
        else if (Kind == ratw::link::Event)
            ReceiveEventJson(Json);
    });
    if (WasOpen && !Remote->IsOpen())
    {
        UE_LOG(LogTemp, Warning, TEXT("RATW the standalone server closed the connection."));
        auto Lost = MakeShared<FJsonObject>();
        Lost->SetStringField(TEXT("type"), TEXT("lobby"));
        Lost->SetStringField(TEXT("stage"), TEXT("login"));
        Lost->SetBoolField(TEXT("ok"), false);
        Lost->SetStringField(TEXT("message"), TEXT("The connection to the world server was lost."));
        Lost->SetArrayField(TEXT("characters"), {});
        ShowLobby(Lost);
    }
}
bool ARatwPlayerController::AllowsLocalCredentials() const
{
    if (Remote.IsValid()) return Remote->IsLoopback();     // The standalone server decides too.
    if (!GetWorld()) return false;
    if (GetWorld()->GetNetMode() == NM_Standalone || (HasAuthority() && IsLocalController())) return true;
    const auto* Connection = GetNetConnection();
    return Connection && FRatwAccounts::IsLoopbackAddress(const_cast<UNetConnection*>(Connection)->LowLevelGetRemoteAddress(false));
}
void ARatwPlayerController::ShowLobby(const TSharedPtr<FJsonObject>& Event)
{
    PendingLobby = Event;
    EnteredWorld = false;
    PresentationCharacterId.Empty();
    PresentationSession.Empty(); PresentationCell.Empty();
    PresentationGeneration = NewestGeneration = -1;
    SnapshotRevision = MotionRevision = -1;
    PendingSnapshot.Empty(); ReceivedEvents.Empty();
    if (!IsLocalController() || !GetWorld() || !GetWorld()->GetGameViewport() || !FSlateApplication::IsInitialized()) return;
    if (GameWidget.IsValid())
        GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(GameWidget.ToSharedRef());
    GameWidget.Reset();
    if (!FrontDoorWidget.IsValid())
    {
        SAssignNew(FrontDoorWidget, SRatwFrontDoor).OnCommand(TFunction<void(const FString&)>([this](const FString& Json) { SubmitCommand(Json); }));
        GetWorld()->GetGameViewport()->AddViewportWidgetContent(FrontDoorWidget.ToSharedRef());
    }
    FrontDoorWidget->ReceiveEvent(Event);
    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(FrontDoorWidget);
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
}
void ARatwPlayerController::ShowGame()
{
    EnteredWorld = true;
    PendingLobby.Reset();
    if (!IsLocalController() || !GetWorld() || !GetWorld()->GetGameViewport() || !FSlateApplication::IsInitialized()) return;
    if (FrontDoorWidget.IsValid())
        GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(FrontDoorWidget.ToSharedRef());
    FrontDoorWidget.Reset();
    if (!GameWidget.IsValid())
    {
        SAssignNew(GameWidget, SRatwGame).OnCommand(TFunction<void(const FString&)>([this](const FString& Json) { SubmitCommand(Json); }));
        GetWorld()->GetGameViewport()->AddViewportWidgetContent(GameWidget.ToSharedRef());
    }
    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(GameWidget);
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
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
    {
        SnapshotBytesSent += Compressed.Num();
        ++SnapshotsSent;
        ClientCompressedSnapshot(Compressed, RawBytes);
    }
    else
        UE_LOG(LogTemp, Error, TEXT("RATW snapshot exceeds bounded network envelope"));
}

void ARatwPlayerController::ClientCompressedSnapshot_Implementation(const TArray<uint8>& Compressed, int32 RawBytes)
{
    FString Json;
    SnapshotBytesReceived += Compressed.Num();
    if (ratwwire::Decode(Compressed, RawBytes, Json))
        ApplySnapshotJson(Json);
}

void ARatwPlayerController::ServerSnapshotAck_Implementation(double Revision, bool Missing)
{
    if (auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ARatwGameMode>() : nullptr)
        Mode->HandleAcknowledge(this, Revision, Missing);
}

void ARatwPlayerController::ApplySnapshotJson(const FString& Json)
{
    if (!EnteredWorld) return;
    TSharedPtr<FJsonObject> Object;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) || !Object.IsValid()) return;
    const TSharedPtr<FJsonObject>* Self = nullptr;
    FString Id;
    if (!Object->TryGetObjectField(TEXT("self"), Self) || !Self || !Self->IsValid() ||
        !(*Self)->TryGetStringField(TEXT("id"), Id) || Id != PresentationCharacterId) return;
    if (Object->GetStringField(TEXT("motionSession")) != PresentationSession) return;
    const int32 Generation = Object->GetIntegerField(TEXT("cellGeneration"));
    const double Revision = Object->GetNumberField(TEXT("revision"));
    if (Generation < NewestGeneration || Revision <= SnapshotRevision) return;
    // Put back the parts the server left out because this client holds them; lacking one, ask for everything.
    if (!ratwsections::Fill(Object, Sections))
    {
        SendAck(Revision, true);
        return;
    }
    SendAck(Revision, false);
    NewestGeneration = PresentationGeneration = Generation;
    PresentationCell = Object->GetObjectField(TEXT("cell"))->GetStringField(TEXT("id"));
    SnapshotRevision = Revision;
    ++SnapshotCount;
    {
        PendingSnapshot.Reset();                       // The whole snapshot, as tools and tests read it.
        const auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&PendingSnapshot);
        FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
    }
    if (GameWidget.IsValid()) GameWidget->ApplySnapshot(Object);
}

void ARatwPlayerController::ClientMotion(const TSharedPtr<FJsonObject>& Frame)
{
    TArray<uint8> Compressed;
    int32 RawBytes = 0;
    if (ratwwire::EncodeBytes(ratwmotion::Pack(Frame), Compressed, RawBytes)) ClientCompressedMotion(Compressed, RawBytes);
}

void ARatwPlayerController::ClientCompressedMotion_Implementation(const TArray<uint8>& Compressed, int32 RawBytes)
{
    if (!EnteredWorld) return;
    TArray<uint8> Bytes;
    if (!ratwwire::DecodeBytes(Compressed, RawBytes, Bytes)) return;
    const TSharedPtr<FJsonObject> Frame = ratwmotion::Unpack(Bytes);
    if (!Frame) return;
    ApplyMotionFrame(Frame);
}

void ARatwPlayerController::ApplyMotionFrame(const TSharedPtr<FJsonObject>& Frame)
{
    if (Frame->GetStringField(TEXT("motionSession")) != PresentationSession ||
        Frame->GetStringField(TEXT("observer")) != PresentationCharacterId) return;
    const int32 Generation = Frame->GetIntegerField(TEXT("cellGeneration"));
    const double Revision = Frame->GetNumberField(TEXT("revision"));
    if (Generation < NewestGeneration || Revision <= MotionRevision || Revision < SnapshotRevision) return;
    NewestGeneration = Generation;
    MotionRevision = Revision;
    if (Generation != PresentationGeneration || Frame->GetStringField(TEXT("cellId")) != PresentationCell) return;
    ++MotionFrameCount;
    if (GameWidget.IsValid()) GameWidget->ApplyMotion(Frame);
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
    ReceiveEventJson(Json);
}

void ARatwPlayerController::ReceiveEventJson(const FString& Json)
{
    TSharedPtr<FJsonObject> Object;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) || !Object.IsValid()) return;
    FString Type;
    Object->TryGetStringField(TEXT("type"), Type);
    if (Type == TEXT("lobby")) ShowLobby(Object);
    else if (Type == TEXT("entered"))
    {
        const FString Session = Object->GetStringField(TEXT("motionSession"));
        if (Session != PresentationSession)
        {
            PresentationSession = Session; PresentationCell.Empty();
            PresentationGeneration = NewestGeneration = -1;
            SnapshotRevision = MotionRevision = -1;
            MotionFrameCount = SnapshotCount = 0;
            PendingSnapshot.Empty();
            Sections.Reset();
        }
        Object->TryGetStringField(TEXT("id"), PresentationCharacterId);
        ShowGame();
    }
    else if (!EnteredWorld) return;
    ReceivedEvents.Add(Json);
    if (ReceivedEvents.Num() > 256)
        ReceivedEvents.RemoveAt(0);
    if (GameWidget.IsValid() && Type != TEXT("entered"))
        GameWidget->ReceiveEvent(Object);
    // Do not silently create plaintext roleplay archives in ordinary client logs.
    UE_LOG(LogTemp, Verbose, TEXT("RATW_EVENT received %d UTF-16 units"), Json.Len());
}
