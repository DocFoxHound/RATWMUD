#pragma once

#include "Runtime/RatwJson.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace ratwmotion
{
inline ratwjson::Object Frame(const ratw::World& World, const std::string& Observer)
{
    using namespace ratwjson;
    auto Root = New();
    const auto* Self = World.entity(Observer);
    if (!Self) return Root;
    Text(Root, TEXT("observer"), Observer);
    Text(Root, TEXT("cellId"), Self->cellId);
    Root->SetNumberField(TEXT("time"), World.time());
    Array Poses;
    for (const auto& Pair : World.entities())
    {
        const auto& E = Pair.second;
        const bool IsSelf = Pair.first == Observer;
        if (!IsSelf && (E.cellId != Self->cellId || World.visionClarity(Observer, Pair.first) <= 0)) continue;
        auto Pose = New();
        Text(Pose, TEXT("id"), E.id);
        Pose->SetNumberField(TEXT("x"), E.position.x);
        Pose->SetNumberField(TEXT("y"), E.position.y);
        Pose->SetNumberField(TEXT("facing"), E.facing);
        Pose->SetBoolField(TEXT("moving"), std::abs(E.velocity.x) + std::abs(E.velocity.y) > .001 ||
            E.postureRemaining > 0 || (IsSelf && (!E.path.empty() || std::abs(E.input.x) + std::abs(E.input.y) > .001)));
        Poses.Add(V(Pose));
    }
    Root->SetArrayField(TEXT("entities"), Poses);
    return Root;
}

// Motion frames go twenty times a second, so on the wire they are binary (Docs/Design/26-living-npcs.md, Phase 6):
// the frame's stamps, then each pose as its ID, x, y and facing (single precision: a hundred-thousandth of a tile) and
// whether it is moving. Pack and Unpack turn a frame (as Frame and the host's stamps make it) into bytes and back.
constexpr uint32 Magic = 0x31544d52;               // "RMT1"
constexpr int32 MaxPoses = 4096;

inline TArray<uint8> Pack(const ratwjson::Object& Frame)
{
    TArray<uint8> Bytes;
    FMemoryWriter Out(Bytes);
    uint32 Tag = Magic;
    FString Session = Frame->GetStringField(TEXT("motionSession")), Observer = Frame->GetStringField(TEXT("observer")),
            Cell = Frame->GetStringField(TEXT("cellId"));
    int32 Generation = static_cast<int32>(Frame->GetNumberField(TEXT("cellGeneration")));
    double Revision = Frame->GetNumberField(TEXT("revision")), Time = Frame->GetNumberField(TEXT("time"));
    Out << Tag << Session << Observer << Cell << Generation << Revision << Time;
    const TArray<TSharedPtr<FJsonValue>>* Poses = nullptr;
    int32 Count = Frame->TryGetArrayField(TEXT("entities"), Poses) && Poses ? FMath::Min(Poses->Num(), MaxPoses) : 0;
    Out << Count;
    for (int32 I = 0; I < Count; ++I)
    {
        const auto Pose = (*Poses)[I]->AsObject();
        FString Id = Pose->GetStringField(TEXT("id"));
        float X = static_cast<float>(Pose->GetNumberField(TEXT("x"))), Y = static_cast<float>(Pose->GetNumberField(TEXT("y"))),
              Facing = static_cast<float>(Pose->GetNumberField(TEXT("facing")));
        uint8 Moving = Pose->GetBoolField(TEXT("moving")) ? 1 : 0;
        Out << Id << X << Y << Facing << Moving;
    }
    return Bytes;
}

// Null for anything that isn't a whole, well-formed frame.
inline ratwjson::Object Unpack(const TArray<uint8>& Bytes)
{
    using namespace ratwjson;
    FMemoryReader In(Bytes);
    In.ArMaxSerializeSize = 4096;                   // No string longer than this, whatever a length claims.
    uint32 Tag = 0;
    FString Session, Observer, Cell;
    int32 Generation = 0, Count = 0;
    double Revision = 0, Time = 0;
    In << Tag;
    if (Tag != Magic) return nullptr;
    In << Session << Observer << Cell << Generation << Revision << Time << Count;
    if (In.IsError() || Count < 0 || Count > MaxPoses) return nullptr;
    auto Root = New();
    Root->SetStringField(TEXT("motionSession"), Session);
    Root->SetStringField(TEXT("observer"), Observer);
    Root->SetStringField(TEXT("cellId"), Cell);
    Root->SetNumberField(TEXT("cellGeneration"), Generation);
    Root->SetNumberField(TEXT("revision"), Revision);
    Root->SetNumberField(TEXT("time"), Time);
    Array Poses;
    for (int32 I = 0; I < Count; ++I)
    {
        FString Id;
        float X = 0, Y = 0, Facing = 0;
        uint8 Moving = 0;
        In << Id << X << Y << Facing << Moving;
        if (In.IsError()) return nullptr;
        auto Pose = New();
        Pose->SetStringField(TEXT("id"), Id);
        Pose->SetNumberField(TEXT("x"), X);
        Pose->SetNumberField(TEXT("y"), Y);
        Pose->SetNumberField(TEXT("facing"), Facing);
        Pose->SetBoolField(TEXT("moving"), Moving != 0);
        Poses.Add(V(Pose));
    }
    if (In.IsError() || !In.AtEnd()) return nullptr;
    Root->SetArrayField(TEXT("entities"), Poses);
    return Root;
}
}
