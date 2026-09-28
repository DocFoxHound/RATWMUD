#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Hash/CityHash.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Delta snapshots (Docs/Design/26-living-npcs.md, Phase 6). A snapshot's big parts (the cell's ground and heights,
// what the wolf can see, the maps, the doors, the satchel) change far less often than snapshots are sent, five times a
// second. Each part goes with a key naming its content. Once the client has acknowledged a snapshot, the server leaves
// out every part whose key the client is known to hold, and the client puts its kept copy back before anything uses
// the snapshot. Snapshots travel unreliably, so the server relies only on what an acknowledgement proves arrived; a
// client missing a part asks for everything again.
namespace ratwsections
{
struct FSection
{
    const TCHAR* Parent;   // The object holding it: "" for the snapshot itself.
    const TCHAR* Field;
    const TCHAR* Name;     // Its name among the keys.
};

inline const TArray<FSection>& Sections()
{
    static const TArray<FSection> All = {
        {TEXT(""), TEXT("visibility"), TEXT("visibility")}, {TEXT("cell"), TEXT("rows"), TEXT("cell.rows")},
        {TEXT("cell"), TEXT("heights"), TEXT("cell.heights")}, {TEXT(""), TEXT("worldMap"), TEXT("worldMap")},
        {TEXT(""), TEXT("travelMap"), TEXT("travelMap")}, {TEXT(""), TEXT("doors"), TEXT("doors")},
        {TEXT(""), TEXT("inventory"), TEXT("inventory")}};
    return All;
}

inline TSharedPtr<FJsonObject> ParentOf(const TSharedPtr<FJsonObject>& Root, const FSection& Section)
{
    if (!*Section.Parent)
        return Root;
    const TSharedPtr<FJsonObject>* Child = nullptr;
    return Root->TryGetObjectField(Section.Parent, Child) && Child ? *Child : nullptr;
}

inline FString KeyOf(const TSharedPtr<FJsonValue>& Value)
{
    FString Json;
    const auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
    FJsonSerializer::Serialize(Value, FString(), Writer);
    const FTCHARToUTF8 Utf8(*Json);
    return FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(CityHash64(Utf8.Get(), Utf8.Length())));
}

using FKeys = TMap<FString, FString>;

// The server's side: gives every part its key (in "sectionKeys") and leaves out those the client holds. Returns this
// snapshot's keys, which become what the client holds once it acknowledges the snapshot.
inline FKeys Strip(const TSharedPtr<FJsonObject>& Root, const FKeys& Known)
{
    FKeys Keys;
    const auto KeysJson = MakeShared<FJsonObject>();
    for (const auto& Section : Sections())
    {
        const auto Parent = ParentOf(Root, Section);
        const auto Value = Parent.IsValid() ? Parent->TryGetField(Section.Field) : nullptr;
        if (!Value.IsValid())
            continue;
        const FString Key = KeyOf(Value);
        Keys.Add(Section.Name, Key);
        KeysJson->SetStringField(Section.Name, Key);
        if (const FString* Held = Known.Find(Section.Name); Held && *Held == Key)
            Parent->RemoveField(Section.Field);
    }
    Root->SetObjectField(TEXT("sectionKeys"), KeysJson);
    return Keys;
}

// The client's side: the newest few versions it has received of each part.
constexpr int32 KeptPerSection = 6;
struct FCache
{
    TMap<FString, TArray<TPair<FString, TSharedPtr<FJsonValue>>>> Kept;
    void Reset() { Kept.Reset(); }
};

// Puts back what the server left out and keeps what it sent, leaving the snapshot as if sent whole. False if a part
// left out isn't kept here (the client then asks for everything again, and doesn't use this snapshot).
inline bool Fill(const TSharedPtr<FJsonObject>& Root, FCache& Cache)
{
    const TSharedPtr<FJsonObject>* Keys = nullptr;
    if (!Root->TryGetObjectField(TEXT("sectionKeys"), Keys) || !Keys || !Keys->IsValid())
        return true;                                   // Sent whole, as an older server does.
    bool Complete = true;
    for (const auto& Section : Sections())
    {
        FString Key;
        if (!(*Keys)->TryGetStringField(Section.Name, Key))
            continue;
        const auto Parent = ParentOf(Root, Section);
        if (!Parent.IsValid())
        {
            Complete = false;
            continue;
        }
        auto& Kept = Cache.Kept.FindOrAdd(Section.Name);
        if (const auto Value = Parent->TryGetField(Section.Field); Value.IsValid())
        {
            Kept.RemoveAll([&](const TPair<FString, TSharedPtr<FJsonValue>>& Pair) { return Pair.Key == Key; });
            Kept.Add({Key, Value});
            if (Kept.Num() > KeptPerSection)
                Kept.RemoveAt(0);
            continue;
        }
        const auto* Found = Kept.FindByPredicate([&](const TPair<FString, TSharedPtr<FJsonValue>>& Pair) { return Pair.Key == Key; });
        if (!Found)
        {
            Complete = false;
            continue;
        }
        Parent->SetField(Section.Field, Found->Value);
    }
    Root->RemoveField(TEXT("sectionKeys"));
    return Complete;
}
} // namespace ratwsections
