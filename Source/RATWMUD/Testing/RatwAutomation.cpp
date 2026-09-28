#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Core/RatwWorld.h"
#include "Runtime/RatwSocialCore.h"
#include "Runtime/RatwPersistence.h"
#include "Runtime/RatwSnapshotCodec.h"
#include "Runtime/RatwSnapshotSections.h"
#include "Runtime/RatwMotion.h"
#include "Runtime/RatwJson.h"
#include "Runtime/RatwSocietyJson.h"
#include "Runtime/RatwDMBridge.h"
#include "Runtime/RatwDialogueProvider.h"
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

namespace
{
// A snapshot shaped like the server's: a cell with its ground, what is seen, the maps, the doors, the satchel.
TSharedPtr<FJsonObject> SampleSnapshot(const FString& Seen, int32 Revealed = -1)
{
    using namespace ratwjson;
    auto Root = New(), Cell = New();
    Array Rows, Heights, Visibility, Map, Doors, Inventory;
    for (int32 Y = 0; Y < 128; ++Y)
    {
        Rows.Add(V(FString::ChrN(128, TEXT('.'))));
        Heights.Add(V(FString::ChrN(128, TEXT('0'))));
        Visibility.Add(V(Y == 5 ? Seen : FString::ChrN(128, TEXT('1'))));
    }
    for (int32 I = 0; I < 40; ++I)
    {
        auto M = New();
        M->SetStringField(TEXT("id"), FString::Printf(TEXT("cell_%d"), I));
        M->SetNumberField(TEXT("x"), I * 64);
        // What the wolf remembers of the cell: the bulk of a map entry.
        FString Glyphs = I == Revealed ? TEXT("~") : TEXT("#");
        uint32 Seed = 2654435761u * (I + 1);              // Varied, as remembered ground is: it doesn't compress away.
        for (int32 K = 0; K < 1000; ++K)
        {
            Seed = Seed * 1664525u + 1013904223u;
            Glyphs.AppendChar(TEXT(".,'\"~^#T")[(Seed >> 24) % 8]);
        }
        M->SetStringField(TEXT("glyphs"), Glyphs);
        Map.Add(V(M));
    }
    Cell->SetStringField(TEXT("id"), TEXT("yard"));
    Cell->SetArrayField(TEXT("rows"), Rows);
    Cell->SetArrayField(TEXT("heights"), Heights);
    Root->SetObjectField(TEXT("cell"), Cell);
    Root->SetArrayField(TEXT("visibility"), Visibility);
    Root->SetArrayField(TEXT("worldMap"), Map);
    Root->SetArrayField(TEXT("travelMap"), Map);
    Root->SetArrayField(TEXT("doors"), Doors);
    Root->SetArrayField(TEXT("inventory"), Inventory);
    Root->SetNumberField(TEXT("revision"), 7);
    return Root;
}
FString Condensed(const TSharedPtr<FJsonObject>& Object)
{
    FString Json;
    const auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
    FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
    return Json;
}
// The same content, whatever order the fields are in (a part put back comes last).
bool Same(const TSharedPtr<FJsonObject>& A, const TSharedPtr<FJsonObject>& B)
{
    return FJsonValue::CompareEqual(FJsonValueObject(A), FJsonValueObject(B));
}
int32 WireBytes(const TSharedPtr<FJsonObject>& Object)
{
    TArray<uint8> Bytes;
    int32 Raw = 0;
    return ratwwire::Encode(Condensed(Object), Bytes, Raw) ? Bytes.Num() : -1;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwDeltaSnapshotTest, "RATW.Network.DeltaSnapshots",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwDeltaSnapshotTest::RunTest(const FString&)
{
    const FString Seen = FString::ChrN(128, TEXT('2'));
    const FString Whole = Condensed(SampleSnapshot(Seen));
    // First: nothing is known to be held, so everything goes, each part with its key.
    ratwsections::FKeys Known;
    ratwsections::FCache Client;
    auto First = SampleSnapshot(Seen);
    const auto FirstKeys = ratwsections::Strip(First, Known);
    int32 Parts = 0;
    for (const auto& Key : FirstKeys) Parts += !Key.Key.Contains(TEXT("#"));
    TestEqual(TEXT("Every part has a key"), Parts, ratwsections::Sections().Num());
    TestEqual(TEXT("and every map entry"), FirstKeys.Num() - Parts, 80);
    const int32 FullBytes = WireBytes(First);
    TestTrue(TEXT("The client takes a whole snapshot as it is"), ratwsections::Fill(First, Client));
    TestTrue(TEXT("and it is exactly the snapshot"), Same(First, SampleSnapshot(Seen)));
    // Unacknowledged, the next goes whole too; acknowledged, what the client holds is left out and put back.
    auto Unacknowledged = SampleSnapshot(Seen);
    ratwsections::Strip(Unacknowledged, Known);
    TestEqual(TEXT("Nothing is left out before an acknowledgement"), WireBytes(Unacknowledged), FullBytes);
    Known = FirstKeys;
    auto Second = SampleSnapshot(Seen);
    ratwsections::Strip(Second, Known);
    const int32 DeltaBytes = WireBytes(Second);
    TestTrue(FString::Printf(TEXT("Held parts are left out (%d bytes, whole %d)"), DeltaBytes, FullBytes), DeltaBytes * 4 < FullBytes);
    TestFalse(TEXT("The ground is not sent again"), Second->GetObjectField(TEXT("cell"))->HasField(TEXT("rows")));
    TestTrue(TEXT("The client puts it back"), ratwsections::Fill(Second, Client));
    TestTrue(TEXT("exactly as it was"), Same(Second, SampleSnapshot(Seen)));
    // One tile comes into view: only what is seen goes, and the result is exact.
    const FString Moved = TEXT("1") + FString::ChrN(127, TEXT('2'));
    auto Third = SampleSnapshot(Moved);
    const auto ThirdKeys = ratwsections::Strip(Third, Known);
    TestTrue(TEXT("What is seen goes again"), Third->HasField(TEXT("visibility")));
    TestFalse(TEXT("but not the maps"), Third->HasField(TEXT("worldMap")));
    TestTrue(TEXT("Filled"), ratwsections::Fill(Third, Client));
    TestTrue(TEXT("exactly"), Same(Third, SampleSnapshot(Moved)));
    // A reveal in one cell: the maps go again, but only that cell's entry whole; the rest by key.
    auto Reveal = SampleSnapshot(Moved, 3);
    ratwsections::Strip(Reveal, ThirdKeys);
    int32 Held = 0;
    for (const auto& Entry : Reveal->GetArrayField(TEXT("worldMap")))
        Held += Entry->AsObject()->HasField(TEXT("$held"));
    TestEqual(TEXT("Every other cell goes by its key"), Held, 39);
    TestTrue(FString::Printf(TEXT("A reveal costs one cell (%d bytes)"), WireBytes(Reveal)), WireBytes(Reveal) * 4 < FullBytes);
    TestTrue(TEXT("Filled"), ratwsections::Fill(Reveal, Client));
    TestTrue(TEXT("exactly, the revealed cell and the rest"), Same(Reveal, SampleSnapshot(Moved, 3)));
    // A client that lost what it kept (a new session) can't fill a snapshot, and says so.
    ratwsections::FCache Forgot;
    auto Fourth = SampleSnapshot(Moved);
    ratwsections::Strip(Fourth, ThirdKeys);
    TestFalse(TEXT("A part it doesn't hold is noticed"), ratwsections::Fill(Fourth, Forgot));
    // An older server's snapshot (no keys) is used as it is.
    auto Old = SampleSnapshot(Seen);
    TestTrue(TEXT("Older snapshots pass untouched"), ratwsections::Fill(Old, Forgot) && Condensed(Old) == Whole);
    // A different part in place of a held one is never mistaken for it.
    auto Changed = SampleSnapshot(Seen);
    Changed->GetObjectField(TEXT("cell"))->SetArrayField(TEXT("rows"), {});
    ratwsections::Strip(Changed, FirstKeys);
    TestTrue(TEXT("Changed ground is sent"), Changed->GetObjectField(TEXT("cell"))->HasField(TEXT("rows")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwBinaryMotionTest, "RATW.Network.BinaryMotionFrames",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwBinaryMotionTest::RunTest(const FString&)
{
    ratw::World World;
    World.addPlayer("ash", "Ash");
    auto Frame = ratwmotion::Frame(World, "ash");
    Frame->SetStringField(TEXT("motionSession"), TEXT("0123456789abcdef"));
    Frame->SetNumberField(TEXT("cellGeneration"), 3);
    Frame->SetNumberField(TEXT("revision"), 1234);
    const auto Bytes = ratwmotion::Pack(Frame);
    const auto Back = ratwmotion::Unpack(Bytes);
    TestTrue(TEXT("A frame survives the wire"), Back.IsValid());
    if (!Back.IsValid()) return false;
    for (const TCHAR* Field : {TEXT("motionSession"), TEXT("observer"), TEXT("cellId")})
        TestEqual(FString::Printf(TEXT("%s kept"), Field), Back->GetStringField(Field), Frame->GetStringField(Field));
    TestEqual(TEXT("generation kept"), Back->GetNumberField(TEXT("cellGeneration")), 3.0);
    TestEqual(TEXT("revision kept"), Back->GetNumberField(TEXT("revision")), 1234.0);
    const auto& Poses = Frame->GetArrayField(TEXT("entities"));
    const auto& Unpacked = Back->GetArrayField(TEXT("entities"));
    TestEqual(TEXT("Every pose"), Unpacked.Num(), Poses.Num());
    for (int32 I = 0; I < FMath::Min(Poses.Num(), Unpacked.Num()); ++I)
    {
        const auto A = Poses[I]->AsObject(), B = Unpacked[I]->AsObject();
        TestEqual(TEXT("its ID"), B->GetStringField(TEXT("id")), A->GetStringField(TEXT("id")));
        TestTrue(TEXT("its place, to a hundred-thousandth of a tile"),
                 FMath::Abs(B->GetNumberField(TEXT("x")) - A->GetNumberField(TEXT("x"))) < 1e-4 &&
                     FMath::Abs(B->GetNumberField(TEXT("y")) - A->GetNumberField(TEXT("y"))) < 1e-4);
        TestEqual(TEXT("moving or not"), B->GetBoolField(TEXT("moving")), A->GetBoolField(TEXT("moving")));
    }
    FString Json;
    const auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
    FJsonSerializer::Serialize(Frame.ToSharedRef(), Writer);
    TestTrue(FString::Printf(TEXT("Smaller than JSON (%d bytes against %d)"), Bytes.Num(), FTCHARToUTF8(*Json).Length()),
             Bytes.Num() < FTCHARToUTF8(*Json).Length());
    // Anything malformed is refused whole.
    auto Cut = Bytes;
    Cut.SetNum(Cut.Num() - 3);
    TestFalse(TEXT("A cut frame is refused"), ratwmotion::Unpack(Cut).IsValid());
    auto Wrong = Bytes;
    Wrong[0] ^= 0xff;
    TestFalse(TEXT("Not a frame"), ratwmotion::Unpack(Wrong).IsValid());
    auto Longer = Bytes;
    Longer.Add(0);
    TestFalse(TEXT("Trailing bytes are refused"), ratwmotion::Unpack(Longer).IsValid());
    TestFalse(TEXT("Nothing is nothing"), ratwmotion::Unpack({}).IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwPostureStorageTest, "RATW.Movement.PostureSkillsPersistence",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwPostureStorageTest::RunTest(const FString&)
{
    ratw::World World;
    auto& Wolf = World.addPlayer("ash", "Ash");
    Wolf.sneakSkill = 73;
    Wolf.hearingSkill = 42;
    World.setPosture("ash", "lying");
    World.move("ash", 1, 0);
    const auto Json = ratwjson::PersistEntity(Wolf, World.time());
    TestEqual(TEXT("Rising state is sent to UI"), ratwjson::String(Json, TEXT("posture")), FString(TEXT("rising")));
    TestTrue(TEXT("Queued motion reports moving before displacement"), ratwjson::Bool(Json, TEXT("moving")));
    auto Saved = World.save();
    Saved.players[0] = ratwjson::ReadEntity(ratwjson::Decode(ratwjson::Encode(Json)));
    ratw::World Restored;
    TestTrue(TEXT("Native serialized transitional save restores"), Restored.restore(Saved).ok);
    const auto* Loaded = Restored.entity("ash");
    TestEqual(TEXT("Sneak skill persists"), Loaded->sneakSkill, 73.0);
    TestEqual(TEXT("Hearing skill persists"), Loaded->hearingSkill, 42.0);
    TestTrue(TEXT("Restart resolves posture without restoring motion"),
             Loaded->posture == "crouching" && Loaded->path.empty() && Loaded->input.x == 0 && !Loaded->turning);
    const auto Public = ratwjson::Entity(Wolf, World.time());
    TestFalse(TEXT("Other players do not receive private sneak skill"), Public->HasField(TEXT("sneakSkill")));
    TestFalse(TEXT("Other players do not receive private hearing skill"), Public->HasField(TEXT("hearingSkill")));
    auto Legacy = ratwjson::PersistEntity(*Loaded, World.time());
    Legacy->RemoveField(TEXT("sneakSkill"));
    Legacy->RemoveField(TEXT("hearingSkill"));
    const auto Old = ratwjson::ReadEntity(Legacy);
    TestEqual(TEXT("Older saves default sneak skill safely"), Old.sneakSkill, 0.0);
    TestEqual(TEXT("Older saves default hearing skill safely"), Old.hearingSkill, 0.0);
    Legacy->SetStringField(TEXT("sneakSkill"), TEXT("invalid"));
    Saved.players[0] = ratwjson::ReadEntity(Legacy);
    TestFalse(TEXT("Malformed new skill field fails restoration"), Restored.restore(Saved).ok);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwScentWireTest, "RATW.Perception.ScentWindWirePrivacy",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwScentWireTest::RunTest(const FString&)
{
    ratw::World World;
    auto& Observer = World.addPlayer("listener", "Listener");
    auto& Source = World.addPlayer("hidden-secret-id", "Hidden secret name");
    Observer.cellId = Source.cellId = "exterior";
    Observer.position = {20.5, 12.5};
    Source.position = {8.5, 12.5};
    Source.posture = "crouching";
    Observer.scentSkill = 64;
    Observer.noseHealth = .8;
    World.setWeather("exterior", ratw::Weather::Clear);
    TestTrue(TEXT("Configure east-flowing breeze"), World.setWind("exterior", 0, .5).ok);
    const auto View = World.snapshot("listener");
    TestTrue(TEXT("Hidden source has no visual identification"), World.visionClarity("listener", Source.id) == 0);
    TestTrue(TEXT("Hidden source is detected by scent"), World.scentClarity("listener", Source.id) > 0);
    const auto Sense = ratwjson::Senses(View);
    const auto Encoded = ratwjson::Encode(Sense);
    TestFalse(TEXT("Scent wire contains no source ID"), Encoded.Contains(TEXT("hidden-secret-id")));
    TestFalse(TEXT("Scent wire contains no source name"), Encoded.Contains(TEXT("Hidden secret name")));
    const auto Cues = ratwjson::Items(Sense, TEXT("scentCues"));
    TestTrue(TEXT("At most eight aggregate sectors"), Cues.Num() > 0 && Cues.Num() <= 8);
    for (const auto& Value : Cues)
    {
        const auto Cue = Value->AsObject();
        TestEqual(TEXT("Wire exposes only coarse sector, category and wind flag"), Cue->Values.Num(), 3);
        TestTrue(TEXT("Sector is bounded"),
                 ratwjson::Number(Cue, TEXT("sector")) >= 0 && ratwjson::Number(Cue, TEXT("sector")) < 8);
        TestFalse(TEXT("No exact X coordinate"), Cue->HasField(TEXT("x")));
        TestFalse(TEXT("No source count"), Cue->HasField(TEXT("count")));
    }
    const auto Public = ratwjson::Entity(Observer, World.time());
    TestFalse(TEXT("Public entity does not expose scent skill"), Public->HasField(TEXT("scentSkill")));
    const auto Recovered =
        ratwjson::ReadEntity(ratwjson::Decode(ratwjson::Encode(ratwjson::PersistEntity(Observer, 0))));
    TestEqual(TEXT("Scent skill persists through native JSON"), Recovered.scentSkill, 64.0);
    TestEqual(TEXT("Nose health persists through native JSON"), Recovered.noseHealth, .8);
    const auto Wind = World.windAt("exterior");
    const auto RecoveredWind = ratwjson::ReadWind(ratwjson::Decode(ratwjson::Encode(ratwjson::Wind(Wind))));
    TestEqual(TEXT("Wind direction roundtrip"), RecoveredWind.direction, Wind.direction);
    TestEqual(TEXT("Wind strength roundtrip"), RecoveredWind.strength, Wind.strength);
    const auto BadWind = ratwjson::Wind(Wind);
    BadWind->SetStringField(TEXT("direction"), TEXT("invalid"));
    auto Saved = World.save();
    Saved.winds["exterior"] = ratwjson::ReadWind(BadWind);
    TestFalse(TEXT("Malformed wind cannot silently become calm weather"), World.restore(Saved).ok);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwPaceWireTest, "RATW.Movement.PacePersistenceAndWire",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwPaceWireTest::RunTest(const FString&)
{
    using namespace ratwjson;
    ratw::World World;
    auto& Wolf = World.addPlayer("ash", "Ash");
    Wolf.dexterity = 71;
    Wolf.stamina = 12.5;
    Wolf.exhausted = true;
    TestTrue(TEXT("Valid pace intent accepted"), PaceCommand(World, "ash", Decode(TEXT("{\"pace\":8}"))).ok);
    for (const TCHAR* Input : {TEXT("{}"), TEXT("{\"pace\":-1}"), TEXT("{\"pace\":11}"), TEXT("{\"pace\":4.5}"),
                               TEXT("{\"pace\":true}"), TEXT("{\"pace\":\"4\"}"), TEXT("{\"pace\":1e100}")})
        TestFalse(TEXT("Malformed pace never becomes a movement multiplier"),
                  PaceCommand(World, "ash", Decode(Input)).ok);
    TestEqual(TEXT("Rejected commands preserve selected pace"), Wolf.pace, 8);
    const auto Public = ratwjson::Entity(Wolf, 0);
    TestFalse(TEXT("Other observers do not receive private stamina"), Public->HasField(TEXT("stamina")));
    TestFalse(TEXT("Other observers do not receive dexterity"), Public->HasField(TEXT("dexterity")));
    const auto Self = New();
    PrivatePace(Self, Wolf);
    TestEqual(TEXT("Owner receives requested pace"), Number(Self, TEXT("pace")), 8.0);
    TestEqual(TEXT("Fatigue reports effective walking"), Number(Self, TEXT("effectivePace")), 0.0);
    TestTrue(TEXT("Owner sees fatigue limitation"), Bool(Self, TEXT("exhausted")));
    const auto Serialized = PersistEntity(Wolf, 0);
    auto Saved = World.save();
    Saved.players[0] = ReadEntity(Decode(Encode(Serialized)));
    ratw::World Reopened;
    TestTrue(TEXT("Native roundtrip preserves stamina without free refill"), Reopened.restore(Saved).ok);
    TestEqual(TEXT("Dexterity survives restart"), Reopened.entity("ash")->dexterity, 71.0);
    TestEqual(TEXT("Stamina survives restart"), Reopened.entity("ash")->stamina, 12.5);
    TestEqual(TEXT("Pace survives restart"), Reopened.entity("ash")->pace, 8);
    TestTrue(TEXT("Exhaustion hysteresis survives restart"), Reopened.entity("ash")->exhausted);
    for (const TCHAR* Key : {TEXT("dexterity"), TEXT("stamina"), TEXT("pace"), TEXT("exhausted")})
    {
        auto Corrupt = Decode(Encode(Serialized));
        Corrupt->SetStringField(Key, TEXT("invalid"));
        Saved.players[0] = ReadEntity(Corrupt);
        TestFalse(TEXT("Malformed movement stat rejects complete save"), Reopened.restore(Saved).ok);
        TestEqual(TEXT("Invalid save leaves old stamina unchanged"), Reopened.entity("ash")->stamina, 12.5);
    }
    auto Legacy = Decode(Encode(Serialized));
    for (const TCHAR* Key : {TEXT("dexterity"), TEXT("stamina"), TEXT("pace"), TEXT("exhausted")})
        Legacy->RemoveField(Key);
    const auto Old = ReadEntity(Legacy);
    TestEqual(TEXT("Legacy character dexterity defaults safely"), Old.dexterity, 50.0);
    TestEqual(TEXT("Legacy character starts with full stamina once"), Old.stamina, 100.0);
    TestEqual(TEXT("Legacy character walks by default"), Old.pace, 0);
    TestFalse(TEXT("Legacy character is not exhausted"), Old.exhausted);
    TestFalse(TEXT("Travel intentions do not enter persisted entity"), Serialized->HasField(TEXT("travel")));
    const auto Places = World.travelMap("ash");
    for (const auto& Place : Places)
    {
        TestTrue(TEXT("Travel index contains only entered cells"), Place.knowledge == ratw::Knowledge::Visited);
        const auto Entry = ratwjson::MapCell(Place, false);
        TestFalse(TEXT("Distant atlas does not resend terrain"), Entry->HasField(TEXT("glyphs")));
        TestFalse(TEXT("Distant atlas never contains remote entities"), Entry->HasField(TEXT("entities")));
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwEnvironmentWireTest, "RATW.Environment.ClockWeatherAndWire",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwEnvironmentWireTest::RunTest(const FString&)
{
    using namespace ratwjson;
    ratw::World World;
    auto& Wolf = World.addPlayer("ash", "Ash");
    Wolf.cellId = "exterior";
    Wolf.position = {16.5, 12.5};
    World.setWeather("exterior", ratw::Weather::Clear);
    const double BeforeHour = World.environmentAt("exterior").hour;
    TestFalse(TEXT("Players cannot change shared clock"),
              EnvironmentCommand(World, "exterior", TEXT("time"), TEXT("night"), false).ok);
    TestFalse(TEXT("Players cannot change weather"),
              EnvironmentCommand(World, "exterior", TEXT("weather"), TEXT("snow"), false).ok);
    TestEqual(TEXT("Denied clock request leaves time unchanged"), World.environmentAt("exterior").hour, BeforeHour);
    TestTrue(TEXT("Denied weather request leaves world unchanged"),
             World.cell("exterior")->weather == ratw::Weather::Clear);
    for (const TCHAR* Bad : {TEXT(""), TEXT("midnight"), TEXT("nan"), TEXT("-1"), TEXT("24")})
        TestFalse(TEXT("Unknown clock presets rejected"),
                  EnvironmentCommand(World, "exterior", TEXT("time"), Bad, true).ok);
    TestFalse(TEXT("Unknown weather never silently becomes clear"),
              EnvironmentCommand(World, "exterior", TEXT("weather"), TEXT("storm-typo"), true).ok);
    TestTrue(TEXT("Development night preset accepted"),
             EnvironmentCommand(World, "exterior", TEXT("time"), TEXT("night"), true).ok);
    TestTrue(TEXT("Development snow preset accepted"),
             EnvironmentCommand(World, "exterior", TEXT("weather"), TEXT("snow"), true).ok);
    const auto View = World.snapshot("ash");
    const auto Wire = Decode(Encode(ratwjson::Environment(View.environment)));
    TestEqual(TEXT("Shared phase reaches client"), String(Wire, TEXT("phase")), FString(TEXT("night")));
    TestTrue(TEXT("Night and snow sight penalties stack"), Number(Wire, TEXT("sight")) < .3);
    TestTrue(TEXT("Movement slowdown reaches client"), Number(Wire, TEXT("movement")) < 1);
    TestEqual(TEXT("Wire matches exact authoritative hearing factor"), Number(Wire, TEXT("hearing")),
              World.environmentAt("exterior").hearing);
    TestEqual(TEXT("Weather/lighting/calendar metadata has no actor IDs or coordinates"), Wire->Values.Num(), 14);
    const auto Shelter = ratwjson::Environment(World.environmentAt("tavern"));
    FRatwDialogueContext Context;
    Context.HeardText = TEXT("What is the weather like?");
    Context.Environment = EnvironmentDescription(*World.cell("exterior"), View.environment);
    TestEqual(TEXT("Authored NPC response follows current local conditions"),
              FRatwDialogueProvider::AuthoredReply(Context), Context.Environment);
    TestFalse(TEXT("Snow fallback does not invent rain"),
              FRatwDialogueProvider::AuthoredReply(Context).Contains(TEXT("Rain")));
    TestEqual(TEXT("Shelter uses same world hour"), Number(Shelter, TEXT("hour")), Number(Wire, TEXT("hour")));
    TestEqual(TEXT("Sheltered light is unaffected by outdoor night"), Number(Shelter, TEXT("illumination")), 1.0);
    for (const TCHAR* Key : {TEXT("sight"), TEXT("hearing"), TEXT("scent"), TEXT("movement")})
        TestEqual(TEXT("Shelter has neutral environment factors"), Number(Shelter, Key), 1.0);
    auto Saved = World.save();
    auto Record = New();
    Record->SetNumberField(TEXT("clockOffsetHours"), Saved.clockOffsetHours);
    Saved.clockOffsetHours = ReadClockOffset(Decode(Encode(Record)));
    ratw::World Restarted;
    TestTrue(TEXT("Clock survives native serialized restore"), Restarted.restore(Saved).ok);
    TestEqual(TEXT("Restart does not award offline clock advancement"), Restarted.environmentAt("exterior").hour,
              World.environmentAt("exterior").hour);
    TestEqual(TEXT("Legacy save defaults to noon offset"), ReadClockOffset(New()), 12.0);
    for (const TCHAR* Bad : {TEXT("{\"clockOffsetHours\":true}"), TEXT("{\"clockOffsetHours\":\"12\"}"),
                             TEXT("{\"clockOffsetHours\":null}"), TEXT("{\"clockOffsetHours\":-1}"),
                             TEXT("{\"clockOffsetHours\":24}"), TEXT("{\"clockOffsetHours\":1e100}")})
    {
        Saved.clockOffsetHours = ReadClockOffset(Decode(Bad));
        TestFalse(TEXT("Malformed clock atomically rejects save"), Restarted.restore(Saved).ok);
    }
    Saved = World.save();
    for (const TCHAR* Bad : {TEXT("{\"w\":true}"), TEXT("{\"w\":\"1\"}"), TEXT("{\"w\":1.5}"), TEXT("{\"w\":1e100}"),
                             TEXT("{\"w\":null}"), TEXT("{\"w\":7}")})
    {
        const auto Invalid = Decode(Bad);
        Saved.weather["exterior"] = ReadWeather(Invalid->Values[TEXT("w")]);
        TestFalse(TEXT("Malformed enum cannot overflow or become clear"), Restarted.restore(Saved).ok);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwLightingWireTest, "RATW.Environment.IndoorLightingWire",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwLightingWireTest::RunTest(const FString&)
{
    using namespace ratwjson;
    ratw::World World;
    World.addPlayer("ash", "Ash");
    TestFalse(TEXT("Players cannot relight cells through a forged development command"),
              EnvironmentCommand(World, "tavern", TEXT("lighting"), TEXT("unlit"), false).ok);
    TestFalse(TEXT("Unknown lighting preset cannot change world state"),
              EnvironmentCommand(World, "tavern", TEXT("lighting"), TEXT("laser"), true).ok);
    TestTrue(TEXT("Dark-room preset accepted in development"),
             EnvironmentCommand(World, "tavern", TEXT("lighting"), TEXT("unlit"), true).ok);
    World.setTimeOfDay(12);
    const auto Dark = World.snapshot("ash");
    const auto Wire = Decode(Encode(ratwjson::Environment(Dark.environment)));
    TestTrue(TEXT("An unlit room is dark even at noon"), Number(Wire, TEXT("illumination")) < .1);
    TestEqual(TEXT("Shelter is not a source of light"), String(Wire, TEXT("lightSource")), FString(TEXT("dark")));
    TestEqual(TEXT("Darkness does not mute hearing"), Number(Wire, TEXT("hearing")), 1.0);
    TestEqual(TEXT("An unlit room never glows"), Number(Wire, TEXT("glowStrength")), 0.0);
    TestTrue(TEXT("NPC scene context accurately describes unlit interior"),
             EnvironmentDescription(Dark.cell, Dark.environment).Contains(TEXT("unlit and dark")));
    auto Saved = World.save();
    Saved.lighting["tavern"] = ReadLighting(Decode(Encode(Lighting(World.cell("tavern")->lighting))));
    ratw::World Restarted;
    TestTrue(TEXT("Light profile roundtrips through JSON and core restore"), Restarted.restore(Saved).ok);
    TestEqual(TEXT("Restart does not relight an unlit room"), Restarted.environmentAt("tavern").illumination,
              Dark.environment.illumination);
    for (const TCHAR* Bad : {TEXT("{}"), TEXT("{\"artificial\":true,\"daylightAccess\":0,\"tone\":\"warm\"}"),
                             TEXT("{\"artificial\":\"1\",\"daylightAccess\":0,\"tone\":\"warm\"}"),
                             TEXT("{\"artificial\":1e100,\"daylightAccess\":0,\"tone\":\"warm\"}"),
                             TEXT("{\"artificial\":0,\"daylightAccess\":null,\"tone\":\"warm\"}"),
                             TEXT("{\"artificial\":0,\"daylightAccess\":0,\"tone\":\"red\"}")})
    {
        Saved.lighting["tavern"] = ReadLighting(Decode(Bad));
        TestFalse(TEXT("Malformed lighting atomically rejects complete save"), Restarted.restore(Saved).ok);
        TestEqual(TEXT("Rejected save leaves old room dark"), Restarted.environmentAt("tavern").illumination,
                  Dark.environment.illumination);
    }
    TestTrue(TEXT("Warm development profile accepted"),
             EnvironmentCommand(World, "tavern", TEXT("lighting"), TEXT("warm"), true).ok);
    const auto Day = World.environmentAt("tavern");
    TestEqual(TEXT("Daylit tavern has no ambient glow"), Day.glowStrength, 0.0);
    World.setTimeOfDay(0);
    const auto Night = World.environmentAt("tavern");
    TestEqual(TEXT("Lit tavern stays clear at night"), Night.illumination, 1.0);
    TestTrue(TEXT("Warm glow is present at night"), Night.glowStrength > .9 && Night.lightingTone == "warm");
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwLargePopulationTest, "RATW.Society.LargePopulationPersistence",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwLargePopulationTest::RunTest(const FString&)
{
    // A town of 200 residents (Upper Accord has 157) saves, round-trips through the save's JSON and restores whole.
    using namespace ratwjson;
    std::string Cell = "id: field\nname: Field\ndescription: Open ground.\nworld: 0 0 0\noutdoors: true\nweather: clear\ngrid:\n";
    for (int Y = 0; Y < 32; ++Y)
        Cell += std::string(32, ',') + "\n";
    std::string Manifest = "RATW_WORLD 2\ncell \"field\" \"cells/field.cell\"\nspawn \"field\" 1.5 1.5\neconomy 1000 100 50 10 12\n";
    for (int I = 0; I < 200; ++I)
    {
        const std::string Id = "r" + std::to_string(I), X = std::to_string(1 + I % 30) + ".5", Y = std::to_string(1 + I / 30) + ".5";
        Manifest += "resident \"" + Id + "\" \"Resident " + std::to_string(I) + "\" \"civilian\" \"working\" \"A wolf.\" \"Hello.\" 30 "
                    "\"timber\" \"female\" \"average\" \"saddle\" 3 1 5 1 1 6 18 \"-\" 10 0 1 \"field\" " + X + " " + Y +
                    " \"field\" " + X + " " + Y + " \"field\" " + X + " " + Y + "\n";
    }
    ratw::World World;
    const auto Loaded = World.loadWorldFiles({{"world.ratw", Manifest}, {"cells/field.cell", Cell}}, "many");
    TestTrue(TEXT("A world of 200 residents loads"), Loaded.ok);
    if (!Loaded.ok)
        return true;
    for (int I = 0; I < 40; ++I)
        World.tick(1);
    auto Saved = World.save();
    TestEqual(TEXT("All 200 residents are in the save"), int32(Saved.society.residents.size()), 200);
    Saved.society = ReadSociety(Decode(Encode(ratwjson::Society(Saved.society))));
    TestEqual(TEXT("All 200 residents survive the save's JSON"), int32(Saved.society.residents.size()), 200);
    ratw::World Restarted;
    TestTrue(TEXT("The restarted world loads"), Restarted.loadWorldFiles({{"world.ratw", Manifest}, {"cells/field.cell", Cell}}, "many").ok);
    const auto Restored = Restarted.restore(Saved);
    TestTrue(TEXT("A save of 200 residents restores"), Restored.ok);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwSocietyWireTest, "RATW.Society.CalendarEconomyPersistence",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwSocietyWireTest::RunTest(const FString&)
{
    using namespace ratwjson;
    ratw::World World;
    auto& P = World.addPlayer("player-ash", "Ash");
    auto Saved = World.save();
    Saved.society = ReadSociety(Decode(Encode(ratwjson::Society(Saved.society))));
    ratw::World Restarted;
    TestTrue(TEXT("Finite cash, stock, needs, quotas and ledger JSON roundtrip"), Restarted.restore(Saved).ok);
    TestEqual(TEXT("Restore does not give repeat welcome money"), Restarted.society().account(P.id)->cash, std::int64_t(20));
    for (const TCHAR* Key : {TEXT("minted"), TEXT("budgetDay"), TEXT("herbPatch"), TEXT("exportsRemaining")})
    {
        auto Bad = ratwjson::Society(World.society().state()); Bad->SetStringField(Key, TEXT("1"));
        Saved.society = ReadSociety(Bad);
        TestFalse(TEXT("String-valued accounting field rejects entire save"), Restarted.restore(Saved).ok);
    }
    auto Bad = ratwjson::Society(World.society().state());
    Child(Child(Bad, TEXT("accounts")), TEXT("player-ash"))->SetNumberField(TEXT("cash"), 999);
    Saved.society = ReadSociety(Bad);
    TestFalse(TEXT("Unbacked cash creation rejects complete save"), Restarted.restore(Saved).ok);
    Bad = ratwjson::Society(World.society().state());
    Bad->SetStringField(TEXT("ledger"), TEXT("invalid")); Saved.society = ReadSociety(Bad);
    TestFalse(TEXT("Wrong ledger shape cannot silently reset history"), Restarted.restore(Saved).ok);
    Bad = ratwjson::Society(World.society().state());
    Child(Child(Bad, TEXT("residents")), TEXT("npc_cook"))->SetNumberField(TEXT("reason"), 12);
    Saved.society = ReadSociety(Bad);
    TestFalse(TEXT("JSON numeric values cannot coerce into resident text"), Restarted.restore(Saved).ok);
    Bad = ratwjson::Society(World.society().state());
    Child(Bad, TEXT("accounts"))->Values[TEXT("player-ash")] = TSharedPtr<FJsonValue>();
    Saved.society = ReadSociety(Bad);
    TestFalse(TEXT("Null in-memory JSON node rejects rather than dereferencing"), Restarted.restore(Saved).ok);
    TestFalse(TEXT("Ordinary player cannot accelerate shared calendar"), EnvironmentCommand(World, "tavern", TEXT("calendar"), TEXT("year"), false).ok);
    TestTrue(TEXT("Developer can exercise an annual birthday"), EnvironmentCommand(World, "tavern", TEXT("calendar"), TEXT("year"), true).ok);
    auto E = ReadEntity(Decode(Encode(PersistEntity(P, World.time()))));
    TestEqual(TEXT("Age JSON persists annual advancement"), E.age, 19);
    TestEqual(TEXT("Physical birthday reward persists"), E.strength, 51.0);
    TestEqual(TEXT("Birthday notification remains pending until delivered"), E.ageNoticePending, 1);
    const auto Env = ratwjson::Environment(World.environmentAt("tavern"));
    TestEqual(TEXT("Calendar year reaches the UI"), Number(Child(Env, TEXT("calendar")), TEXT("year")), 2.0);
    TestTrue(TEXT("Lunar illumination is bounded"), Number(Child(Env, TEXT("calendar")), TEXT("moonIllumination")) >= 0 && Number(Child(Env, TEXT("calendar")), TEXT("moonIllumination")) <= 1);
    auto Actor = PersistEntity(P, World.time()); Actor->SetNumberField(TEXT("age"), 18.5);
    Saved = World.save(); Saved.players[0] = ReadEntity(Actor);
    TestFalse(TEXT("Fractional age cannot corrupt saved character"), Restarted.restore(Saved).ok);
    Actor = PersistEntity(P, World.time()); Actor->SetStringField(TEXT("lastBirthdayDay"), TEXT("365"));
    Saved.players[0] = ReadEntity(Actor);
    TestFalse(TEXT("Malformed birthday anchor atomically rejects checkpoint"), Restarted.restore(Saved).ok);
    const auto Cash = World.society().account(P.id)->cash;
    TestFalse(TEXT("Gather cannot be invoked from inside the tavern"), World.gather(P.id).ok);
    P.cellId = "exterior"; P.position = {17.5, 7.5}; World.setTimeOfDay(12); World.setWeather("exterior", ratw::Weather::Clear);
    const int Herbs = ratw::Society::stock(*World.society().account(P.id), "herbs"), Patch = World.society().state().herbPatch;
    TestTrue(TEXT("Visible in-reach gather consumes an actual patch resource"), World.gather(P.id).ok);
    TestEqual(TEXT("Gathered goods enter actual player inventory"), ratw::Society::stock(*World.society().account(P.id), "herbs"), Herbs + 1);
    TestEqual(TEXT("Gathering depletes shared finite patch"), World.society().state().herbPatch, Patch - 1);
    TestEqual(TEXT("Gathering does not mint money"), World.society().account(P.id)->cash, Cash);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwDirectorTest, "RATW.Director.PrivateOperatorContract",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwDirectorTest::RunTest(const FString&)
{
    using namespace ratwjson;
    FRatwDMBridge Bridge;
    ratw::World World;
    World.addPlayer("player-ash", "Ash");
    std::set<std::string> Online{"player-ash"};
    int Commits = 0, Notices = 0;
    auto Commit = [&]() { ++Commits; return true; };
    auto Notice = [&](const std::set<std::string>& To, const FString&) { Notices += int(To.size()); };
    auto Envelope = [&](const TCHAR* Id, const TCHAR* Kind, const TCHAR* Payload) {
        auto O = New(); O->SetNumberField(TEXT("version"), 1); O->SetStringField(TEXT("id"), Id);
        O->SetStringField(TEXT("worldId"), Bridge.Id()); O->SetNumberField(TEXT("createdAtUnix"), 1000);
        O->SetNumberField(TEXT("expiresAtUnix"), 1120); O->SetStringField(TEXT("kind"), Kind);
        O->SetObjectField(TEXT("payload"), Decode(Payload)); return O;
    };
    const FString Hash = TEXT("0123456789012345678901234567890123456789");
    auto Transfer = Envelope(TEXT("transfer-1"), TEXT("economy_transfer"), TEXT("{\"from\":\"treasury\",\"to\":\"player-ash\",\"item\":\"meal\",\"quantity\":1,\"coins\":3}"));
    TestTrue(TEXT("Operator transfers finite existing resources"), Bool(Bridge.Execute(Transfer, TEXT("transfer-1"), Hash, World, Online, 1001, Commit, Notice), TEXT("ok")));
    TestEqual(TEXT("Existing purse receives exactly three pennies"), World.society().account("player-ash")->cash, std::int64_t(23));
    TestTrue(TEXT("Money still conserved"), World.society().conserved());
    TestTrue(TEXT("Retry replays receipt even after original expiry"), Bool(Bridge.Execute(Transfer, TEXT("transfer-1"), Hash, World, Online, 1200, Commit, Notice), TEXT("ok")));
    TestEqual(TEXT("Retry neither re-commits nor duplicates transfer"), Commits, 1);
    TestEqual(TEXT("Retry preserves purse"), World.society().account("player-ash")->cash, std::int64_t(23));
    FRatwDMBridge Restart;
    TestTrue(TEXT("Private receipt state survives JSON restore"), Restart.Restore(Decode(Encode(Bridge.State()))));
    auto BrokenReceipt = Decode(Encode(Bridge.State()));
    Child(Items(BrokenReceipt, TEXT("receipts"))[0]->AsObject(), TEXT("result"))->SetNumberField(TEXT("version"), 2);
    TestFalse(TEXT("Unsupported persisted receipt version fails closed"), Restart.Restore(BrokenReceipt));
    BrokenReceipt = Decode(Encode(Bridge.State()));
    Child(Items(BrokenReceipt, TEXT("receipts"))[0]->AsObject(), TEXT("result"))->RemoveField(TEXT("detail"));
    TestFalse(TEXT("Missing receipt detail cannot be replayed as a valid response"), Restart.Restore(BrokenReceipt));
    BrokenReceipt = Decode(Encode(Bridge.State()));
    Items(BrokenReceipt, TEXT("receipts"))[0]->AsObject()->SetStringField(TEXT("fingerprint"), FString::ChrN(40, 'z'));
    TestFalse(TEXT("Persisted fingerprint must contain only hexadecimal"), Restart.Restore(BrokenReceipt));
    TestTrue(TEXT("Restarted bridge replays same request"), Bool(Restart.Execute(Transfer, TEXT("transfer-1"), Hash, World, Online, 1200, Commit, Notice), TEXT("ok")));
    TestFalse(TEXT("Reused ID with altered content is rejected"), Bool(Bridge.Execute(Transfer, TEXT("transfer-1"), TEXT("different"), World, Online, 1001, Commit, Notice), TEXT("ok")));
    auto Cross = Envelope(TEXT("cross-world"), TEXT("weather"), TEXT("{\"cell\":\"exterior\",\"preset\":\"fog\"}"));
    Cross->SetStringField(TEXT("worldId"), TEXT("wrong-world"));
    TestFalse(TEXT("Cross-world requests are rejected"), Bool(Bridge.Execute(Cross, TEXT("cross-world"), Hash, World, Online, 1001, Commit, Notice), TEXT("ok")));
    auto Bad = Envelope(TEXT("bad-bool"), TEXT("economy_transfer"), TEXT("{\"from\":\"treasury\",\"to\":\"player-ash\",\"item\":false,\"quantity\":0,\"coins\":3}"));
    TestFalse(TEXT("Boolean item cannot coerce into an empty money-only transfer"), Bool(Bridge.Execute(Bad, TEXT("bad-bool"), Hash, World, Online, 1001, Commit, Notice), TEXT("ok")));
    auto Announcement = Envelope(TEXT("notice-1"), TEXT("notice"), TEXT("{\"scope\":\"players\",\"targets\":[\"player-ash\",\"player-absent\"],\"text\":\"A bell sounds across the yard.\"}"));
    TestTrue(TEXT("Chapter-resolved list only notices connected members"), Bool(Bridge.Execute(Announcement, TEXT("notice-1"), Hash, World, Online, 1001, Commit, Notice), TEXT("ok")));
    TestEqual(TEXT("One recipient, no invented offline delivery"), Notices, 1);
    Bridge.Execute(Announcement, TEXT("notice-1"), Hash, World, Online, 1002, Commit, Notice);
    TestEqual(TEXT("Retried notice is not emitted a second time"), Notices, 1);
    auto BadNotice = Envelope(TEXT("bad-notice"), TEXT("notice"), TEXT("{\"scope\":\"world\",\"target\":false,\"text\":\"Must not broadcast.\"}"));
    TestFalse(TEXT("Malformed irrelevant notice target fails closed"), Bool(Bridge.Execute(BadNotice, TEXT("bad-notice"), Hash, World, Online, 1001, Commit, Notice), TEXT("ok")));
    TestEqual(TEXT("Malformed notice emitted nothing"), Notices, 1);
    auto Unsupported = Envelope(TEXT("army-1"), TEXT("army"), TEXT("{}"));
    TestFalse(TEXT("Unimplemented army cannot be reported as applied"), Bool(Bridge.Execute(Unsupported, TEXT("army-1"), Hash, World, Online, 1001, Commit, Notice), TEXT("ok")));
    auto Expired = Envelope(TEXT("expired-1"), TEXT("weather"), TEXT("{\"cell\":\"exterior\",\"preset\":\"fog\"}"));
    TestFalse(TEXT("Expired new request is rejected"), Bool(Bridge.Execute(Expired, TEXT("expired-1"), Hash, World, Online, 1121, Commit, Notice), TEXT("ok")));
    auto Hidden = World.addPlayer("player-offline", "Offline"); World.removePlayer("player-offline");
    const auto View = Bridge.Snapshot(World, {{"player-offline", Hidden}}, Online, {{"player-ash", 1000}}, 42, 1002);
    TestEqual(TEXT("Operator sees all three cells independent of player fog"), Items(View, TEXT("cells")).Num(), 3);
    TestEqual(TEXT("Operator sees all six NPCs plus online and saved offline players"), Items(View, TEXT("characters")).Num(), 8);
    TestFalse(TEXT("Operator export excludes conversation memory"), View->HasField(TEXT("activeMemory")));
    FRatwDMBridge Failing;
    auto Rollback = Envelope(TEXT("rollback-1"), TEXT("economy_transfer"), TEXT("{\"from\":\"treasury\",\"to\":\"player-ash\",\"item\":\"\",\"quantity\":0,\"coins\":4}"));
    Rollback->SetStringField(TEXT("worldId"), Failing.Id());
    const auto Cash = World.society().account("player-ash")->cash;
    World.entity("player-ash")->path = {{17.5, 12.5}};
    World.entity("player-ash")->typing = true;
    TestFalse(TEXT("Checkpoint failure does not acknowledge application"), Bool(Failing.Execute(Rollback, TEXT("rollback-1"), Hash, World, Online, 1001, []() { return false; }, Notice), TEXT("ok")));
    TestEqual(TEXT("Failed checkpoint rolls back authoritative funds"), World.society().account("player-ash")->cash, Cash);
    TestTrue(TEXT("Rollback retains unrelated transient movement and typing"), World.entity("player-ash")->typing && World.entity("player-ash")->path.size() == 1);
    return true;
}
#endif
