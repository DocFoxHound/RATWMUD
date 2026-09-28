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
//
// The maps are lists of cells, each with an "id" and everything the wolf remembers of it. When a map has changed,
// usually only the cell the wolf is in has: each entry the client holds goes as {"$held": key} and is put back from
// its copy, so a reveal costs one cell, not the whole map. An entry sent whole comes with its key among the keys
// ("worldMap#<id>"), so the client never has to work keys out (the server's JSON may be written differently).
namespace ratwsections
{
struct FSection
{
    const TCHAR* Parent;   // The object holding it: "" for the snapshot itself.
    const TCHAR* Field;
    const TCHAR* Name;     // Its name among the keys.
    bool Entries = false;  // A list of objects with an "id", each held (and sent) on its own.
};

inline const TArray<FSection>& Sections()
{
    static const TArray<FSection> All = {
        {TEXT(""), TEXT("visibility"), TEXT("visibility")}, {TEXT("cell"), TEXT("rows"), TEXT("cell.rows")},
        {TEXT("cell"), TEXT("heights"), TEXT("cell.heights")}, {TEXT(""), TEXT("worldMap"), TEXT("worldMap"), true},
        {TEXT(""), TEXT("travelMap"), TEXT("travelMap"), true}, {TEXT(""), TEXT("doors"), TEXT("doors")},
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
        const bool Whole = [&] { const FString* Held = Known.Find(Section.Name); return Held && *Held == Key; }();
        const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
        if (!Section.Entries || !Value->TryGetArray(List) || !List)
        {
            if (Whole)
                Parent->RemoveField(Section.Field);
            continue;
        }
        if (Whole)
        {
            // Unchanged, so are its entries: the client holds them as it did (no need to work their keys out again).
            const FString Prefix = FString(Section.Name) + TEXT("#");
            for (const auto& Held : Known)
                if (Held.Key.StartsWith(Prefix))
                    Keys.Add(Held.Key, Held.Value);
            Parent->RemoveField(Section.Field);
            continue;
        }
        // Each entry the client holds goes only by its key ("worldMap#<id>" among the keys).
        TArray<TSharedPtr<FJsonValue>> Sent;
        bool Changed = false;
        for (const auto& Entry : *List)
        {
            const TSharedPtr<FJsonObject>* Object = nullptr;
            FString Id;
            if (!Entry.IsValid() || !Entry->TryGetObject(Object) || !Object || !(*Object)->TryGetStringField(TEXT("id"), Id))
            {
                Sent.Add(Entry);
                continue;
            }
            const FString Name = FString(Section.Name) + TEXT("#") + Id, EntryKey = KeyOf(Entry);
            Keys.Add(Name, EntryKey);
            if (const FString* Held = Known.Find(Name); Held && *Held == EntryKey)
            {
                const auto Reference = MakeShared<FJsonObject>();
                Reference->SetStringField(TEXT("$held"), EntryKey);
                Sent.Add(MakeShared<FJsonValueObject>(Reference));
                Changed = true;
            }
            else
            {
                KeysJson->SetStringField(Name, EntryKey);   // The client keeps it under this key.
                Sent.Add(Entry);
            }
        }
        if (Changed)
            Parent->SetArrayField(Section.Field, Sent);
    }
    Root->SetObjectField(TEXT("sectionKeys"), KeysJson);
    return Keys;
}

// The client's side: the newest few versions it has received of each part, and the entries of the maps by key.
constexpr int32 KeptPerSection = 6, EntriesKept = 4096;
struct FCache
{
    TMap<FString, TArray<TPair<FString, TSharedPtr<FJsonValue>>>> Kept;
    TMap<FString, TSharedPtr<FJsonValue>> Entries;
    TArray<FString> EntryOrder;                        // Oldest first, to let the oldest go.
    void Reset() { Kept.Reset(); Entries.Reset(); EntryOrder.Reset(); }
    void KeepEntry(const FString& Key, const TSharedPtr<FJsonValue>& Value)
    {
        if (Entries.Contains(Key))
            return;
        Entries.Add(Key, Value);
        EntryOrder.Add(Key);
        if (EntryOrder.Num() > EntriesKept)
        {
            Entries.Remove(EntryOrder[0]);
            EntryOrder.RemoveAt(0);
        }
    }
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
        if (auto Value = Parent->TryGetField(Section.Field); Value.IsValid())
        {
            const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
            if (Section.Entries && Value->TryGetArray(List) && List)
            {
                // Entries sent whole are kept by their key; those held are put back.
                TArray<TSharedPtr<FJsonValue>> Whole;
                bool Referenced = false;
                for (const auto& Entry : *List)
                {
                    const TSharedPtr<FJsonObject>* Object = nullptr;
                    FString Held;
                    if (Entry.IsValid() && Entry->TryGetObject(Object) && Object && (*Object)->TryGetStringField(TEXT("$held"), Held))
                    {
                        const auto* Found = Cache.Entries.Find(Held);
                        if (!Found)
                            return false;
                        Whole.Add(*Found);
                        Referenced = true;
                        continue;
                    }
                    FString Id, EntryKey;
                    if (Object && (*Object)->TryGetStringField(TEXT("id"), Id) &&
                        (*Keys)->TryGetStringField(FString(Section.Name) + TEXT("#") + Id, EntryKey))
                        Cache.KeepEntry(EntryKey, Entry);     // Under the server's key for it.
                    Whole.Add(Entry);
                }
                if (Referenced)
                {
                    Value = MakeShared<FJsonValueArray>(Whole);
                    Parent->SetField(Section.Field, Value);
                }
            }
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
