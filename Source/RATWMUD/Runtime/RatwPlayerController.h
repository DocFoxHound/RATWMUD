#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Runtime/RatwSnapshotSections.h"
#include "RatwPlayerController.generated.h"

class SRatwGame;
class SRatwFrontDoor;
class FJsonObject;

UCLASS()
class RATWMUD_API ARatwPlayerController : public APlayerController
{
    GENERATED_BODY()
  public:
    ARatwPlayerController();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    UFUNCTION(Server, Reliable) void ServerCommand(const FString& Json);
    void ClientSnapshot(const FString& Json);
    UFUNCTION(Client, Unreliable) void ClientCompressedSnapshot(const TArray<uint8>& Compressed, int32 RawBytes);
    // A motion frame (as ratwmotion::Frame and the host's stamps make it), sent in binary.
    void ClientMotion(const TSharedPtr<FJsonObject>& Frame);
    UFUNCTION(Client, Unreliable) void ClientCompressedMotion(const TArray<uint8>& Compressed, int32 RawBytes);
    void ClientEvent(const FString& Json);
    UFUNCTION(Client, Reliable) void ClientCompressedEvent(const TArray<uint8>& Compressed, int32 RawBytes);
    // The client has applied the snapshot of this revision (or, Missing, lacks a part it was expected to hold).
    UFUNCTION(Server, Unreliable) void ServerSnapshotAck(double Revision, bool Missing);
    // Authority side (see RatwSnapshotSections.h): the parts of a snapshot this client is known to hold, and the keys
    // each snapshot not yet acknowledged carried, by revision. Reset when the client enters the world.
    ratwsections::FKeys KnownSections;
    TMap<double, ratwsections::FKeys> SentSections;
    int64 SnapshotBytesSent = 0;
    int32 SnapshotsSent = 0;
    void ResetSections()
    {
        KnownSections.Reset();
        SentSections.Reset();
    }
    void SubmitCommand(const FString& Json);
    const FString& GetLatestSnapshotJson() const
    {
        return PendingSnapshot;
    }
    const TArray<FString>& GetReceivedEvents() const
    {
        return ReceivedEvents;
    }
    TSharedPtr<SRatwGame> GetGameWidget() const
    {
        return GameWidget;
    }
    TSharedPtr<SRatwFrontDoor> GetFrontDoorWidget() const { return FrontDoorWidget; }
    TSharedPtr<FJsonObject> GetLatestLobby() const { return PendingLobby; }
    FString EntityId;
    FString DevelopmentIdentity;
    // Authority-side session identity only; never a replicated property.
    FString AccountUsername;
    FString MotionSession, MotionCell;
    int32 MotionGeneration = 0;
    int32 GetMotionFrameCount() const { return MotionFrameCount; }
    int32 GetSnapshotCount() const { return SnapshotCount; }
    int64 GetSnapshotBytesReceived() const { return SnapshotBytesReceived; }
    bool AllowsLocalCredentials() const;

  private:
    TSharedPtr<SRatwGame> GameWidget;
    TSharedPtr<SRatwFrontDoor> FrontDoorWidget;
    TSharedPtr<FJsonObject> PendingLobby;
    FString PendingSnapshot;
    TArray<FString> ReceivedEvents;
    bool EnteredWorld = false;
    FString PresentationCharacterId;
    FString PresentationSession, PresentationCell;
    int32 PresentationGeneration = -1, NewestGeneration = -1;
    double SnapshotRevision = -1, MotionRevision = -1;
    int32 MotionFrameCount = 0, SnapshotCount = 0;
    int64 SnapshotBytesReceived = 0;
    ratwsections::FCache Sections;
    void ShowLobby(const TSharedPtr<FJsonObject>& Event);
    void ShowGame();
    void ApplySnapshotJson(const FString& Json);
};
