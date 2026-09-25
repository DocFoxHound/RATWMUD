#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Core/RatwAppearance.h"
#include "Core/RatwWorld.h"
#include "Runtime/RatwJson.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwAppearanceWireTest, "RATW.Appearance.StrictWire",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwAppearanceWireTest::RunTest(const FString&)
{
    using namespace ratwjson;
    ratw::Appearance Custom;
    Custom.species = "maned"; Custom.sex = "female"; Custom.stature = "tall"; Custom.pattern = "piebald";
    Custom.baseColor = 6; Custom.gradientColor = 7; Custom.markingColor = 0;
    Custom.gradientAmount = .75; Custom.patternAmount = .25;
    const FString Canonical = Encode(Appearance(Custom));
    auto Fresh = [&]() { return Decode(Canonical); };
    ratw::Appearance Parsed;
    TestTrue(TEXT("Complete appearance roundtrips through actual JSON"), ReadAppearance(Fresh(), Parsed));
    TestEqual(TEXT("Canonical nine fields roundtrip exactly"), Encode(Appearance(Parsed)), Canonical);
    TestEqual(TEXT("Appearance schema has exactly nine fields"), Fresh()->Values.Num(), 9);
    auto Reject = [&](const Object& Invalid, const FString& Reason) {
        auto Unchanged = Custom;
        TestFalse(*Reason, ReadAppearance(Invalid, Unchanged));
        TestEqual(*(Reason + TEXT(" leaves output unchanged")), Encode(Appearance(Unchanged)), Canonical);
        TestFalse(*(Reason + TEXT(" return overload is invalid")), ratw::validAppearance(ReadAppearance(Invalid)));
    };
    Reject(Object(), TEXT("Missing/null object rejected by strict parser"));
    Reject(New(), TEXT("Empty object cannot become an implicit preset"));
    for (const TCHAR* Key : {TEXT("species"), TEXT("sex"), TEXT("stature"), TEXT("pattern"), TEXT("baseColor"),
                            TEXT("gradientColor"), TEXT("markingColor"), TEXT("gradientAmount"), TEXT("patternAmount")})
    {
        auto Invalid = Fresh(); Invalid->RemoveField(Key);
        Reject(Invalid, FString(Key) + TEXT(" is required when appearance is provided"));
        Invalid = Fresh(); Invalid->SetField(Key, MakeShared<FJsonValueNull>());
        Reject(Invalid, FString(Key) + TEXT(" cannot be null"));
    }
    auto Extra = Fresh(); Extra->SetNumberField(TEXT("power"), 100);
    Reject(Extra, TEXT("Unknown appearance field rejected"));
    Extra = Fresh(); Extra->RemoveField(TEXT("pattern")); Extra->SetNumberField(TEXT("power"), 100);
    Reject(Extra, TEXT("Nine-key object with substituted unknown field rejected"));
    for (const TCHAR* Key : {TEXT("species"), TEXT("sex"), TEXT("stature"), TEXT("pattern")})
    {
        for (const TCHAR* Bad : {TEXT(""), TEXT("Unknown"), TEXT("timber "), TEXT("<script>")})
        {
            auto Invalid = Fresh(); Invalid->SetStringField(Key, Bad);
            Reject(Invalid, FString(Key) + TEXT(" unsupported enum is rejected"));
        }
        auto Invalid = Fresh(); Invalid->SetNumberField(Key, 1);
        Reject(Invalid, FString(Key) + TEXT(" numeric enum is not coerced"));
        Invalid = Fresh(); Invalid->SetBoolField(Key, true);
        Reject(Invalid, FString(Key) + TEXT(" boolean enum is not coerced"));
    }
    for (const TCHAR* Key : {TEXT("baseColor"), TEXT("gradientColor"), TEXT("markingColor")})
    {
        for (double Bad : {-1., 8., .5, 1.e100, std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::infinity()})
        {
            auto Invalid = Fresh(); Invalid->SetNumberField(Key, Bad);
            Reject(Invalid, FString(Key) + TEXT(" palette value must be finite whole 0..7"));
        }
        auto Invalid = Fresh(); Invalid->SetStringField(Key, TEXT("3"));
        Reject(Invalid, FString(Key) + TEXT(" numeric string is not coerced"));
        Invalid = Fresh(); Invalid->SetBoolField(Key, false);
        Reject(Invalid, FString(Key) + TEXT(" boolean is not coerced to palette zero"));
    }
    for (const TCHAR* Key : {TEXT("gradientAmount"), TEXT("patternAmount")})
    {
        for (double Bad : {-.001, 1.001, std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
        {
            auto Invalid = Fresh(); Invalid->SetNumberField(Key, Bad);
            Reject(Invalid, FString(Key) + TEXT(" blend must be finite normalized value"));
        }
        auto Invalid = Fresh(); Invalid->SetStringField(Key, TEXT("0.5"));
        Reject(Invalid, FString(Key) + TEXT(" numeric string blend is not coerced"));
        Invalid = Fresh(); Invalid->SetBoolField(Key, true);
        Reject(Invalid, FString(Key) + TEXT(" boolean blend is not coerced"));
        for (double Endpoint : {0., 1.})
        {
            auto Valid = Fresh(); Valid->SetNumberField(Key, Endpoint);
            TestTrue(TEXT("Blend normalized endpoints are inclusive"), ReadAppearance(Valid, Parsed));
        }
    }
    FString EmbeddedNull = TEXT("timber");
    EmbeddedNull.AppendChar(TCHAR(0));
    EmbeddedNull.Append(TEXT("concealed"));
    auto NullEnum = Fresh(); NullEnum->SetStringField(TEXT("species"), EmbeddedNull);
    Reject(NullEnum, TEXT("Embedded NUL cannot truncate into a valid species"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwAppearancePersistenceTest, "RATW.Appearance.LegacyAtomicPersistence",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwAppearancePersistenceTest::RunTest(const FString&)
{
    using namespace ratwjson;
    ratw::World World;
    auto& Wolf = World.addPlayer("player-portrait", "Juniper");
    Wolf.appearance.species = "arctic"; Wolf.appearance.sex = "female";
    Wolf.appearance.baseColor = 0; Wolf.appearance.markingColor = 2;
    Wolf.age = 64;
    const auto Saved = World.save();
    const FString Json = Encode(PersistEntity(Wolf, World.time()));
    auto RoundTrip = Saved; RoundTrip.players[0] = ReadEntity(Decode(Json));
    ratw::World Restored;
    TestTrue(TEXT("Appearance-bearing JSON checkpoint restores"), Restored.restore(RoundTrip).ok);
    TestEqual(TEXT("Player portrait survives serialized restart"), Encode(Appearance(Restored.entity(Wolf.id)->appearance)),
              Encode(Appearance(Wolf.appearance)));
    auto Legacy = Decode(Json); Legacy->RemoveField(TEXT("appearance"));
    auto LegacySave = Saved; LegacySave.players[0] = ReadEntity(Legacy);
    TestTrue(TEXT("Legacy omission restores a valid canonical appearance"), Restored.restore(LegacySave).ok);
    TestEqual(TEXT("Legacy appearance equals default, not last in-memory portrait"),
              Encode(Appearance(Restored.entity(Wolf.id)->appearance)), Encode(Appearance(ratw::Appearance{})));
    TestTrue(TEXT("Restore customized fixture again"), Restored.restore(RoundTrip).ok);
    for (int Kind = 0; Kind < 4; ++Kind)
    {
        auto Invalid = Decode(Json);
        if (Kind == 0) Invalid->SetField(TEXT("appearance"), MakeShared<FJsonValueNull>());
        if (Kind == 1) Invalid->SetStringField(TEXT("appearance"), TEXT("arctic"));
        if (Kind == 2) Invalid->SetObjectField(TEXT("appearance"), New());
        if (Kind == 3) Child(Invalid, TEXT("appearance"))->SetStringField(TEXT("baseColor"), TEXT("0"));
        auto Damaged = Saved; Damaged.players[0] = ReadEntity(Invalid); Damaged.calendarDays += 1;
        TestFalse(TEXT("Provided malformed appearance rejects whole saved world"), Restored.restore(Damaged).ok);
        TestEqual(TEXT("Rejected appearance cannot advance the clock"), Restored.calendarDays(), Saved.calendarDays);
        TestEqual(TEXT("Rejected appearance cannot replace an existing portrait"),
                  Encode(Appearance(Restored.entity(Wolf.id)->appearance)), Encode(Appearance(Wolf.appearance)));
    }
    TestTrue(TEXT("Birthday advances authoritative age"), Restored.advanceCalendar(365).ok);
    const auto Elder = Entity(*Restored.entity(Wolf.id), Restored.time());
    TestEqual(TEXT("Public stage derives from authoritative birthday"), String(Elder, TEXT("lifeStage")), FString(TEXT("old")));
    TestFalse(TEXT("Visible entity does not disclose exact age"), Elder->HasField(TEXT("age")));
    TestFalse(TEXT("Visible entity does not disclose strength"), Elder->HasField(TEXT("strength")));
    PrivatePace(Elder, *Restored.entity(Wolf.id));
    TestEqual(TEXT("Self-only projection provides exact age"), Number(Elder, TEXT("age")), 65.);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwAppearancePrivacyTest, "RATW.Appearance.SightFilteredPortraits",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwAppearancePrivacyTest::RunTest(const FString&)
{
    using namespace ratwjson;
    ratw::World World;
    auto& Observer = World.addPlayer("observer", "Observer");
    auto& Hidden = World.addPlayer("concealed", "Concealed");
    Observer.position = {22.5, 6.5}; Hidden.position = {26.5, 6.5};
    Hidden.appearance.species = "ethiopian"; Hidden.appearance.pattern = "piebald";
    TestEqual(TEXT("Closed pantry blocks portrait visibility"), World.visionClarity(Observer.id, Hidden.id), 0.);
    TestTrue(TEXT("Hidden wolf can still be heard"), World.hearingClarity(Observer.id, Hidden.id, ratw::Voice::Yell) > 0);
    auto VisibleJson = New(); Array Actors;
    for (const auto& E : World.snapshot(Observer.id).entities) Actors.Add(V(Entity(E, World.time())));
    VisibleJson->SetArrayField(TEXT("entities"), Actors);
    TestFalse(TEXT("Snapshot excludes unseen identity even with distinctive coat"), Encode(VisibleJson).Contains(TEXT("ethiopian")));
    TestFalse(TEXT("Guessing hidden actor ID cannot inspect its portrait"), World.interact(Observer.id, Hidden.id, "inspect").ok);
    Hidden.position = {21.5, 6.5};
    bool Found = false;
    for (const auto& E : World.snapshot(Observer.id).entities)
        if (E.id == Hidden.id)
        {
            Found = true;
            const auto Public = Entity(E, World.time());
            TestEqual(TEXT("Visible portrait is the authoritative appearance"), String(Child(Public, TEXT("appearance")), TEXT("species")),
                      FString(TEXT("ethiopian")));
            TestFalse(TEXT("Visible portrait does not include private stamina"), Public->HasField(TEXT("stamina")));
        }
    TestTrue(TEXT("Unobstructed adjacent wolf becomes inspectable"), Found);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwAppearanceAssetsTest, "RATW.Appearance.SharedPortraitAssets",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwAppearanceAssetsTest::RunTest(const FString&)
{
    // Inspect the same shipped sources used by the shared Slate doll, without
    // requiring a rendering device in headless Automation. Root visual tests
    // separately exercise age frames and recoloring inside the actual widget.
    auto& Module = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    for (const TCHAR* Species : {TEXT("timber"), TEXT("maned"), TEXT("arctic"), TEXT("red"), TEXT("ethiopian")})
    {
        const FString Label(Species);
        const FString Path = FPaths::ProjectDir() / TEXT("Data/Portraits") / (Label + TEXT(".png"));
        TArray<uint8> Png;
        if (!TestTrue(*(Label + TEXT(" portrait atlas is shipped")), FFileHelper::LoadFileToArray(Png, *Path))) continue;
        if (!TestTrue(*(Label + TEXT(" atlas has bounded compressed size")), Png.Num() > 0 && Png.Num() <= 16 * 1024 * 1024)) continue;
        const auto Decoder = Module.CreateImageWrapper(EImageFormat::PNG);
        if (!TestTrue(*(Label + TEXT(" atlas decodes as PNG")), Decoder.IsValid() && Decoder->SetCompressed(Png.GetData(), Png.Num()))) continue;
        if (!TestTrue(*(Label + TEXT(" atlas uses the shared RGBA8 contract")),
                      (Decoder->GetFormat() == ERGBFormat::RGBA || Decoder->GetFormat() == ERGBFormat::BGRA) &&
                      Decoder->GetBitDepth() == 8)) continue;
        const int32 Width = Decoder->GetWidth(), Height = Decoder->GetHeight();
        if (!TestTrue(*(Label + TEXT(" atlas partitions into four equal bounded frames")),
                      Width >= 4 && Height >= 4 && Width <= 4096 && Height <= 4096 && Width % 2 == 0 && Height % 2 == 0)) continue;
        TArray64<uint8> Pixels;
        if (!TestTrue(*(Label + TEXT(" atlas provides complete RGBA pixel data")),
                      Decoder->GetRaw(Pixels) && Pixels.Num() == int64(Width) * Height * 4)) continue;
        for (int32 Frame = 0; Frame < 4; ++Frame)
        {
            bool HasWolf = false, HasTransparentBackground = false;
            const int32 Left = (Frame % 2) * (Width / 2), Top = (Frame / 2) * (Height / 2);
            for (int32 Y = Top; Y < Top + Height / 2; ++Y)
                for (int32 X = Left; X < Left + Width / 2; ++X)
                {
                    const uint8 Alpha = Pixels[(int64(Y) * Width + X) * 4 + 3];
                    HasWolf |= Alpha > 0;
                    HasTransparentBackground |= Alpha == 0;
                }
            TestTrue(*(Label + FString::Printf(TEXT(" frame %d contains visible portrait pixels"), Frame)), HasWolf);
            TestTrue(*(Label + FString::Printf(TEXT(" frame %d preserves transparent background"), Frame)), HasTransparentBackground);
        }
    }
    return true;
}
#endif
