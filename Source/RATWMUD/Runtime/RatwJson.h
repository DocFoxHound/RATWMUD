#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Core/RatwWorld.h"

namespace ratwjson
{
using Object = TSharedPtr<FJsonObject>;
using Array = TArray<TSharedPtr<FJsonValue>>;
inline FString F(const std::string& Value)
{
    return FString(UTF8_TO_TCHAR(Value.c_str()));
}
inline std::string S(const FString& Value)
{
    return std::string(TCHAR_TO_UTF8(*Value));
}
inline std::string S(const UE::TSharedString<TCHAR>& Value)
{
    return S(FString(Value.ToView()));
}
inline Object New()
{
    return MakeShared<FJsonObject>();
}
inline FString String(const Object& O, const TCHAR* Key, const FString& Default = FString())
{
    FString V;
    return O.IsValid() && O->TryGetStringField(Key, V) ? V : Default;
}
inline double Number(const Object& O, const TCHAR* Key, double Default = 0)
{
    double V;
    return O.IsValid() && O->TryGetNumberField(Key, V) && FMath::IsFinite(V) ? V : Default;
}
inline bool Bool(const Object& O, const TCHAR* Key, bool Default = false)
{
    bool V;
    return O.IsValid() && O->TryGetBoolField(Key, V) ? V : Default;
}
inline Array Items(const Object& O, const TCHAR* Key)
{
    const Array* V;
    return O.IsValid() && O->TryGetArrayField(Key, V) ? *V : Array();
}
inline Object Child(const Object& O, const TCHAR* Key)
{
    const Object* V;
    return O.IsValid() && O->TryGetObjectField(Key, V) ? *V : Object();
}
inline TSharedPtr<FJsonValue> V(const Object& O)
{
    return MakeShared<FJsonValueObject>(O);
}
inline TSharedPtr<FJsonValue> V(const FString& Value)
{
    return MakeShared<FJsonValueString>(Value);
}
inline FString Encode(const Object& O)
{
    FString Json;
    FJsonSerializer::Serialize(O.ToSharedRef(),
                               TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json));
    return Json;
}
inline Object Decode(const FString& Json)
{
    Object O;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), O);
    return O;
}
inline void Text(const Object& O, const TCHAR* Key, const std::string& Value)
{
    O->SetStringField(Key, F(Value));
}
inline Object Entity(const ratw::Entity& E, double Time)
{
    auto O = New();
    Text(O, TEXT("id"), E.id);
    Text(O, TEXT("name"), E.name);
    Text(O, TEXT("cell"), E.cellId);
    O->SetNumberField(TEXT("x"), E.position.x);
    O->SetNumberField(TEXT("y"), E.position.y);
    O->SetNumberField(TEXT("facing"), E.facing);
    O->SetBoolField(TEXT("npc"), E.npc);
    O->SetNumberField(TEXT("color"), E.speakingColor);
    Text(O, TEXT("posture"), E.posture);
    Text(O, TEXT("state"), E.state);
    Text(O, TEXT("description"), E.description);
    Text(O, TEXT("activity"), E.activity);
    O->SetBoolField(TEXT("typing"), E.typing);
    O->SetBoolField(TEXT("speaking"), E.speakingUntil > Time);
    O->SetNumberField(TEXT("speakingRemaining"), FMath::Max(0.0, E.speakingUntil - Time));
    O->SetBoolField(TEXT("transitioned"), E.transitioned);
    return O;
}
inline Object PersistEntity(const ratw::Entity& E, double Time)
{
    auto O = Entity(E, Time);
    O->SetNumberField(TEXT("hearing"), E.hearing);
    O->SetNumberField(TEXT("vision"), E.vision);
    O->SetNumberField(TEXT("earHealth"), E.earHealth);
    O->SetNumberField(TEXT("eyeHealth"), E.eyeHealth);
    return O;
}
inline ratw::Entity ReadEntity(const Object& O)
{
    ratw::Entity E;
    E.id = S(String(O, TEXT("id")));
    E.name = S(String(O, TEXT("name")));
    E.cellId = S(String(O, TEXT("cell")));
    E.position = {Number(O, TEXT("x")), Number(O, TEXT("y"))};
    E.facing = Number(O, TEXT("facing"));
    E.npc = Bool(O, TEXT("npc"));
    E.speakingColor = static_cast<int>(FMath::Clamp(Number(O, TEXT("color")), 0.0, 31.0));
    E.posture = S(String(O, TEXT("posture"), TEXT("standing")));
    E.state = S(String(O, TEXT("state")));
    E.description = S(String(O, TEXT("description")));
    E.activity = S(String(O, TEXT("activity")));
    E.hearing = Number(O, TEXT("hearing"), 1);
    E.vision = Number(O, TEXT("vision"), 1);
    E.earHealth = Number(O, TEXT("earHealth"), 1);
    E.eyeHealth = Number(O, TEXT("eyeHealth"), 1);
    return E;
}
} // namespace ratwjson
