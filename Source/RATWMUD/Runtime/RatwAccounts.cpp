#include "Runtime/RatwAccounts.h"
#include "Runtime/RatwJson.h"

THIRD_PARTY_INCLUDES_START
// OpenSSL's UI typedef otherwise collides with Unreal's namespace UI.
#define UI OpenSSL_UI
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#undef UI
THIRD_PARTY_INCLUDES_END

namespace
{
FString Hex(const uint8* Bytes, int32 Count)
{
    static const TCHAR* Digits = TEXT("0123456789abcdef");
    FString Out;
    Out.Reserve(Count * 2);
    for (int32 I = 0; I < Count; ++I)
    {
        Out.AppendChar(Digits[Bytes[I] >> 4]);
        Out.AppendChar(Digits[Bytes[I] & 15]);
    }
    return Out;
}
bool ReadHex(const FString& Value, uint8* Out, int32 Count)
{
    if (Value.Len() != Count * 2) return false;
    auto Nibble = [](TCHAR C) {
        return C >= '0' && C <= '9' ? int32(C - '0') : C >= 'a' && C <= 'f' ? int32(C - 'a' + 10) : -1;
    };
    for (int32 I = 0; I < Count; ++I)
    {
        const int32 A = Nibble(Value[I * 2]), B = Nibble(Value[I * 2 + 1]);
        if (A < 0 || B < 0) return false;
        Out[I] = uint8((A << 4) | B);
    }
    return true;
}
bool Derive(const FString& Password, const uint8* Salt, int32 Iterations, uint8* Out)
{
    FTCHARToUTF8 Utf8(*Password);
    return PKCS5_PBKDF2_HMAC(Utf8.Get(), Utf8.Length(), Salt, 32, Iterations, EVP_sha256(), 32, Out) == 1;
}
bool ExactFields(const ratwjson::Object& Object, std::initializer_list<const TCHAR*> Fields)
{
    if (!Object.IsValid() || Object->Values.Num() != int32(Fields.size())) return false;
    for (const auto* Key : Fields) if (!Object->HasField(Key)) return false;
    return true;
}
bool StrictString(const ratwjson::Object& Object, const TCHAR* Key, FString& Out)
{
    const auto* V = Object.IsValid() ? Object->Values.Find(Key) : nullptr;
    return V && V->IsValid() && (*V)->Type == EJson::String && (*V)->TryGetString(Out);
}
bool CharacterId(const FString& Id)
{
    if (!Id.StartsWith(TEXT("wolf-")) || Id.Len() != 37) return false;
    for (int32 I = 5; I < Id.Len(); ++I)
        if (!(Id[I] >= '0' && Id[I] <= '9') && !(Id[I] >= 'a' && Id[I] <= 'f')) return false;
    return true;
}
}

bool FRatwAccounts::NormalizeUsername(const FString& Input, FString& Normalized)
{
    if (Input.Len() < 3 || Input.Len() > 32) return false;
    const FString Lower = Input.ToLower();
    if (Lower[0] < 'a' || Lower[0] > 'z') return false;
    for (TCHAR C : Lower)
        if (!(C >= 'a' && C <= 'z') && !(C >= '0' && C <= '9') && C != '_' && C != '-') return false;
    Normalized = Lower;
    return true;
}
bool FRatwAccounts::ValidPassword(const FString& Password)
{
    if (Password.Len() > 128) return false;
    for (TCHAR C : Password) if (C < 32 || C == 127) return false;
    const FTCHARToUTF8 Utf8(*Password);
    return Utf8.Length() >= 12 && Utf8.Length() <= 128;
}
bool FRatwAccounts::ValidDisplayName(const FString& Name)
{
    if (Name.Len() < 2 || Name.Len() > 32 || Name.TrimStartAndEnd() != Name) return false;
    for (TCHAR C : Name) if (C < 32 || C == 127) return false;
    return true;
}
bool FRatwAccounts::ValidCommandId(const FString& Id)
{
    if (Id.IsEmpty() || Id.Len() > 128) return false;
    for (TCHAR C : Id)
        if (!(C >= 'a' && C <= 'z') && !(C >= 'A' && C <= 'Z') && !(C >= '0' && C <= '9') && C != '-' && C != '_') return false;
    return true;
}
bool FRatwAccounts::IsLoopbackAddress(const FString& Address)
{
    FString Ip = Address;
    if (Ip.StartsWith(TEXT("[")) && Ip.EndsWith(TEXT("]"))) Ip = Ip.Mid(1, Ip.Len() - 2);
    if (Ip == TEXT("::1") || Ip == TEXT("0:0:0:0:0:0:0:1")) return true;
    if (Ip.StartsWith(TEXT("::ffff:"), ESearchCase::IgnoreCase)) Ip = Ip.Mid(7);
    TArray<FString> Parts;
    Ip.ParseIntoArray(Parts, TEXT("."), false);
    if (Parts.Num() != 4 || Parts[0] != TEXT("127")) return false;
    for (const auto& Part : Parts)
    {
        if (Part.IsEmpty() || Part.Len() > 3 || (Part.Len() > 1 && Part[0] == '0')) return false;
        int32 Value = 0;
        for (TCHAR C : Part) { if (C < '0' || C > '9') return false; Value = Value * 10 + C - '0'; }
        if (Value > 255) return false;
    }
    return true;
}
FString FRatwAccounts::Fingerprint(const FString& Value)
{
    const FTCHARToUTF8 Utf8(*Value);
    uint8 Digest[32];
    SHA256(reinterpret_cast<const unsigned char*>(Utf8.Get()), Utf8.Length(), Digest);
    return Hex(Digest, 32);
}
bool FRatwAccounts::Register(const FString& Username, const FString& Password, FString& Error)
{
    FString User;
    if (!NormalizeUsername(Username, User) || !ValidPassword(Password))
    {
        Error = TEXT("Use a 3–32 character username starting with a letter (letters, digits, _ or -), and a 12–128 byte password without control characters.");
        return false;
    }
    if (Accounts.Contains(User) || Accounts.Num() >= AccountLimit)
    {
        Error = TEXT("This account cannot be registered. Try another username, or sign in to an existing account.");
        return false;
    }
    uint8 Salt[32], Verifier[32];
    if (RAND_bytes(Salt, sizeof(Salt)) != 1 || !Derive(Password, Salt, PasswordIterations, Verifier))
    {
        Error = TEXT("Secure password storage is unavailable; no account was created.");
        return false;
    }
    FAccount Account;
    Account.Salt = Hex(Salt, 32);
    Account.Verifier = Hex(Verifier, 32);
    OPENSSL_cleanse(Verifier, sizeof(Verifier));
    Accounts.Add(User, MoveTemp(Account));
    return true;
}
bool FRatwAccounts::Authenticate(const FString& Username, const FString& Password) const
{
    FString User;
    if (!NormalizeUsername(Username, User) || !ValidPassword(Password)) return false;
    const auto* Account = Accounts.Find(User);
    // Missing users pay the same bounded PBKDF2 cost; response text is identical.
    uint8 Salt[32] = {}, Expected[32] = {}, Actual[32] = {};
    if (Account && (!ReadHex(Account->Salt, Salt, 32) || !ReadHex(Account->Verifier, Expected, 32))) return false;
    const bool Derived = Derive(Password, Salt, Account ? Account->Iterations : PasswordIterations, Actual);
    const bool Equal = Derived && CRYPTO_memcmp(Actual, Expected, 32) == 0;
    OPENSSL_cleanse(Actual, sizeof(Actual));
    OPENSSL_cleanse(Expected, sizeof(Expected));
    return Account && Equal;
}
bool FRatwAccounts::Exists(const FString& Username) const { return Accounts.Contains(Username); }
bool FRatwAccounts::Owns(const FString& Username, const FString& Id) const
{
    const auto* Account = Accounts.Find(Username);
    return Account && Account->Characters.Contains(Id);
}
TArray<FString> FRatwAccounts::Characters(const FString& Username) const
{
    const auto* Account = Accounts.Find(Username);
    return Account ? Account->Characters : TArray<FString>();
}
bool FRatwAccounts::AddCharacter(const FString& Username, const FString& Id,
                               const FString& CommandId, const FString& RequestFingerprint)
{
    auto* Account = Accounts.Find(Username);
    if (!Account || Account->Characters.Num() >= CharacterSlots || !CharacterId(Id) || !ValidCommandId(CommandId) ||
        Account->Creations.Contains(CommandId) || RequestFingerprint.Len() != 64) return false;
    for (const auto& Pair : Accounts) if (Pair.Value.Characters.Contains(Id)) return false;
    Account->Characters.Add(Id);
    Account->Creations.Add(CommandId, {Id, RequestFingerprint});
    return true;
}
FString FRatwAccounts::CreatedCharacter(const FString& Username, const FString& CommandId,
                                      const FString& RequestFingerprint, bool& Conflict) const
{
    Conflict = false;
    const auto* Account = Accounts.Find(Username);
    const auto* Creation = Account ? Account->Creations.Find(CommandId) : nullptr;
    if (!Creation) return {};
    Conflict = Creation->Fingerprint != RequestFingerprint;
    return Conflict ? FString() : Creation->Character;
}
TSharedPtr<FJsonObject> FRatwAccounts::State() const
{
    using namespace ratwjson;
    auto Root = New();
    Root->SetNumberField(TEXT("version"), 1);
    Array Entries;
    TArray<FString> Users;
    Accounts.GetKeys(Users); Users.Sort();
    for (const auto& User : Users)
    {
        const auto& Account = Accounts[User];
        auto Entry = New();
        Entry->SetStringField(TEXT("username"), User);
        Entry->SetStringField(TEXT("salt"), Account.Salt);
        Entry->SetStringField(TEXT("verifier"), Account.Verifier);
        Entry->SetNumberField(TEXT("iterations"), Account.Iterations);
        Array Ids;
        for (const auto& Id : Account.Characters) Ids.Add(V(Id));
        Entry->SetArrayField(TEXT("characters"), Ids);
        auto Creations = New();
        for (const auto& Pair : Account.Creations)
        {
            auto Receipt = New();
            Receipt->SetStringField(TEXT("character"), Pair.Value.Character);
            Receipt->SetStringField(TEXT("fingerprint"), Pair.Value.Fingerprint);
            Creations->SetObjectField(Pair.Key, Receipt);
        }
        Entry->SetObjectField(TEXT("creations"), Creations);
        Entries.Add(V(Entry));
    }
    Root->SetArrayField(TEXT("entries"), Entries);
    return Root;
}
bool FRatwAccounts::Restore(const TSharedPtr<FJsonObject>& Root)
{
    using namespace ratwjson;
    if (!ExactFields(Root, {TEXT("version"), TEXT("entries")}) || StrictNumber(Root, TEXT("version"), -1) != 1) return false;
    const Array* Entries;
    if (!Root->TryGetArrayField(TEXT("entries"), Entries) || Entries->Num() > AccountLimit) return false;
    TMap<FString, FAccount> Candidate;
    TSet<FString> Owned;
    for (const auto& Value : *Entries)
    {
        if (!Value.IsValid() || Value->Type != EJson::Object) return false;
        const auto Entry = Value->AsObject();
        if (!ExactFields(Entry, {TEXT("username"), TEXT("salt"), TEXT("verifier"), TEXT("iterations"), TEXT("characters"), TEXT("creations")})) return false;
        FAccount Account;
        FString User, Normalized;
        uint8 Bytes[32];
        const double Iterations = StrictNumber(Entry, TEXT("iterations"), -1);
        if (!StrictString(Entry, TEXT("username"), User) || !NormalizeUsername(User, Normalized) || User != Normalized || Candidate.Contains(User) ||
            !StrictString(Entry, TEXT("salt"), Account.Salt) || !ReadHex(Account.Salt, Bytes, 32) ||
            !StrictString(Entry, TEXT("verifier"), Account.Verifier) || !ReadHex(Account.Verifier, Bytes, 32) ||
            Iterations != PasswordIterations) return false;
        const Array* Ids;
        if (!Entry->TryGetArrayField(TEXT("characters"), Ids) || Ids->Num() > CharacterSlots) return false;
        for (const auto& IdValue : *Ids)
        {
            FString Id;
            if (!IdValue.IsValid() || IdValue->Type != EJson::String || !IdValue->TryGetString(Id) || !CharacterId(Id) || Owned.Contains(Id)) return false;
            Owned.Add(Id); Account.Characters.Add(Id);
        }
        const auto Creations = Child(Entry, TEXT("creations"));
        if (!Creations.IsValid() || Creations->Values.Num() != Account.Characters.Num()) return false;
        TSet<FString> Created;
        for (const auto& Pair : Creations->Values)
        {
            const FString RequestId(Pair.Key.ToView());
            if (!ValidCommandId(RequestId) || !Pair.Value.IsValid() || Pair.Value->Type != EJson::Object) return false;
            const auto Receipt = Pair.Value->AsObject();
            FCreation Creation;
            if (!ExactFields(Receipt, {TEXT("character"), TEXT("fingerprint")}) ||
                !StrictString(Receipt, TEXT("character"), Creation.Character) || !Account.Characters.Contains(Creation.Character) || Created.Contains(Creation.Character) ||
                !StrictString(Receipt, TEXT("fingerprint"), Creation.Fingerprint) || !ReadHex(Creation.Fingerprint, Bytes, 32)) return false;
            Created.Add(Creation.Character); Account.Creations.Add(RequestId, MoveTemp(Creation));
        }
        Candidate.Add(User, MoveTemp(Account));
    }
    Accounts = MoveTemp(Candidate);
    return true;
}
bool FRatwAccounts::ReferencesOnly(const TSet<FString>& CharacterIds) const
{
    TSet<FString> Owned;
    for (const auto& Pair : Accounts)
        for (const auto& Id : Pair.Value.Characters)
        {
            if (!CharacterIds.Contains(Id)) return false;
            Owned.Add(Id);
        }
    // Legacy development characters are deliberately unowned; the generated
    // wolf namespace never is, including when an old save omits accounts.
    for (const auto& Id : CharacterIds)
        if (Id.StartsWith(TEXT("wolf-")) && (!CharacterId(Id) || !Owned.Contains(Id))) return false;
    return true;
}
bool FRatwAccountRateLimit::Allow(const FString& Key, double Now)
{
    if (!FMath::IsFinite(Now) || Key.IsEmpty()) return false;
    Global.RemoveAll([Now](double At) { return At <= Now - 60 || At > Now; });
    auto& Peer = Peers.FindOrAdd(Key);
    Peer.RemoveAll([Now](double At) { return At <= Now - 60 || At > Now; });
    if (Global.Num() >= 24 || Peer.Num() >= 6) return false;
    Global.Add(Now); Peer.Add(Now);
    return true;
}
