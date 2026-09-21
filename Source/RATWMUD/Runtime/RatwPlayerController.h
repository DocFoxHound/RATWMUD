#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RatwPlayerController.generated.h"

class SRatwGame;

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
    void ClientEvent(const FString& Json);
    UFUNCTION(Client, Reliable) void ClientCompressedEvent(const TArray<uint8>& Compressed, int32 RawBytes);
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
    FString EntityId;
    FString DevelopmentIdentity;

  private:
    TSharedPtr<SRatwGame> GameWidget;
    FString PendingSnapshot;
    TArray<FString> ReceivedEvents;
    void ApplySnapshotJson(const FString& Json);
};
