#pragma once

#include "Runtime/RatwJson.h"

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
}
