#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RatwGameMode.generated.h"

class ARatwPlayerController;
class FRatwRuntime;

UCLASS()
class RATWMUD_API ARatwGameMode : public AGameModeBase
{
    GENERATED_BODY()
  public:
    ARatwGameMode();
    virtual ~ARatwGameMode() override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    void HandleCommand(ARatwPlayerController* Controller, const FString& Json);

  private:
    TSharedPtr<FRatwRuntime> Runtime;
    float TickAccumulator = 0.f;
};
