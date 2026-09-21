#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Core/RatwWorld.h"
#include "Runtime/RatwSocialCore.h"
#include "Runtime/RatwPersistence.h"
#include "Runtime/RatwSnapshotCodec.h"
#include "HAL/FileManager.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwStorageTest, "RATW.Persistence.SQLiteRestart",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwStorageTest::RunTest(const FString&)
{
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("Tests") / FGuid::NewGuid().ToString();
    const FString Path = Directory / TEXT("world.sqlite");
    const FString First = TEXT("{\"memory\":\"I promised to return ··· after the rain.\",\"revision\":1}");
    {
        FRatwPersistence Database;
        TestTrue(TEXT("Open isolated SQLite store"), Database.Open(Path));
        TestTrue(TEXT("Commit unicode payload"), Database.Save(First, 1));
        TestEqual(TEXT("Read committed payload"), Database.Load(), First);
    }
    {
        FRatwPersistence Database;
        TestTrue(TEXT("Reopen after simulated process lifetime"), Database.Open(Path));
        TestEqual(TEXT("Record survives reopen exactly"), Database.Load(), First);
        const FString Second = TEXT("{\"revision\":2,\"summary\":\"Still remembered\"}");
        TestTrue(TEXT("Replace atomically"), Database.Save(Second, 2));
        TestEqual(TEXT("Read latest revision"), Database.Load(), Second);
        TestTrue(TEXT("Prepare empty corrupt record fixture"), Database.Save(TEXT(""), 3));
        TestEqual(TEXT("Existing empty record is not mistaken for a new world"), Database.Load(),
                  FString(TEXT("{\"schema\":-1}")));
        TestTrue(TEXT("Restore payload for unsupported-version fixture"), Database.Save(First, 4));
    }
    {
        FSQLiteDatabase Fixture;
        TestTrue(TEXT("Open version fixture"), Fixture.Open(*Path));
        TestTrue(TEXT("Prepare unsupported schema version"),
                 Fixture.Execute(TEXT("UPDATE world_state SET schema_version=2 WHERE id=1;")));
        Fixture.Close();
        FRatwPersistence Database;
        TestTrue(TEXT("Reopen unknown-version store"), Database.Open(Path));
        TestEqual(TEXT("Unknown version fails closed instead of becoming a blank world"), Database.Load(),
                  FString(TEXT("{\"schema\":-1}")));
    }
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwMemoryTest, "RATW.NPC.InactivityDeadline",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwMemoryTest::RunTest(const FString&)
{
    ratw::MemoryStore Memory;
    Memory.record("keeper", "ash", {1, 1000, "ash", "I promise to return before winter."});
    TestEqual(TEXT("Do not consolidate a minute early"), Memory.consolidate(4599), 0);
    Memory.record("keeper", "ash", {2, 4599, "ash", "One more thing before I go."});
    TestEqual(TEXT("Activity resets the entire hour"), Memory.consolidate(4600), 0);
    TestEqual(TEXT("One second before new deadline"), Memory.consolidate(8198), 0);
    TestEqual(TEXT("Exactly one hour of inactivity"), Memory.consolidate(8199), 1);
    TestEqual(TEXT("Repeated consolidation is harmless"), Memory.consolidate(9000), 0);
    TestEqual(TEXT("Permanent summary count"), static_cast<int32>(Memory.summaries.size()), 1);
    TestFalse(TEXT("Summary can be retrieved"), Memory.recall("keeper", "ash").empty());
    TestTrue(TEXT("Another NPC cannot recall this private conversation"), Memory.recall("scout", "ash").empty());
    TestEqual(TEXT("No memory decays after years"), Memory.consolidate(1000000000), 0);
    TestEqual(TEXT("Summary remains"), static_cast<int32>(Memory.summaries.size()), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwNarrativeTest, "RATW.Narrative.SensoryPrivacy",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwNarrativeTest::RunTest(const FString&)
{
    const auto Post = ratw::parsePost("\"The road was quiet.\" /pose lowers his ears. \"Let us rest.\"");
    TestTrue(TEXT("Parse mixed longform speech and action"), Post.ok);
    const auto Hidden = ratw::perceivePost(Post, 0, 0, 31);
    TestTrue(TEXT("Wholly unperceived event omitted"), Hidden.empty());
    const auto Heard = ratw::perceivePost(Post, 1, 0, 31);
    TestEqual(TEXT("Mixed event preserves segment order"), static_cast<int32>(Heard.size()), 3);
    if (Heard.size() == 3)
    {
        TestEqual(TEXT("Heard words remain"), FString(UTF8_TO_TCHAR(Heard[0].text.c_str())),
                  FString(TEXT("The road was quiet.")));
        TestEqual(TEXT("Hidden action is only missing-movement mark"), FString(UTF8_TO_TCHAR(Heard[1].text.c_str())),
                  FString(TEXT("···")));
    }
    TestFalse(TEXT("Unknown slash action rejected"), ratw::parsePost("/grantxp 100000").ok);
    TestFalse(TEXT("Unclosed quotation rejected"), ratw::parsePost("\"Not finished").ok);
    TestTrue(TEXT("Literal slash escape accepted"), ratw::parsePost("//sigh is only a word here.").ok);
    const auto First = ratw::maskWords("I will keep this promise until the last leaf has fallen", .4, 811);
    const auto Second = ratw::maskWords("I will keep this promise until the last leaf has fallen", .4, 811);
    TestTrue(TEXT("Masks stable for one recipient/event"), First == Second);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwWorldTest, "RATW.World.EngineIntegration",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwWorldTest::RunTest(const FString&)
{
    ratw::World World;
    auto& Wolf = World.addPlayer("ash", "Ash");
    TestEqual(TEXT("Default cell"), FString(UTF8_TO_TCHAR(Wolf.cellId.c_str())), FString(TEXT("tavern")));
    const auto Snapshot = World.snapshot("ash");
    TestEqual(TEXT("Reference width"), Snapshot.cell.width, 32);
    TestEqual(TEXT("Reference height"), Snapshot.cell.height, 24);
    TestTrue(TEXT("No unknown nonadjacent cells sent"), Snapshot.worldMap.size() <= 3);
    TestFalse(TEXT("Invalid actor command rejected"), World.move("forged", 1, 0).ok);
    TestTrue(TEXT("Stationary facing accepted"), World.face("ash", 1, 1).ok);
    const auto Saved = World.save();
    ratw::World Restarted;
    TestTrue(TEXT("Persisted core state restores"), Restarted.restore(Saved).ok);
    TestNotNull(TEXT("Character exists after restart"), Restarted.entity("ash"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwWireTest, "RATW.Network.BoundedSnapshotCodec",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwWireTest::RunTest(const FString&)
{
    FString LargeJson = TEXT("{\"unicode\":\"· 狼 café\",\"tiles\":\"");
    for (int32 Index = 0; Index < 8000; ++Index)
        LargeJson += TEXT("#.^^, ");
    LargeJson += TEXT("\"}");
    TArray<uint8> Bytes;
    int32 RawBytes = 0;
    FString RoundTrip;
    TestTrue(TEXT("Compress a snapshot bigger than old RPC limit"), ratwwire::Encode(LargeJson, Bytes, RawBytes));
    TestTrue(TEXT("Wire stays safely below64KiB"), Bytes.Num() < ratwwire::MaxCompressedBytes);
    TestTrue(TEXT("Decode valid bounded payload"), ratwwire::Decode(Bytes, RawBytes, RoundTrip));
    TestEqual(TEXT("UTF8 roundtrip exact"), RoundTrip, LargeJson);
    TestFalse(TEXT("Reject advertised decompression bomb before allocation"),
              ratwwire::Decode(Bytes, ratwwire::MaxRawBytes + 1, RoundTrip));
    TestFalse(TEXT("Reject negative length"), ratwwire::Decode(Bytes, -1, RoundTrip));
    TestFalse(TEXT("Reject empty payload"), ratwwire::Decode({}, RawBytes, RoundTrip));
    return true;
}
#endif
