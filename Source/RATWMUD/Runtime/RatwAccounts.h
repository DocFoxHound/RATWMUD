#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// Trusted-local development accounts. The owner map and password verifiers
// are serialized only into the private atomic world checkpoint, never a view.
class FRatwAccounts
{
  public:
    static constexpr int32 PasswordIterations = 600000;
    static constexpr int32 CharacterSlots = 6;
    static constexpr int32 AccountLimit = 128;

    static bool NormalizeUsername(const FString& Input, FString& Normalized);
    static bool ValidPassword(const FString& Password);
    static bool ValidDisplayName(const FString& Name);
    static bool ValidCommandId(const FString& Id);
    static bool IsLoopbackAddress(const FString& Address);
    static FString Fingerprint(const FString& Value);

    bool Register(const FString& Username, const FString& Password, FString& Error);
    bool Authenticate(const FString& Username, const FString& Password) const;
    bool Exists(const FString& Username) const;
    bool Owns(const FString& Username, const FString& CharacterId) const;
    TArray<FString> Characters(const FString& Username) const;
    bool AddCharacter(const FString& Username, const FString& CharacterId,
                      const FString& CommandId, const FString& RequestFingerprint);
    // An empty result means no matching receipt; Conflict detects ID recycling.
    FString CreatedCharacter(const FString& Username, const FString& CommandId,
                             const FString& RequestFingerprint, bool& Conflict) const;
    TSharedPtr<FJsonObject> State() const;
    bool Restore(const TSharedPtr<FJsonObject>& State);
    // Both directions: every owner references a saved character and every
    // generated wolf character has an owner. Legacy player-* stays unowned.
    bool ReferencesOnly(const TSet<FString>& CharacterIds) const;

  private:
    struct FCreation
    {
        FString Character, Fingerprint;
    };
    struct FAccount
    {
        FString Salt, Verifier;
        int32 Iterations = PasswordIterations;
        TArray<FString> Characters;
        TMap<FString, FCreation> Creations;
    };
    TMap<FString, FAccount> Accounts;
};

// The global budget also limits reconnects. Monotonic time, never game time.
class FRatwAccountRateLimit
{
  public:
    bool Allow(const FString& Key, double Now);
    void Forget(const FString& Key) { Peers.Remove(Key); }

  private:
    TArray<double> Global;
    TMap<FString, TArray<double>> Peers;
};
