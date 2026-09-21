#pragma once
#include "CoreMinimal.h"

struct FRatwDialogueContext
{
    FString NpcId, Name, Description, Activity, PlayerName, HeardText, Memory, Scene;
};

// Provider-neutral local endpoint protocol. A provider returns prose only;
// authoritative commands are never accepted from its response.
class FRatwDialogueProvider
{
  public:
    void Configure(const FString& Endpoint);
    FString Label() const;
    void Reply(const FRatwDialogueContext& Context, TFunction<void(FString)> Completion);
    static FString AuthoredReply(const FRatwDialogueContext& Context);

  private:
    FString LocalEndpoint;
};
