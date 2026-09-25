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
inline double StrictNumber(const Object& O, const TCHAR* Key, double Default)
{
    const auto* Value = O.IsValid() ? O->Values.Find(Key) : nullptr;
    double Result = Default;
    return Value && Value->IsValid() && (*Value)->Type == EJson::Number && (*Value)->TryGetNumber(Result) &&
                   FMath::IsFinite(Result)
               ? Result
               : Default;
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
inline Object Appearance(const ratw::Appearance& A)
{
    auto O = New();
    Text(O, TEXT("species"), A.species);
    Text(O, TEXT("sex"), A.sex);
    Text(O, TEXT("stature"), A.stature);
    Text(O, TEXT("pattern"), A.pattern);
    O->SetNumberField(TEXT("baseColor"), A.baseColor);
    O->SetNumberField(TEXT("gradientColor"), A.gradientColor);
    O->SetNumberField(TEXT("markingColor"), A.markingColor);
    O->SetNumberField(TEXT("gradientAmount"), A.gradientAmount);
    O->SetNumberField(TEXT("patternAmount"), A.patternAmount);
    return O;
}
inline bool ReadAppearance(const Object& O, ratw::Appearance& Out)
{
    // A provided appearance is complete and strict: no coercion, unknown keys,
    // partial presets, out-of-range clamping, or replacing bad input with defaults.
    if (!O.IsValid() || O->Values.Num() != 9) return false;
    ratw::Appearance A;
    auto ReadId = [&](const TCHAR* Key, std::string& Id) {
        const auto* Value = O->Values.Find(Key);
        FString Parsed;
        if (!Value || !Value->IsValid() || (*Value)->Type != EJson::String || !(*Value)->TryGetString(Parsed))
            return false;
        if (Parsed.Len() == 0 || Parsed.Len() > 16) return false;
        for (const TCHAR Character : Parsed)
            if (Character < TCHAR('a') || Character > TCHAR('z')) return false;
        Id = S(Parsed);
        return true;
    };
    if (!ReadId(TEXT("species"), A.species) || !ReadId(TEXT("sex"), A.sex) ||
        !ReadId(TEXT("stature"), A.stature) || !ReadId(TEXT("pattern"), A.pattern)) return false;
    auto ReadColor = [&](const TCHAR* Key, int& Color) {
        const double Parsed = StrictNumber(O, Key, -1);
        if (Parsed < 0 || Parsed >= ratw::CoatColorCount || Parsed != FMath::FloorToDouble(Parsed)) return false;
        Color = static_cast<int>(Parsed);
        return true;
    };
    if (!ReadColor(TEXT("baseColor"), A.baseColor) || !ReadColor(TEXT("gradientColor"), A.gradientColor) ||
        !ReadColor(TEXT("markingColor"), A.markingColor)) return false;
    A.gradientAmount = StrictNumber(O, TEXT("gradientAmount"), -1);
    A.patternAmount = StrictNumber(O, TEXT("patternAmount"), -1);
    if (!ratw::validAppearance(A)) return false;
    Out = A;
    return true;
}
inline ratw::Appearance ReadAppearance(const Object& O)
{
    ratw::Appearance A;
    if (!ReadAppearance(O, A)) A.species.clear(); // Invalid sentinel rejects the whole save/create transaction.
    return A;
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
    O->SetBoolField(TEXT("turning"), E.turning);
    O->SetBoolField(TEXT("moving"), FMath::Abs(E.velocity.x) + FMath::Abs(E.velocity.y) > 0.001 ||
                                        FMath::Abs(E.input.x) + FMath::Abs(E.input.y) > 0.001 || !E.path.empty());
    O->SetNumberField(TEXT("postureRemaining"), E.postureRemaining);
    Text(O, TEXT("postureTarget"), E.postureTarget);
    O->SetBoolField(TEXT("npc"), E.npc);
    // Entity is called only for self, sight-sanitized visible actors, or private
    // persistence; appearance never travels in anonymous scent/hearing cues.
    O->SetObjectField(TEXT("appearance"), Appearance(E.appearance));
    Text(O, TEXT("lifeStage"), ratw::lifeStageName(ratw::lifeStage(E.age)));
    O->SetNumberField(TEXT("shoulderHeightCm"), ratw::shoulderHeightCm(E.appearance, E.age));
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
    O->SetNumberField(TEXT("age"), E.age);
    O->SetNumberField(TEXT("strength"), E.strength);
    O->SetNumberField(TEXT("wisdom"), E.wisdom);
    O->SetNumberField(TEXT("lastBirthdayDay"), E.lastBirthdayDay);
    O->SetNumberField(TEXT("ageNoticePending"), E.ageNoticePending);
    O->SetNumberField(TEXT("hearing"), E.hearing);
    O->SetNumberField(TEXT("vision"), E.vision);
    O->SetNumberField(TEXT("earHealth"), E.earHealth);
    O->SetNumberField(TEXT("eyeHealth"), E.eyeHealth);
    O->SetNumberField(TEXT("sneakSkill"), E.sneakSkill);
    O->SetNumberField(TEXT("hearingSkill"), E.hearingSkill);
    O->SetNumberField(TEXT("smell"), E.smell);
    O->SetNumberField(TEXT("noseHealth"), E.noseHealth);
    O->SetNumberField(TEXT("scentSkill"), E.scentSkill);
    O->SetNumberField(TEXT("dexterity"), E.dexterity);
    O->SetNumberField(TEXT("stamina"), E.stamina);
    O->SetNumberField(TEXT("pace"), E.pace);
    O->SetBoolField(TEXT("exhausted"), E.exhausted);
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
    // Legacy records omit appearance; malformed provided objects must not turn
    // into the valid default and silently discard the player's choices.
    if (O.IsValid() && O->HasField(TEXT("appearance"))) E.appearance = ReadAppearance(Child(O, TEXT("appearance")));
    E.speakingColor = static_cast<int>(FMath::Clamp(Number(O, TEXT("color")), 0.0, 31.0));
    E.posture = S(String(O, TEXT("posture"), TEXT("standing")));
    E.state = S(String(O, TEXT("state")));
    E.description = S(String(O, TEXT("description")));
    E.activity = S(String(O, TEXT("activity")));
    E.hearing = Number(O, TEXT("hearing"), 1);
    E.vision = Number(O, TEXT("vision"), 1);
    E.earHealth = Number(O, TEXT("earHealth"), 1);
    E.eyeHealth = Number(O, TEXT("eyeHealth"), 1);
    E.sneakSkill = Number(O, TEXT("sneakSkill"), O.IsValid() && O->HasField(TEXT("sneakSkill")) ? -1.0 : 0.0);
    E.hearingSkill = Number(O, TEXT("hearingSkill"), O.IsValid() && O->HasField(TEXT("hearingSkill")) ? -1.0 : 0.0);
    E.smell = Number(O, TEXT("smell"), O.IsValid() && O->HasField(TEXT("smell")) ? -1.0 : 1.0);
    E.noseHealth = Number(O, TEXT("noseHealth"), O.IsValid() && O->HasField(TEXT("noseHealth")) ? -1.0 : 1.0);
    E.scentSkill = Number(O, TEXT("scentSkill"), O.IsValid() && O->HasField(TEXT("scentSkill")) ? -1.0 : 0.0);
    E.dexterity = StrictNumber(O, TEXT("dexterity"), O.IsValid() && O->HasField(TEXT("dexterity")) ? -1.0 : 50.0);
    auto AgingNumber = [&](const TCHAR* Key, double Default) { return StrictNumber(O, Key, O.IsValid() && O->HasField(Key) ? -2.0 : Default); };
    const double Age = AgingNumber(TEXT("age"), 18), Notices = AgingNumber(TEXT("ageNoticePending"), 0);
    E.age = Age >= 0 && Age <= 10000 && Age == FMath::FloorToDouble(Age) ? int(Age) : -1;
    E.ageNoticePending = Notices >= 0 && Notices <= 10000 && Notices == FMath::FloorToDouble(Notices) ? int(Notices) : -1;
    E.strength = AgingNumber(TEXT("strength"), 50);
    E.wisdom = AgingNumber(TEXT("wisdom"), 30);
    E.lastBirthdayDay = AgingNumber(TEXT("lastBirthdayDay"), -1);
    E.stamina = StrictNumber(O, TEXT("stamina"), O.IsValid() && O->HasField(TEXT("stamina")) ? -1.0 : 100.0);
    const double Pace = StrictNumber(O, TEXT("pace"), O.IsValid() && O->HasField(TEXT("pace")) ? -1.0 : 0.0);
    E.pace = Pace >= 0 && Pace <= 10 && Pace == FMath::FloorToDouble(Pace) ? static_cast<int>(Pace) : -1;
    E.exhausted = Bool(O, TEXT("exhausted"));
    bool Exhausted = false;
    if (O.IsValid() && O->HasField(TEXT("exhausted")))
    {
        const auto* Flag = O->Values.Find(TEXT("exhausted"));
        if (!Flag || !Flag->IsValid() || (*Flag)->Type != EJson::Boolean ||
            !O->TryGetBoolField(TEXT("exhausted"), Exhausted))
            E.stamina = -1; // Malformed fatigue state rejects the complete save.
    }
    E.postureTarget = S(String(O, TEXT("postureTarget")));
    E.postureRemaining = Number(O, TEXT("postureRemaining"));
    // Input, paths and manual turn intents are deliberately never reloaded.
    E.turnTarget = E.facing;
    return E;
}
inline ratw::Result PaceCommand(ratw::World& World, const std::string& Id, const Object& O)
{
    const double Pace = StrictNumber(O, TEXT("pace"), -1);
    if (Pace < 0 || Pace > 10 || Pace != FMath::FloorToDouble(Pace))
        return {false, "Pace must be a whole step from 0 to 10.", {}};
    return World.setPace(Id, static_cast<int>(Pace));
}
inline void PrivatePace(const Object& O, const ratw::Entity& E)
{
    O->SetNumberField(TEXT("age"), E.age);
    O->SetNumberField(TEXT("strength"), E.strength);
    O->SetNumberField(TEXT("wisdom"), E.wisdom);
    O->SetNumberField(TEXT("effectiveDexterity"), ratw::effectiveDexterity(E));
    O->SetNumberField(TEXT("dexterity"), E.dexterity);
    O->SetNumberField(TEXT("stamina"), E.stamina);
    O->SetNumberField(TEXT("pace"), E.pace);
    O->SetNumberField(TEXT("effectivePace"), ratw::effectivePace(E));
    O->SetStringField(TEXT("paceName"), UTF8_TO_TCHAR(ratw::paceName(ratw::effectivePace(E))));
    O->SetNumberField(TEXT("staminaRate"), E.staminaRate);
    O->SetBoolField(TEXT("exhausted"), E.exhausted);
    auto Sprint = E;
    Sprint.pace = 10;
    Sprint.exhausted = false;
    Sprint.posture = "standing";
    O->SetNumberField(TEXT("topSpeed"), ratw::paceSpeed(Sprint));
    O->SetNumberField(TEXT("currentSpeed"), FMath::Sqrt(E.velocity.x * E.velocity.x + E.velocity.y * E.velocity.y));
}
inline Object MapCell(const ratw::MapCell& M, bool IncludeGlyphs = true)
{
    auto O = New();
    Text(O, TEXT("id"), M.id);
    Text(O, TEXT("name"), M.name);
    O->SetNumberField(TEXT("width"), M.width);
    O->SetNumberField(TEXT("height"), M.height);
    O->SetNumberField(TEXT("x"), M.worldX);
    O->SetNumberField(TEXT("y"), M.worldY);
    O->SetNumberField(TEXT("z"), M.worldZ);
    O->SetStringField(TEXT("knowledge"), UTF8_TO_TCHAR(ratw::knowledgeName(M.knowledge)));
    O->SetBoolField(TEXT("current"), M.current);
    O->SetBoolField(TEXT("visible"), M.visible);
    if (IncludeGlyphs)
        Text(O, TEXT("glyphs"), std::string(M.rememberedGlyphs.begin(), M.rememberedGlyphs.end()));
    return O;
}
inline Object Travel(const ratw::TravelState& T)
{
    auto O = New();
    O->SetBoolField(TEXT("active"), T.active);
    O->SetBoolField(TEXT("paused"), T.paused);
    Text(O, TEXT("destination"), T.destination);
    Text(O, TEXT("status"), T.status);
    Text(O, TEXT("nextDoor"), T.nextDoor);
    Array Route;
    for (const auto& Id : T.route)
        Route.Add(V(F(Id)));
    O->SetArrayField(TEXT("route"), Route);
    return O;
}
inline Object Wind(const ratw::Wind& W)
{
    auto O = New();
    O->SetNumberField(TEXT("direction"), W.direction);
    O->SetNumberField(TEXT("strength"), W.strength);
    O->SetBoolField(TEXT("variable"), W.variable);
    return O;
}
inline Object Environment(const ratw::Environment& E)
{
    auto O = New();
    auto Date = New();
    Date->SetNumberField(TEXT("absoluteDays"), E.date.absoluteDays);
    Date->SetNumberField(TEXT("year"), E.date.year);
    Date->SetNumberField(TEXT("dayOfYear"), E.date.dayOfYear);
    Date->SetNumberField(TEXT("dayOfSeason"), E.date.dayOfSeason);
    Text(Date, TEXT("season"), ratw::calendar::seasonName(E.date.season));
    Text(Date, TEXT("moonName"), E.date.moonName);
    Date->SetNumberField(TEXT("moonPhase"), E.date.moonPhase);
    Date->SetNumberField(TEXT("moonIllumination"), E.date.moonIllumination);
    O->SetObjectField(TEXT("calendar"), Date);
    O->SetNumberField(TEXT("hour"), E.hour);
    Text(O, TEXT("phase"), E.phase);
    O->SetNumberField(TEXT("daylight"), E.daylight);
    O->SetNumberField(TEXT("illumination"), E.illumination);
    O->SetNumberField(TEXT("sight"), E.sight);
    O->SetNumberField(TEXT("hearing"), E.hearing);
    O->SetNumberField(TEXT("scent"), E.scent);
    O->SetNumberField(TEXT("movement"), E.movement);
    O->SetNumberField(TEXT("artificialLight"), E.artificialLight);
    O->SetNumberField(TEXT("daylightAccess"), E.daylightAccess);
    O->SetNumberField(TEXT("glowStrength"), E.glowStrength);
    Text(O, TEXT("lightingTone"), E.lightingTone);
    Text(O, TEXT("lightSource"), E.lightSource);
    return O;
}
inline Object Lighting(const ratw::Lighting& L)
{
    auto O = New();
    O->SetNumberField(TEXT("artificial"), L.artificial);
    O->SetNumberField(TEXT("daylightAccess"), L.daylightAccess);
    Text(O, TEXT("tone"), L.tone);
    return O;
}
inline ratw::Lighting ReadLighting(const Object& O)
{
    ratw::Lighting L;
    L.artificial = StrictNumber(O, TEXT("artificial"), -1);
    L.daylightAccess = StrictNumber(O, TEXT("daylightAccess"), -1);
    L.tone = S(String(O, TEXT("tone")));
    if (!O.IsValid() || O->Values.Num() != 3)
        L.artificial = -1;
    return L; // Core validation rejects the entire record if malformed.
}
inline FString EnvironmentDescription(const ratw::Cell& Cell, const ratw::Environment& E)
{
    const FString Time = F(E.phase);
    if (!Cell.outdoors)
    {
        const FString Light = E.lightSource == "dark" ? TEXT("The room is unlit and dark; ears and nose remain useful.")
                              : E.illumination < .6   ? TEXT("The room is dimly lit, limiting distant vision.")
                              : E.glowStrength > .1 && E.lightingTone == "warm"
                                  ? TEXT("Warm artificial light keeps the room clearly visible.")
                              : E.lightSource == "daylight" || E.glowStrength < .1
                                  ? TEXT("Daylight keeps the room clearly visible.")
                                  : TEXT("Artificial light keeps the room clearly visible.");
        return TEXT("It is ") + Time + TEXT(". ") + Light + TEXT(" This room is sheltered from outdoor weather.");
    }
    FString Conditions;
    switch (Cell.weather)
    {
    case ratw::Weather::Rain:
        Conditions =
            TEXT("Rain blurs the distance, masks quieter sounds, scatters airborne scent, and slows the footing.");
        break;
    case ratw::Weather::Snow:
        Conditions = TEXT("Snow veils the distance, muffles sound, weakens airborne scent, and makes travel slower.");
        break;
    case ratw::Weather::Fog:
        Conditions = TEXT("Fog conceals the distance. Ears and nose can still find what the eyes cannot.");
        break;
    default:
        Conditions = E.phase == "night" ? TEXT("The sky is clear, but darkness conceals distant movement.")
                                        : TEXT("The sky is clear; the changing light shapes what you can see.");
        break;
    }
    return TEXT("It is ") + Time + TEXT(". ") + Conditions;
}
inline double ReadClockOffset(const Object& O)
{
    // Missing means a legacy noon start; malformed must reject the whole save.
    return O.IsValid() && O->HasField(TEXT("clockOffsetHours")) ? StrictNumber(O, TEXT("clockOffsetHours"), -1) : 12.0;
}
inline ratw::Weather ReadWeather(const TSharedPtr<FJsonValue>& Value)
{
    double NumberValue = -1;
    if (!Value.IsValid() || Value->Type != EJson::Number || !Value->TryGetNumber(NumberValue) ||
        !FMath::IsFinite(NumberValue) || NumberValue < 0 || NumberValue > 3 ||
        FMath::FloorToDouble(NumberValue) != NumberValue)
        return static_cast<ratw::Weather>(-1);
    return static_cast<ratw::Weather>(static_cast<int>(NumberValue));
}
inline ratw::Result EnvironmentCommand(ratw::World& World, const std::string& CellId, const FString& Type,
                                       const FString& Value, bool DevTools)
{
    if (!DevTools)
        return {false, "Environment controls are available only in development sessions.", CellId};
    if (Type == TEXT("calendar"))
        return Value == TEXT("day") ? World.advanceCalendar(1) : Value == TEXT("year") ? World.advanceCalendar(365)
                : ratw::Result{false, "Unknown calendar step.", CellId};
    if (Type == TEXT("weather") && Value == TEXT("seasonal")) return World.useSeasonalWeather(CellId);
    if (Type == TEXT("lighting"))
    {
        if (Value != TEXT("warm") && Value != TEXT("unlit") && Value != TEXT("daylit") && Value != TEXT("cool"))
            return {false, "Unknown lighting preset.", CellId};
        return World.setLighting(CellId, Value == TEXT("unlit") || Value == TEXT("daylit") ? 0.0 : 1.0,
                                 Value == TEXT("unlit") ? 0.0 : 1.0, Value == TEXT("cool") ? "cool" : "warm");
    }
    if (Type == TEXT("time"))
    {
        if (Value != TEXT("dawn") && Value != TEXT("day") && Value != TEXT("dusk") && Value != TEXT("night"))
            return {false, "Unknown time-of-day preset.", CellId};
        return World.setTimeOfDay(Value == TEXT("dawn")   ? 6.0
                                  : Value == TEXT("day")  ? 12.0
                                  : Value == TEXT("dusk") ? 18.0
                                                          : 0.0);
    }
    if (Type != TEXT("weather") || !World.cell(CellId) ||
        (Value != TEXT("clear") && Value != TEXT("rain") && Value != TEXT("snow") && Value != TEXT("fog")))
        return {false, "Unknown weather preset or cell.", CellId};
    World.setWeather(CellId, Value == TEXT("rain")   ? ratw::Weather::Rain
                             : Value == TEXT("snow") ? ratw::Weather::Snow
                             : Value == TEXT("fog")  ? ratw::Weather::Fog
                                                     : ratw::Weather::Clear);
    return {true, "Weather updated.", CellId};
}
inline ratw::Wind ReadWind(const Object& O)
{
    ratw::Wind W;
    W.direction = Number(O, TEXT("direction"));
    W.strength = Number(O, TEXT("strength"), -1);
    W.variable = Bool(O, TEXT("variable"));
    double Direction = 0;
    bool Variable = false;
    if (!O.IsValid() || !O->TryGetNumberField(TEXT("direction"), Direction) || !FMath::IsFinite(Direction) ||
        !O->TryGetBoolField(TEXT("variable"), Variable))
        W.strength = -1; // Core restore rejects the entire invalid record.
    return W;
}
inline Object Senses(const ratw::Snapshot& Snapshot)
{
    auto O = New();
    Array Cues;
    for (const auto& Cue : Snapshot.scentCues)
    {
        auto J = New();
        J->SetNumberField(TEXT("sector"), Cue.sector);
        J->SetNumberField(TEXT("strength"), Cue.strength);
        J->SetBoolField(TEXT("windborne"), Cue.windborne);
        Cues.Add(V(J));
    }
    O->SetArrayField(TEXT("scentCues"), Cues);
    O->SetBoolField(TEXT("movementHeard"), Snapshot.movementHeard);
    return O;
}
} // namespace ratwjson
