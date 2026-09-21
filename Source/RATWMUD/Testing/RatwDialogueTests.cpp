#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "Runtime/RatwDialogueProvider.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwDialogueOfflineTest, "RATW.Dialogue.AuthoredFallback",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwDialogueOfflineTest::RunTest(const FString&)
{
    FRatwDialogueProvider Provider;
    TestEqual(TEXT("Default provider is truthfully labeled"), Provider.Label(),
              FString(TEXT("Authored offline dialogue")));
    Provider.Configure(TEXT("https://example.invalid/paid-provider"));
    TestEqual(TEXT("Remote endpoint not silently used"), Provider.Label(), FString(TEXT("Authored offline dialogue")));
    for (const FString Bad :
         {TEXT("http://127.0.0.1:8080@remote.example/dialogue"), TEXT("http://localhost:8080\\@remote.example/"),
          TEXT("http://127.0.0.1:8080\n/"), TEXT("http://localhost:99999/dialogue"),
          TEXT("http://localhost:80evil/dialogue"), TEXT("http://127.0.0.1.evil:80/")})
    {
        Provider.Configure(Bad);
        TestEqual(TEXT("Malformed/local-prefix bypass endpoint rejected"), Provider.Label(),
                  FString(TEXT("Authored offline dialogue")));
    }
    FRatwDialogueContext Context;
    Context.NpcId = TEXT("npc_keeper");
    Context.Name = TEXT("Rowan");
    Context.PlayerName = TEXT("Ash");
    Context.HeardText = TEXT("Do you remember?");
    Context.Memory = TEXT("I promised to return the blue scarf.");
    FString Reply;
    int Count = 0;
    Provider.Reply(Context, [&](FString Text) {
        Reply = Text;
        ++Count;
    });
    TestEqual(TEXT("Offline reply completes once"), Count, 1);
    TestTrue(TEXT("Memory recall includes supplied memory"), Reply.Contains(TEXT("blue scarf")));
    return true;
}

namespace
{
struct FDialogueFixtureState
{
    FProcHandle Process;
    FAutomationTestBase* Test = nullptr;
    FRatwDialogueProvider Provider;
    double Started = FPlatformTime::Seconds();
    bool Issued = false;
    int Replies = 0;
    TMap<FString, int> Counts;
    ~FDialogueFixtureState()
    {
        if (Process.IsValid())
        {
            if (FPlatformProcess::IsProcRunning(Process))
                FPlatformProcess::TerminateProc(Process);
            FPlatformProcess::WaitForProc(Process);
            FPlatformProcess::CloseProc(Process);
        }
    }
};
class FDialogueFixtureCommand : public IAutomationLatentCommand
{
  public:
    explicit FDialogueFixtureCommand(TSharedRef<FDialogueFixtureState> InState) : State(InState) {}
    virtual bool Update() override
    {
        const double Elapsed = FPlatformTime::Seconds() - State->Started;
        if (!State->Issued && Elapsed > 0.5)
        {
            State->Issued = true;
            for (const FString Case : {TEXT("fixture_success"), TEXT("fixture_failure"), TEXT("fixture_malformed"),
                                       TEXT("fixture_empty"), TEXT("fixture_oversize"), TEXT("fixture_timeout")})
            {
                FRatwDialogueContext Context;
                Context.NpcId = TEXT("npc_keeper");
                Context.Name = TEXT("Rowan");
                Context.PlayerName = TEXT("Ash");
                Context.HeardText = Case;
                const FString Fallback = FRatwDialogueProvider::AuthoredReply(Context);
                auto Shared = State;
                State->Provider.Reply(Context, [Shared, Case, Fallback](FString Reply) {
                    ++Shared->Counts.FindOrAdd(Case);
                    ++Shared->Replies;
                    Shared->Test->TestEqual(*(Case + TEXT(" callback exactly once")), Shared->Counts[Case], 1);
                    Shared->Test->TestEqual(*Case, Reply,
                                            Case == TEXT("fixture_success")
                                                ? FString(TEXT("A grounded reply from the local test provider."))
                                                : Fallback);
                });
            }
        }
        if (State->Replies >= 6)
            return true;
        if (Elapsed > 15)
        {
            State->Test->AddError(TEXT("Local dialogue HTTP fixture did not complete within fifteen seconds."));
            return true;
        }
        return false;
    }

  private:
    TSharedRef<FDialogueFixtureState> State;
};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwDialogueHttpTest, "RATW.Dialogue.LocalHttpContract",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwDialogueHttpTest::RunTest(const FString&)
{
    AddExpectedMessage(TEXT("HTTP request timed out after"), ELogVerbosity::Warning,
                       EAutomationExpectedMessageFlags::Contains, 1);
    auto State = MakeShared<FDialogueFixtureState>();
    State->Test = this;
    const FString Script = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("tools/provider_fixture.py"));
    const FString Args = FString::Printf(TEXT("\"%s\" --port 18765"), *Script);
    State->Process =
        FPlatformProcess::CreateProc(TEXT("/usr/bin/python3"), *Args, false, true, true, nullptr, 0, nullptr, nullptr);
    if (!State->Process.IsValid())
    {
        AddError(TEXT("Could not launch the loopback dialogue fixture."));
        return false;
    }
    State->Provider.Configure(TEXT("http://127.0.0.1:18765/dialogue"));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FDialogueFixtureCommand>(State));
    return true;
}
#endif
