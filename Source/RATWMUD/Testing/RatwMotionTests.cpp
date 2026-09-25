#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "UI/RatwMotionBuffer.h"
#include "UI/SRatwGame.h"
#include "Runtime/RatwMotion.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwMotionBufferTest, "RATW.Movement.BufferedFrameInterpolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwMotionBufferTest::RunTest(const FString&)
{
    for (const int32 Hz : {30, 60, 144})
    {
        FRatwMotionBuffer Buffer;
        int32 Next = 0;
        double MaxError = 0, MaxSpeedError = 0, Previous = 0;
        for (int32 Frame = 0; Frame <= Hz * 4; ++Frame)
        {
            const double Now = double(Frame) / Hz;
            // Alternating 20ms arrival jitter and one lost packet, covered by
            // the 100ms render buffer. Simulation samples themselves are 20Hz.
            while (Next * .05 + (Next % 2 ? .02 : 0) <= Now + 1.e-9)
            {
                if (Next != 31) Buffer.Add(Next * .05, FVector2D(Next * .15, 2), Next * .025);
                ++Next;
            }
            const auto Pose = Buffer.At(Now - .1);
            if (Now > .5)
            {
                MaxError = FMath::Max(MaxError, FMath::Abs(Pose.Position.X - (Now - .1) * 3));
                MaxSpeedError = FMath::Max(MaxSpeedError, FMath::Abs((Pose.Position.X - Previous) * Hz - 3));
            }
            Previous = Pose.Position.X;
        }
        TestTrue(*FString::Printf(TEXT("%dHz: constant speed despite jitter and a lost packet"), Hz), MaxSpeedError < 1.e-8);
        TestTrue(*FString::Printf(TEXT("%dHz: accurate buffered position"), Hz), MaxError < 1.e-8);
        TestTrue(TEXT("History remains bounded"), Buffer.Samples.Num() <= 32);
    }
    FRatwMotionBuffer Buffer;
    Buffer.Add(0, FVector2D(0, 0), 3.1);
    Buffer.Add(.05, FVector2D(1, 0), -3.1);
    TestTrue(TEXT("Facing crosses pi by the short arc"), Buffer.At(.025).Facing > 3.1);
    TestFalse(TEXT("Reordered pose rejected"), Buffer.Add(.02, FVector2D(99, 99), 0));
    TestFalse(TEXT("Duplicate pose rejected"), Buffer.Add(.05, FVector2D(99, 99), 0));
    Buffer.Add(.1, FVector2D(1, 0), -3.1);
    TestEqual(TEXT("Stop holds exact authority position"), Buffer.At(.1).Position.X, 1.);
    TestEqual(TEXT("Packet drought cannot extrapolate through walls"), Buffer.At(50).Position.X, 1.);
    Buffer.Add(1, FVector2D(2, 0), 0);
    TestEqual(TEXT("Long interruption resets stale history"), Buffer.Samples.Num(), 1);
    Buffer.Add(1.05, FVector2D(20, 0), 0);
    TestEqual(TEXT("Teleport resets without sliding through obstacles"), Buffer.At(1.01).Position.X, 20.);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwUIMotionTest, "RATW.UI.MotionVisibilityAndTransitions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwUIMotionTest::RunTest(const FString&)
{
    using namespace ratwjson;
    TSharedRef<SRatwGame> UI = SNew(SRatwGame);
    auto Self = New(); Self->SetStringField(TEXT("id"), TEXT("self"));
    Self->SetNumberField(TEXT("x"), 5); Self->SetNumberField(TEXT("y"), 5);
    auto Other = New(); Other->SetStringField(TEXT("id"), TEXT("other"));
    Other->SetNumberField(TEXT("x"), 6); Other->SetNumberField(TEXT("y"), 5);
    auto Cell = New(); Cell->SetStringField(TEXT("id"), TEXT("room"));
    auto Snapshot = New(); Snapshot->SetObjectField(TEXT("cell"), Cell);
    Snapshot->SetObjectField(TEXT("self"), Self);
    Snapshot->SetArrayField(TEXT("entities"), {V(Other)});
    Snapshot->SetNumberField(TEXT("time"), 0); Snapshot->SetNumberField(TEXT("cellGeneration"), 1);
    UI->ApplySnapshot(Snapshot);
    auto Motion = New(); Motion->SetStringField(TEXT("cellId"), TEXT("room"));
    Motion->SetNumberField(TEXT("cellGeneration"), 1); Motion->SetNumberField(TEXT("time"), .05);
    Self->SetNumberField(TEXT("x"), 5.15);
    Motion->SetArrayField(TEXT("entities"), {V(Self)});
    UI->ApplyMotion(Motion);
    TestFalse(TEXT("Hidden wolf removed immediately, not after interpolation"), UI->EntityViews.Contains(TEXT("other")));
    UI->ApplySnapshot(Snapshot);
    TestFalse(TEXT("Delayed metadata cannot resurrect a hidden wolf"), UI->EntityViews.Contains(TEXT("other")));
    TestEqual(TEXT("Delayed metadata cannot rewind a pose"), UI->EntityViews[TEXT("self")].Target.X, 5.15);
    Motion->SetNumberField(TEXT("time"), .1); Motion->SetNumberField(TEXT("cellGeneration"), 2);
    Self->SetNumberField(TEXT("x"), 8);
    UI->ApplyMotion(Motion);
    TestEqual(TEXT("New room motion waits for matching scene metadata"), UI->EntityViews[TEXT("self")].Target.X, 5.15);
    Snapshot->SetNumberField(TEXT("time"), .1); Snapshot->SetNumberField(TEXT("cellGeneration"), 2);
    UI->ApplySnapshot(Snapshot);
    TestEqual(TEXT("Same named room in a new generation snaps at entry"), UI->EntityViews[TEXT("self")].Position.X, 8.);
    TestEqual(TEXT("No old room motion remains"), UI->EntityViews[TEXT("self")].Motion.Samples.Num(), 1);
    // Feed the real widget at 60Hz and verify its output, not just helper math.
    const auto Geometry = FGeometry::MakeRoot(FVector2D(1600, 1000), FSlateLayoutTransform());
    int32 Next = 3;
    double Previous = 8, MaxSpeedError = 0;
    for (int32 Frame = 1; Frame <= 180; ++Frame)
    {
        UI->Tick(Geometry, Frame / 60., 1.f / 60);
        const double Now = .1 + Frame / 60.;
        while (Next * .05 <= Now + 1.e-9)
        {
            Self->SetNumberField(TEXT("x"), 8 + (Next * .05 - .1) * 3);
            Motion->SetNumberField(TEXT("time"), Next++ * .05);
            UI->ApplyMotion(Motion);
        }
        const double Position = UI->EntityViews[TEXT("self")].Position.X;
        if (Frame > 40) MaxSpeedError = FMath::Max(MaxSpeedError, FMath::Abs((Position - Previous) * 60 - 3));
        Previous = Position;
    }
    TestTrue(TEXT("Actual Slate tick has no recurring acceleration/deceleration"), MaxSpeedError < .0001);
    UI->Tick(Geometry, 4, 1.f);
    const double ResumedTime = UI->LatestMotionTime + .05;
    Self->SetNumberField(TEXT("x"), 17.15);
    Motion->SetNumberField(TEXT("time"), ResumedTime);
    UI->ApplyMotion(Motion);
    TestEqual(TEXT("Resuming after a local stall rebases the timeline"), UI->EntityViews[TEXT("self")].Motion.Samples.Num(), 1);
    TestTrue(TEXT("Resumed stream again has interpolation headroom"),
        FMath::IsNearlyEqual(UI->MotionClock - UI->MotionOffset, ResumedTime, 1.e-8));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwMotionPrivacyTest, "RATW.Network.MotionPerceptionPrivacy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwMotionPrivacyTest::RunTest(const FString&)
{
    using namespace ratwjson;
    ratw::World World;
    World.addPlayer("observer", "Observer");
    auto& Other = World.addPlayer("hidden", "Hidden");
    Other.cellId = "exterior";
    Other.path.push_back({30, 30});
    const auto Hidden = Encode(ratwmotion::Frame(World, "observer"));
    TestFalse(TEXT("Cross-cell actors absent, including their IDs"), Hidden.Contains(TEXT("hidden")));
    Other.cellId = World.entity("observer")->cellId;
    Other.position = World.entity("observer")->position;
    const auto Visible = Encode(ratwmotion::Frame(World, "observer"));
    TestTrue(TEXT("Visible wolves have lightweight poses"), Visible.Contains(TEXT("hidden")));
    TestFalse(TEXT("Future paths are never transmitted"), Visible.Contains(TEXT("path")));
    TestFalse(TEXT("Input is never transmitted"), Visible.Contains(TEXT("input")));
    World.entity("observer")->eyeHealth = 0;
    // At zero distance vision can still permit self/overlap. Move away first.
    Other.position.x += 10;
    TestFalse(TEXT("Loss of sight removes exact location from stream"), Encode(ratwmotion::Frame(World, "observer")).Contains(TEXT("hidden")));
    return true;
}
#endif
