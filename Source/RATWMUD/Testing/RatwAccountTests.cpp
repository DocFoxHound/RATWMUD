#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Runtime/RatwAccounts.h"
#include "Runtime/RatwPersistence.h"
#include "Runtime/RatwJson.h"
#include "HAL/FileManager.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwAccountValidationTest, "RATW.Accounts.LocalInputBoundary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwAccountValidationTest::RunTest(const FString&)
{
    FString User;
    TestTrue(TEXT("Normalize valid ASCII user"), FRatwAccounts::NormalizeUsername(TEXT("Ash_2-Wolf"), User));
    TestEqual(TEXT("Case-insensitive identity"), User, FString(TEXT("ash_2-wolf")));
    for (const auto* Invalid : {TEXT("ab"), TEXT("1wolf"), TEXT(" ash"), TEXT("ash wolf"), TEXT("ash@wolf"), TEXT("wölf"), TEXT("Ash\n")})
        TestFalse(TEXT("Reject ambiguous username"), FRatwAccounts::NormalizeUsername(Invalid, User));
    TestTrue(TEXT("Password allows long passphrase"), FRatwAccounts::ValidPassword(TEXT("a unique test passphrase")));
    TestFalse(TEXT("Reject short password"), FRatwAccounts::ValidPassword(TEXT("short")));
    TestFalse(TEXT("Reject control in password"), FRatwAccounts::ValidPassword(TEXT("long enough\npassword")));
    TestFalse(TEXT("Bound password bytes"), FRatwAccounts::ValidPassword(FString::ChrN(129, 'x')));
    TestTrue(TEXT("Unicode display name"), FRatwAccounts::ValidDisplayName(TEXT("Élan of Rain")));
    TestFalse(TEXT("Name cannot carry multiline control"), FRatwAccounts::ValidDisplayName(TEXT("Ash\nWolf")));
    TestFalse(TEXT("Name is trimmed before validation"), FRatwAccounts::ValidDisplayName(TEXT(" Ash ")));
    TestTrue(TEXT("Safe request identity"), FRatwAccounts::ValidCommandId(TEXT("create-a1_b2")));
    TestFalse(TEXT("No path-shaped request identity"), FRatwAccounts::ValidCommandId(TEXT("../create")));
    for (const auto* Address : {TEXT("127.0.0.1"), TEXT("127.2.3.4"), TEXT("::1"), TEXT("[::1]"), TEXT("::ffff:127.0.0.1")})
        TestTrue(TEXT("Recognize numeric loopback only"), FRatwAccounts::IsLoopbackAddress(Address));
    for (const auto* Address : {TEXT(""), TEXT("localhost"), TEXT("192.168.1.2"), TEXT("0.0.0.0"), TEXT("127.0.0.1.evil"), TEXT("127.0.0.1:7787"), TEXT("127.0.0.256"), TEXT("127.0.0.01"), TEXT("::ffff:192.168.1.2")})
        TestFalse(TEXT("Do not trust remote or malformed peer"), FRatwAccounts::IsLoopbackAddress(Address));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwAccountCredentialsTest, "RATW.Accounts.SaltedVerifierAndOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwAccountCredentialsTest::RunTest(const FString&)
{
    using namespace ratwjson;
    FRatwAccounts Accounts;
    FString Error;
    const FString Password = TEXT("test-only-account-password");
    TestTrue(TEXT("Register first account"), Accounts.Register(TEXT("Ash"), Password, Error));
    TestTrue(TEXT("Register second account"), Accounts.Register(TEXT("birch"), Password, Error));
    TestFalse(TEXT("Cannot recycle normalized account"), Accounts.Register(TEXT("ASH"), Password, Error));
    TestTrue(TEXT("Correct password authenticates"), Accounts.Authenticate(TEXT("ASH"), Password));
    TestFalse(TEXT("Wrong password rejected"), Accounts.Authenticate(TEXT("ash"), TEXT("wrong-but-long-password")));
    TestFalse(TEXT("Unknown account rejected"), Accounts.Authenticate(TEXT("unknown"), Password));
    const auto Entries = Items(Accounts.State(), TEXT("entries"));
    TestEqual(TEXT("600k PBKDF2 iterations"), Number(Entries[0]->AsObject(), TEXT("iterations")), 600000.0);
    TestNotEqual(TEXT("Fresh random salts"), String(Entries[0]->AsObject(), TEXT("salt")), String(Entries[1]->AsObject(), TEXT("salt")));
    TestNotEqual(TEXT("Same password has different verifier"), String(Entries[0]->AsObject(), TEXT("verifier")), String(Entries[1]->AsObject(), TEXT("verifier")));
    TestFalse(TEXT("No plaintext password in saved payload"), Encode(Accounts.State()).Contains(Password));
    const FString Id = TEXT("wolf-0123456789abcdef0123456789abcdef");
    const FString Fingerprint = FRatwAccounts::Fingerprint(TEXT("first choices"));
    TestTrue(TEXT("Link generated owned character"), Accounts.AddCharacter(TEXT("ash"), Id, TEXT("create-1"), Fingerprint));
    TestTrue(TEXT("Owner can select"), Accounts.Owns(TEXT("ash"), Id));
    TestFalse(TEXT("Another account cannot select guessed ID"), Accounts.Owns(TEXT("birch"), Id));
    TestFalse(TEXT("Another account cannot relink ID"), Accounts.AddCharacter(TEXT("birch"), Id, TEXT("other"), Fingerprint));
    bool Conflict = false;
    TestEqual(TEXT("Creation retry returns same character"), Accounts.CreatedCharacter(TEXT("ash"), TEXT("create-1"), Fingerprint, Conflict), Id);
    TestFalse(TEXT("Same payload is not a conflict"), Conflict);
    TestTrue(TEXT("Changed payload gets no replay"), Accounts.CreatedCharacter(TEXT("ash"), TEXT("create-1"), FRatwAccounts::Fingerprint(TEXT("different choices")), Conflict).IsEmpty());
    TestTrue(TEXT("Recycled request ID reports conflict"), Conflict);
    for (int32 I = 1; I < 6; ++I)
        TestTrue(TEXT("Fill available slots"), Accounts.AddCharacter(TEXT("ash"), FString::Printf(TEXT("wolf-%032x"), I), FString::Printf(TEXT("create-%d"), I + 1), Fingerprint));
    TestEqual(TEXT("Six slots"), Accounts.Characters(TEXT("ash")).Num(), 6);
    TestFalse(TEXT("Seventh rejected"), Accounts.AddCharacter(TEXT("ash"), TEXT("wolf-ffffffffffffffffffffffffffffffff"), TEXT("seventh"), Fingerprint));
    TestFalse(TEXT("Development identity cannot be claimed"), Accounts.AddCharacter(TEXT("birch"), TEXT("player-ash"), TEXT("forged"), Fingerprint));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwAccountCheckpointTest, "RATW.Accounts.AtomicCheckpointAndStrictRestore",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwAccountCheckpointTest::RunTest(const FString&)
{
    using namespace ratwjson;
    FRatwAccounts Accounts;
    FString Error;
    TestTrue(TEXT("Register isolated account"), Accounts.Register(TEXT("archive"), TEXT("test-only-checkpoint-password"), Error));
    const FString Id = TEXT("wolf-0123456789abcdef0123456789abcdef");
    TestTrue(TEXT("Own saved character"), Accounts.AddCharacter(TEXT("archive"), Id, TEXT("creation"), FRatwAccounts::Fingerprint(TEXT("choice"))));
    auto Root = New(); Root->SetObjectField(TEXT("accounts"), Accounts.State());
    Root->SetStringField(TEXT("characterId"), Id);
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("Tests") / FGuid::NewGuid().ToString();
    const FString Path = Directory / TEXT("accounts.sqlite");
    {
        FRatwPersistence Storage;
        TestTrue(TEXT("Open checkpoint"), Storage.Open(Path));
        TestTrue(TEXT("One durable transaction for character and ownership"), Storage.Save(Encode(Root), 1));
    }
    FRatwAccounts Restored;
    {
        FRatwPersistence Storage;
        TestTrue(TEXT("Reopen checkpoint"), Storage.Open(Path));
        const auto Saved = Decode(Storage.Load());
        TestTrue(TEXT("Restore verifier/ownership"), Restored.Restore(Child(Saved, TEXT("accounts"))));
        TestTrue(TEXT("Character reference agrees"), Restored.Owns(TEXT("archive"), String(Saved, TEXT("characterId"))));
    }
    TestTrue(TEXT("Password survives restart"), Restored.Authenticate(TEXT("archive"), TEXT("test-only-checkpoint-password")));
    TestTrue(TEXT("Known character reference accepted"), Restored.ReferencesOnly({Id}));
    TestFalse(TEXT("Missing character reference fails closed"), Restored.ReferencesOnly({}));
    TestTrue(TEXT("Legacy development character may coexist unowned"), Restored.ReferencesOnly({Id, TEXT("player-ash")}));
    TestFalse(TEXT("An extra orphan generated character fails closed"), Restored.ReferencesOnly({Id, TEXT("wolf-ffffffffffffffffffffffffffffffff")}));
    TestFalse(TEXT("Malformed reserved generated ID fails closed"), Restored.ReferencesOnly({Id, TEXT("wolf-invalid")}));
    FRatwAccounts NoAccounts;
    TestTrue(TEXT("Legacy save without accounts may contain development characters"), NoAccounts.ReferencesOnly({TEXT("player-ash")}));
    TestFalse(TEXT("Legacy omission cannot strand an owned-namespace character"), NoAccounts.ReferencesOnly({Id}));
    auto Corrupt = Decode(Encode(Accounts.State()));
    Items(Corrupt, TEXT("entries"))[0]->AsObject()->SetNumberField(TEXT("iterations"), 1);
    TestFalse(TEXT("Weak/malformed iteration count rejected"), Restored.Restore(Corrupt));
    TestTrue(TEXT("Failed restore does not overwrite ownership"), Restored.Owns(TEXT("archive"), Id));
    Corrupt = Decode(Encode(Accounts.State()));
    Items(Corrupt, TEXT("entries"))[0]->AsObject()->SetStringField(TEXT("salt"), TEXT("not-a-salt"));
    TestFalse(TEXT("Invalid salt rejects"), Restored.Restore(Corrupt));
    Corrupt = Decode(Encode(Accounts.State()));
    Items(Corrupt, TEXT("entries"))[0]->AsObject()->SetStringField(TEXT("password"), TEXT("not permitted"));
    TestFalse(TEXT("Unknown secret field rejects"), Restored.Restore(Corrupt));
    Corrupt = Decode(Encode(Accounts.State()));
    auto Entries = Items(Corrupt, TEXT("entries"));
    auto Duplicate = Decode(Encode(Entries[0]->AsObject())); Duplicate->SetStringField(TEXT("username"), TEXT("duplicate"));
    Entries.Add(V(Duplicate)); Corrupt->SetArrayField(TEXT("entries"), Entries);
    TestFalse(TEXT("Duplicate owned ID across accounts rejects"), Restored.Restore(Corrupt));
    IFileManager::Get().DeleteDirectory(*Directory, false, true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwAccountThrottleTest, "RATW.Accounts.BoundedRateLimit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwAccountThrottleTest::RunTest(const FString&)
{
    FRatwAccountRateLimit Limit;
    for (int32 I = 0; I < 6; ++I) TestTrue(TEXT("Connection initial allowance"), Limit.Allow(TEXT("one"), 100));
    TestFalse(TEXT("Seventh same-minute attempt rejected"), Limit.Allow(TEXT("one"), 101));
    for (int32 I = 0; I < 18; ++I) TestTrue(TEXT("Other connections share remaining global allowance"), Limit.Allow(FString::FromInt(I), 101));
    TestFalse(TEXT("Reconnect cannot bypass global PBKDF2 budget"), Limit.Allow(TEXT("new-peer"), 102));
    Limit.Forget(TEXT("one"));
    TestFalse(TEXT("Disconnect does not reset global budget"), Limit.Allow(TEXT("one"), 102));
    TestTrue(TEXT("Window expires using real monotonic time"), Limit.Allow(TEXT("one"), 162));
    return true;
}
#endif
