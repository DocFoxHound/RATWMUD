#include "UI/SRatwGame.h"
#include "UI/SRatwFrontDoor.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Layout/Children.h"
#include "InputCoreTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SWindow.h"
#include <limits>

namespace RatwUI
{
FString PaceLabel(int32 Pace);
EOrientation GradientStopOrientation(EOrientation Axis);
} // namespace RatwUI

namespace
{
FKeyEvent Key(FKey Value, bool Shift = false)
{
    return FKeyEvent(Value, FModifierKeysState(Shift, false, false, false, false, false, false, false, false), 0, false,
                     0, 0);
}
FPointerEvent Pointer(FVector2D Point, bool Alt = false, bool Control = false, FKey Button = EKeys::Invalid)
{
    static const TSet<FKey> NoButtons;
    return FPointerEvent(0, Point, Point, NoButtons, Button, 0,
                         FModifierKeysState(false, false, Control, false, Alt, false, false, false, false));
}
TSharedPtr<SWidget> EditableChild(const TSharedRef<SWidget>& Widget)
{
    if (Widget->GetType() == FName(TEXT("SMultiLineEditableText")))
        return Widget;
    auto Children = Widget->GetChildren();
    for (int I = 0; I < Children->Num(); ++I)
    {
        const auto Result = EditableChild(Children->GetChildAt(I));
        if (Result)
            return Result;
    }
    return nullptr;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwUIInputTest, "RATW.UI.InputAndDraftRecovery",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwUIInputTest::RunTest(const FString&)
{
    TArray<FString> Commands;
    TSharedRef<SRatwGame> UI = SNew(SRatwGame).OnCommand([&](const FString& S) { Commands.Add(S); });
    const auto Geometry = FGeometry::MakeRoot(FVector2D(1600, 1000), FSlateLayoutTransform());
    UI->OnKeyDown(Geometry, Key(EKeys::Enter));
    TestTrue(TEXT("Enter begins composing"), UI->bChat);
    UI->Composer->SetText(FText::FromString(TEXT("The first line")));
    const auto Editor = EditableChild(UI->Composer.ToSharedRef());
    TestTrue(TEXT("A real multiline Slate editor is installed"), Editor.IsValid());
    if (Editor)
    {
        Editor->OnKeyDown(Geometry, Key(EKeys::End));
        Editor->OnKeyDown(Geometry, Key(EKeys::Enter, true));
        TestTrue(TEXT("Shift Enter inserts a real newline"), UI->Composer->GetText().ToString().Contains(TEXT("\n")));
        TestTrue(TEXT("Shift Enter keeps type mode"), UI->bChat);
    }
    const FString Preserved = UI->Composer->GetText().ToString();
    UI->ComposerKey(Geometry, Key(EKeys::Escape));
    TestFalse(TEXT("Escape returns navigation"), UI->bChat);
    TestEqual(TEXT("Escape preserves exact draft"), UI->Composer->GetText().ToString(), Preserved);
    UI->OnKeyDown(Geometry, Key(EKeys::Enter));
    UI->Composer->SetText(FText::FromString(TEXT("A retained post.")));
    UI->ComposerKey(Geometry, Key(EKeys::Enter));
    TestFalse(TEXT("Sending returns to movement immediately"), UI->bChat);
    TestTrue(TEXT("Submitted text retained awaiting server acceptance"), UI->PendingDrafts.Num() == 1);
    bool Sent = false;
    for (const auto& C : Commands)
        if (C.Contains(TEXT("\"type\":\"chat\"")) && C.Contains(TEXT("A retained post.")))
            Sent = true;
    TestTrue(TEXT("Sending emits the text command"), Sent);
    auto Reject = MakeShared<FJsonObject>();
    Reject->SetStringField(TEXT("type"), TEXT("error"));
    Reject->SetStringField(TEXT("context"), TEXT("chat"));
    Reject->SetStringField(TEXT("requestId"), TEXT("post_1"));
    Reject->SetStringField(TEXT("text"), TEXT("Test rejection"));
    UI->ReceiveEvent(Reject);
    TestEqual(TEXT("A rejected post restores the submitted draft"), UI->Composer->GetText().ToString(),
              FString(TEXT("A retained post.")));
    UI->OnKeyDown(Geometry, Key(EKeys::W));
    TestTrue(TEXT("W supplies movement in navigation mode"), UI->HeldKeys.Contains(EKeys::W));
    UI->OnKeyUp(Geometry, Key(EKeys::W));
    TestTrue(TEXT("Release stops movement"), UI->HeldKeys.IsEmpty());
    UI->OnKeyDown(Geometry, Key(EKeys::Enter));
    UI->OnKeyDown(Geometry, Key(EKeys::W));
    TestTrue(TEXT("Typing W does not move"), UI->HeldKeys.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwUIRevealTest, "RATW.UI.SequentialRoleplayReveal",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwUIRevealTest::RunTest(const FString&)
{
    TSharedRef<SRatwGame> UI = SNew(SRatwGame).OnCommand([](const FString&) {});
    const auto Geometry = FGeometry::MakeRoot(FVector2D(1600, 1000), FSlateLayoutTransform());
    for (int I = 0; I < 2; ++I)
    {
        auto P = MakeShared<FJsonObject>();
        P->SetStringField(TEXT("type"), TEXT("roleplay"));
        P->SetStringField(TEXT("id"), FString::FromInt(I));
        P->SetStringField(TEXT("speaker"), TEXT("Visible speaker"));
        P->SetStringField(TEXT("text"), TEXT("Five."));
        P->SetStringField(TEXT("channel"), TEXT("ic"));
        UI->ReceiveEvent(P);
    }
    UI->Tick(Geometry, 1, .04f);
    TestTrue(TEXT("First entry starts revealing"), UI->Posts[0].Revealed > 0);
    TestEqual(TEXT("Second entry remains queued"), UI->Posts[1].Revealed, 0);
    UI->Tick(Geometry, 1.1, .1f);
    TestEqual(TEXT("First entry completes"), UI->Posts[0].Revealed, 5);
    TestEqual(TEXT("Posts never reveal concurrently"), UI->Posts[1].Revealed, 0);
    UI->Tick(Geometry, 1.2, .1f);
    TestEqual(TEXT("Second begins after first finishes"), UI->Posts[1].Revealed, 5);
    auto Mixed = MakeShared<FJsonObject>();
    Mixed->SetStringField(TEXT("type"), TEXT("roleplay"));
    Mixed->SetStringField(TEXT("speaker"), TEXT("A voice"));
    Mixed->SetStringField(TEXT("text"), TEXT("flattened fallback"));
    auto Speech = MakeShared<FJsonObject>();
    Speech->SetStringField(TEXT("kind"), TEXT("speech"));
    Speech->SetStringField(TEXT("text"), TEXT("The road is quiet."));
    auto Action = MakeShared<FJsonObject>();
    Action->SetStringField(TEXT("kind"), TEXT("action"));
    Action->SetStringField(TEXT("text"), TEXT("sighs softly."));
    Mixed->SetArrayField(TEXT("segments"),
                         {MakeShared<FJsonValueObject>(Speech), MakeShared<FJsonValueObject>(Action)});
    UI->ReceiveEvent(Mixed);
    TestEqual(TEXT("Typed speech retains quotation marks beside actions"), UI->Posts.Last().Text,
              FString(TEXT("\"The road is quiet.\" sighs softly.")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwUILayoutTest, "RATW.UI.AdjustablePaneBalance",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwUILayoutTest::RunTest(const FString&)
{
    int Commands = 0;
    TSharedRef<SRatwGame> UI = SNew(SRatwGame).OnCommand([&](const FString&) { ++Commands; });
    UI->Composer->SetText(FText::FromString(TEXT("A draft that survives layout changes.\nAnother paragraph.")));
    const int Expected[] = {150, 300, -100, 0};
    for (int Value : Expected)
    {
        UI->Activate({FSlateRect(), TEXT("split"), TEXT("")});
        TestEqual(TEXT("The settings control cycles through four readable presets"), UI->StoryExtra, Value);
        TestTrue(TEXT("Narrative stays at least 425 reference pixels wide"), 525 + UI->StoryExtra >= 425);
        TestTrue(TEXT("Map stays at least 660 reference pixels wide"), 960 - UI->StoryExtra >= 660);
        TestEqual(TEXT("Changing balance preserves multiline draft"), UI->Composer->GetText().ToString(),
                  FString(TEXT("A draft that survives layout changes.\nAnother paragraph.")));
    }
    TestEqual(TEXT("Layout preferences do not mutate server state"), Commands, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwUIFacingTest, "RATW.UI.StationaryFacingPreview",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwUIFacingTest::RunTest(const FString&)
{
    TArray<FString> Commands;
    TSharedRef<SRatwGame> UI = SNew(SRatwGame).OnCommand([&](const FString& S) { Commands.Add(S); });
    const auto Geometry = FGeometry::MakeRoot(FVector2D(1600, 1000), FSlateLayoutTransform());
    auto Self = MakeShared<FJsonObject>();
    Self->SetStringField(TEXT("id"), TEXT("player"));
    Self->SetNumberField(TEXT("x"), 5);
    Self->SetNumberField(TEXT("y"), 5);
    Self->SetNumberField(TEXT("facing"), PI / 2.);
    Self->SetBoolField(TEXT("moving"), false);
    auto Cell = MakeShared<FJsonObject>();
    Cell->SetStringField(TEXT("id"), TEXT("test_cell"));
    Cell->SetNumberField(TEXT("width"), 32);
    Cell->SetNumberField(TEXT("height"), 24);
    auto Snapshot = MakeShared<FJsonObject>();
    Snapshot->SetObjectField(TEXT("self"), Self);
    Snapshot->SetObjectField(TEXT("cell"), Cell);
    auto PublicSelf = MakeShared<FJsonObject>();
    PublicSelf->SetStringField(TEXT("id"), TEXT("player"));
    PublicSelf->SetNumberField(TEXT("x"), 5);
    PublicSelf->SetNumberField(TEXT("y"), 5);
    PublicSelf->SetNumberField(TEXT("facing"), PI / 2.);
    PublicSelf->SetBoolField(TEXT("moving"), false);
    Snapshot->SetArrayField(TEXT("entities"), {MakeShared<FJsonValueObject>(PublicSelf)});
    UI->ApplySnapshot(Snapshot);
    UI->MapOrigin = FVector2D(600, 200);
    UI->MapRect = FSlateRect(584, 199, 1544, 816);
    UI->TileSize = 20;
    const FVector2D East(800, 300), North(700, 250);
    UI->OnMouseMove(Geometry, Pointer(East, true));
    TestTrue(TEXT("Alt mouse movement previews stationary facing"), UI->bFacingPreview);
    TestTrue(TEXT("East preview is zero radians in map coordinates"), FMath::IsNearlyZero(UI->PreviewFacing));
    TestEqual(TEXT("A preview sends no command"), Commands.Num(), 0);
    TestTrue(TEXT("A preview cannot rotate the actual marker"),
             FMath::IsNearlyEqual(UI->EntityViews[TEXT("player")].Facing, PI / 2.));
    UI->OnMouseMove(Geometry, Pointer(North, true));
    // UE_PI is a float constant, whereas the pointer angle is computed in double precision.
    TestTrue(TEXT("Screen north is negative Y and negative pi/2"),
             FMath::IsNearlyEqual(UI->PreviewFacing, -PI / 2., 1.e-6));
    UI->Hits.Add({FSlateRect(790, 290, 810, 310), TEXT("target"), TEXT("door_or_wolf")});
    UI->OnMouseMove(Geometry, Pointer(East, true));
    UI->OnMouseButtonDown(Geometry, Pointer(East, true, false, EKeys::LeftMouseButton));
    TestEqual(TEXT("Alt click sends exactly one facing command"), Commands.Num(), 1);
    TestTrue(TEXT("Alt click takes priority over an entity or door hit"), UI->ContextTarget.IsEmpty());
    TSharedPtr<FJsonObject> Sent;
    TestTrue(TEXT("Facing command is valid JSON"),
             FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Commands.Last()), Sent));
    if (Sent)
    {
        TestEqual(TEXT("Alt click sends face, never path"), Sent->GetStringField(TEXT("type")), FString(TEXT("face")));
        TestEqual(TEXT("Facing coordinates remove map origin and tile scale"), Sent->GetNumberField(TEXT("x")), 10.);
        TestEqual(TEXT("Facing coordinates preserve Y"), Sent->GetNumberField(TEXT("y")), 5.);
    }
    TestTrue(TEXT("Click waits for authoritative facing rather than snapping"),
             FMath::IsNearlyEqual(UI->EntityViews[TEXT("player")].Facing, PI / 2.));
    Self->SetNumberField(TEXT("facing"), .1);
    Snapshot->SetNumberField(TEXT("time"), .2);
    UI->ApplySnapshot(Snapshot);
    TestTrue(TEXT("A facing snapshot is an interpolation target, not a visual snap"),
             FMath::IsNearlyEqual(UI->EntityViews[TEXT("player")].Facing, PI / 2.));
    UI->Tick(Geometry, 1, .025f);
    TestTrue(TEXT("Rendering interpolates toward the server-facing angle"),
             UI->EntityViews[TEXT("player")].Facing > .1 && UI->EntityViews[TEXT("player")].Facing < PI / 2.);
    auto& FacingView = UI->EntityViews[TEXT("player")];
    FacingView.Motion.Samples.Empty();
    FacingView.Motion.Add(0, FVector2D(5, 5), 3.1);
    FacingView.Motion.Add(.2, FVector2D(5, 5), -3.1);
    UI->MotionClock = .1; UI->MotionOffset = 0;
    UI->Tick(Geometry, 1.1, .025f);
    TestTrue(TEXT("Angle interpolation uses the short arc across pi"), UI->EntityViews[TEXT("player")].Facing > 3.1);
    UI->OnKeyUp(Geometry, Key(EKeys::LeftAlt));
    TestFalse(TEXT("Releasing Alt clears the preview"), UI->bFacingPreview);
    UI->OnKeyDown(Geometry, Key(EKeys::LeftAlt));
    TestTrue(TEXT("Pressing Alt at the current cursor previews the direction"), UI->bFacingPreview);
    UI->OnKeyDown(Geometry, Key(EKeys::W));
    TestFalse(TEXT("Movement input immediately hides the preview"), UI->bFacingPreview);
    UI->OnMouseMove(Geometry, Pointer(East, true));
    TestFalse(TEXT("Held movement cannot recreate the preview"), UI->bFacingPreview);
    UI->OnKeyUp(Geometry, Key(EKeys::W));
    Self->SetBoolField(TEXT("moving"), true);
    UI->ApplySnapshot(Snapshot);
    int Before = Commands.Num();
    UI->OnMouseMove(Geometry, Pointer(East, true));
    UI->OnMouseButtonDown(Geometry, Pointer(East, true, false, EKeys::LeftMouseButton));
    TestFalse(TEXT("Authoritative self movement overrides sanitized public-row movement"), UI->bFacingPreview);
    TestEqual(TEXT("Alt click while moving cannot path or inspect"), Commands.Num(), Before);
    TestTrue(TEXT("Suppressed Alt click does not open a menu"), UI->ContextTarget.IsEmpty());
    Self->SetBoolField(TEXT("moving"), false);
    Self->SetNumberField(TEXT("postureRemaining"), .5);
    UI->ApplySnapshot(Snapshot);
    UI->OnMouseMove(Geometry, Pointer(East, true));
    TestFalse(TEXT("A pending posture transition is not stationary aiming"), UI->bFacingPreview);
    Self->SetNumberField(TEXT("postureRemaining"), 0);
    UI->ApplySnapshot(Snapshot);
    UI->bChat = true;
    UI->OnMouseMove(Geometry, Pointer(East, true));
    UI->OnMouseButtonDown(Geometry, Pointer(East, true, false, EKeys::LeftMouseButton));
    TestFalse(TEXT("Chat mode suppresses preview"), UI->bFacingPreview);
    TestTrue(TEXT("Alt map click cannot exit writing mode"), UI->bChat);
    TestEqual(TEXT("Chat mode suppresses facing commands"), Commands.Num(), Before);
    UI->bChat = false;
    UI->bWorldMap = true;
    UI->OnMouseMove(Geometry, Pointer(East, true));
    UI->OnMouseButtonDown(Geometry, Pointer(East, true, false, EKeys::LeftMouseButton));
    TestFalse(TEXT("World map suppresses preview"), UI->bFacingPreview);
    TestEqual(TEXT("World map cannot send local facing"), Commands.Num(), Before);
    UI->bWorldMap = false;
    UI->Modal = TEXT("character");
    UI->OnMouseMove(Geometry, Pointer(East, true));
    UI->OnMouseButtonDown(Geometry, Pointer(East, true, false, EKeys::LeftMouseButton));
    TestFalse(TEXT("A modal suppresses preview"), UI->bFacingPreview);
    TestEqual(TEXT("A modal suppresses facing commands"), Commands.Num(), Before);
    UI->Modal.Empty();
    UI->OnMouseMove(Geometry, Pointer(East, true));
    UI->OnFocusLost(FFocusEvent(EFocusCause::SetDirectly, 0));
    TestFalse(TEXT("Focus loss clears preview"), UI->bFacingPreview);
    Before = Commands.Num();
    UI->OnMouseMove(Geometry, Pointer(East, true));
    UI->OnMouseButtonDown(Geometry, Pointer(East, true, false, EKeys::LeftMouseButton));
    TestFalse(TEXT("Pointer motion without navigation focus cannot recreate preview"), UI->bFacingPreview);
    TestEqual(TEXT("An unfocused Alt click cannot issue a facing command"), Commands.Num(), Before);
    UI->OnFocusReceived(Geometry, FFocusEvent(EFocusCause::SetDirectly, 0));
    UI->OnMouseMove(Geometry, Pointer(East, true));
    UI->OnMouseLeave(Pointer(East, true));
    TestFalse(TEXT("Leaving the widget hides the preview"), UI->bFacingPreview);
    UI->OnMouseMove(Geometry, Pointer(East, true));
    Cell->SetStringField(TEXT("id"), TEXT("next_cell"));
    UI->ApplySnapshot(Snapshot);
    TestFalse(TEXT("Changing cells clears a stale preview"), UI->bFacingPreview);
    UI->OnMouseButtonDown(Geometry, Pointer(East, false, true, EKeys::LeftMouseButton));
    TestEqual(TEXT("Ctrl click remains a stationary-facing shortcut"), Commands.Num(), Before + 1);
    TestTrue(TEXT("Ctrl compatibility still emits face"), Commands.Last().Contains(TEXT("\"type\":\"face\"")));
    UI->Hits.Empty();
    UI->OnMouseButtonDown(Geometry, Pointer(East, false, false, EKeys::LeftMouseButton));
    TestTrue(TEXT("Unmodified map click remains click-to-path"), Commands.Last().Contains(TEXT("\"type\":\"path\"")));
    UI->OnMouseMove(Geometry, Pointer(North, true));
    TestFalse(TEXT("A pending click path blocks preview before the next snapshot"), UI->bFacingPreview);
    UI->ApplySnapshot(Snapshot);
    UI->CanvasScale = .5;
    UI->CanvasOffset = FVector2D(10, 20);
    UI->OnMouseMove(Geometry, Pointer(East * .5 + UI->CanvasOffset, true));
    TestTrue(TEXT("Preview respects letterboxing and canvas scale"), UI->bFacingPreview);
    TestTrue(TEXT("Scaled east remains the same preview angle"), FMath::IsNearlyZero(UI->PreviewFacing));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwUIScentTest, "RATW.UI.ScentAwareness",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwUIScentTest::RunTest(const FString&)
{
    TArray<FString> Commands;
    TSharedRef<SRatwGame> UI = SNew(SRatwGame).OnCommand([&](const FString& S) { Commands.Add(S); });
    auto Snapshot = MakeShared<FJsonObject>();
    auto Cell = MakeShared<FJsonObject>();
    auto Self = MakeShared<FJsonObject>();
    auto Wind = MakeShared<FJsonObject>();
    auto Senses = MakeShared<FJsonObject>();
    Cell->SetStringField(TEXT("id"), TEXT("scent_test"));
    Cell->SetBoolField(TEXT("outdoors"), true);
    Wind->SetNumberField(TEXT("direction"), 0);
    Wind->SetNumberField(TEXT("strength"), .5);
    Wind->SetBoolField(TEXT("variable"), true);
    Cell->SetObjectField(TEXT("wind"), Wind);
    Self->SetStringField(TEXT("id"), TEXT("self"));
    Self->SetStringField(TEXT("name"), TEXT("Self"));
    Self->SetNumberField(TEXT("x"), 5);
    Self->SetNumberField(TEXT("y"), 5);
    Snapshot->SetObjectField(TEXT("cell"), Cell);
    Snapshot->SetObjectField(TEXT("self"), Self);
    Snapshot->SetObjectField(TEXT("senses"), Senses);
    const auto Cue = [](double Sector, double Strength, bool Windborne) {
        auto C = MakeShared<FJsonObject>();
        C->SetNumberField(TEXT("sector"), Sector);
        C->SetNumberField(TEXT("strength"), Strength);
        C->SetBoolField(TEXT("windborne"), Windborne);
        // Unsupported metadata must never create a marker, name, or click target.
        C->SetStringField(TEXT("name"), TEXT("Unseen identity"));
        C->SetStringField(TEXT("id"), TEXT("hidden_wolf"));
        C->SetNumberField(TEXT("x"), 20);
        C->SetNumberField(TEXT("y"), 18);
        return MakeShared<FJsonValueObject>(C);
    };
    Senses->SetArrayField(TEXT("scentCues"), {Cue(4, 1, true), Cue(4, 3, true), Cue(-1, 2, true), Cue(8, 2, true),
                                              Cue(2.5, 2, true), Cue(2, 4, true), Cue(2, 0, true), Cue(2, 1.5, true),
                                              MakeShared<FJsonValueString>(TEXT("bad"))});
    Senses->SetBoolField(TEXT("movementHeard"), true);
    UI->ApplySnapshot(Snapshot);
    TestEqual(TEXT("Repeated sources merge into one sector; invalid sectors/intensities are rejected"),
              UI->ScentCues.Num(), 1);
    if (UI->ScentCues.Num() == 1)
    {
        TestEqual(TEXT("Strongest categorical cue wins without counting wolves"), UI->ScentCues[0].Strength, 3);
        TestEqual(TEXT("West is retained as a broad compass sector"), UI->ScentCues[0].Sector, 4);
        TestTrue(TEXT("Upwind is displayed only for an actual windborne cue"), UI->ScentCues[0].bWindborne);
    }
    TestTrue(TEXT("Hearing receives an anonymous pawstep presence flag"), UI->bMovementHeard);
    TestEqual(TEXT("Unseen scents are not made into visual entities"), UI->EntityViews.Num(), 1);
    TestFalse(TEXT("Scent metadata cannot reveal an unseen wolf's name"),
              UI->ScentLabel().Contains(TEXT("Unseen identity")));
    TestEqual(TEXT("Scent label is anonymous and approximate"), UI->ScentLabel(),
              FString(TEXT("SCENT · unseen wolf roughly W · upwind")));
    TestTrue(TEXT("Eastward air flow is labeled from west to east, not as a westward wind"),
             UI->WindLabel().Contains(TEXT("W -> E")));
    const auto Geometry = FGeometry::MakeRoot(FVector2D(1600, 1000), FSlateLayoutTransform());
    const TSharedRef<SWindow> Window = SNew(SWindow).ClientSize(FVector2D(1600, 1000));
    FSlateWindowElementList Elements(Window);
    UI->MapRect = FSlateRect(584, 199, 1544, 816);
    UI->DrawScent(Geometry, Elements, 0, FVector2D(1000, 500));
    UI->DrawScent(Geometry, Elements, 0, FVector2D(586, 500));
    TestTrue(TEXT("Scent arcs never register clickable targets, even at a map edge"), UI->Hits.IsEmpty());
    TestTrue(TEXT("Perception rendering sends no commands"), Commands.IsEmpty());
    UI->bWorldMap = true;
    UI->DrawScent(Geometry, Elements, 0, FVector2D(1000, 500));
    TestTrue(TEXT("World map receives no scent interaction targets"), UI->Hits.IsEmpty());
    UI->bWorldMap = false;
    TArray<TSharedPtr<FJsonValue>> Many;
    for (int I = 0; I < 64; ++I)
        Many.Add(Cue(I % 8, I % 3 + 1, true));
    Senses->SetArrayField(TEXT("scentCues"), Many);
    UI->ApplySnapshot(Snapshot);
    TestEqual(TEXT("At most eight anonymous directional hints can exist"), UI->ScentCues.Num(), 8);
    for (int I = 0; I < UI->ScentCues.Num(); ++I)
        TestEqual(TEXT("Sectors have stable compass order"), UI->ScentCues[I].Sector, I);
    Cell->SetBoolField(TEXT("outdoors"), false);
    UI->ApplySnapshot(Snapshot);
    TestEqual(TEXT("Indoor air is sheltered even if the input includes outdoor wind"), UI->WindStrength, 0.);
    TestEqual(TEXT("Indoor weather label does not show outdoor flow"), UI->WindLabel(),
              FString(TEXT("SHELTERED · still air")));
    TestFalse(TEXT("Sheltered scent does not claim to be upwind"), UI->ScentLabel().Contains(TEXT("upwind")));
    UI->Activate({FSlateRect(), TEXT("wind"), TEXT("east")});
    TestTrue(TEXT("Regular players cannot emit development wind controls"), Commands.IsEmpty());
    Snapshot->SetBoolField(TEXT("devTools"), true);
    UI->ApplySnapshot(Snapshot);
    UI->Activate({FSlateRect(), TEXT("wind"), TEXT("east")});
    TestEqual(TEXT("Development wind control emits one command"), Commands.Num(), 1);
    if (!Commands.IsEmpty())
        TestTrue(TEXT("Wind preset has a separate server command"),
                 Commands.Last().Contains(TEXT("\"type\":\"wind\"")) &&
                     Commands.Last().Contains(TEXT("\"value\":\"east\"")));
    Cell->SetBoolField(TEXT("outdoors"), true);
    Wind->SetNumberField(TEXT("strength"), 0);
    UI->ApplySnapshot(Snapshot);
    TestEqual(TEXT("Calm air never implies a direction"), UI->WindLabel(), FString(TEXT("AIR FLOW · calm")));
    TestFalse(TEXT("Calm air drops upwind wording"), UI->ScentLabel().Contains(TEXT("upwind")));
    Snapshot->RemoveField(TEXT("senses"));
    UI->ApplySnapshot(Snapshot);
    TestTrue(TEXT("Missing senses clear stale scent hints"), UI->ScentCues.IsEmpty());
    TestFalse(TEXT("Missing senses clear stale pawsteps"), UI->bMovementHeard);
    TestEqual(TEXT("No scent is not a promise that nobody is present"), UI->ScentLabel(),
              FString(TEXT("SCENT · no unseen scent detected")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwUIPaceTravelTest, "RATW.UI.PaceAndKnownTravel",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwUIPaceTravelTest::RunTest(const FString&)
{
    TestEqual(TEXT("Only notch zero is walking"), RatwUI::PaceLabel(0), FString(TEXT("WALK")));
    TestEqual(TEXT("First notch above walking is trotting"), RatwUI::PaceLabel(1), FString(TEXT("TROT")));
    TestEqual(TEXT("Second notch matches the server's trot band"), RatwUI::PaceLabel(2), FString(TEXT("TROT")));
    TestEqual(TEXT("Trot includes notch five"), RatwUI::PaceLabel(5), FString(TEXT("TROT")));
    TestEqual(TEXT("Run begins at notch six"), RatwUI::PaceLabel(6), FString(TEXT("RUN")));
    TestEqual(TEXT("Run includes notch eight"), RatwUI::PaceLabel(8), FString(TEXT("RUN")));
    TestEqual(TEXT("Sprint begins at notch nine"), RatwUI::PaceLabel(9), FString(TEXT("SPRINT")));
    TestEqual(TEXT("Maximum pace is sprinting"), RatwUI::PaceLabel(10), FString(TEXT("SPRINT")));
    TArray<FString> Commands;
    TSharedRef<SRatwGame> UI = SNew(SRatwGame).OnCommand([&](const FString& S) { Commands.Add(S); });
    const auto Geometry = FGeometry::MakeRoot(FVector2D(1600, 1000), FSlateLayoutTransform());
    const auto Self = MakeShared<FJsonObject>();
    Self->SetStringField(TEXT("id"), TEXT("wolf"));
    Self->SetNumberField(TEXT("pace"), 0);
    Self->SetNumberField(TEXT("stamina"), 42);
    Self->SetNumberField(TEXT("staminaRate"), -10);
    const auto Cell = MakeShared<FJsonObject>();
    Cell->SetStringField(TEXT("id"), TEXT("home"));
    const auto Snapshot = MakeShared<FJsonObject>();
    Snapshot->SetObjectField(TEXT("self"), Self);
    Snapshot->SetObjectField(TEXT("cell"), Cell);
    UI->ApplySnapshot(Snapshot);
    UI->MapRect = FSlateRect(584, 199, 1544, 816);
    auto Wheel = [](FVector2D P, float Delta, bool Shift = false, bool Control = false) {
        static const TSet<FKey> Buttons;
        return FPointerEvent(0, P, P, Buttons, EKeys::Invalid, Delta,
                             FModifierKeysState(Shift, false, Control, false, false, false, false, false, false));
    };
    UI->OnKeyDown(Geometry, Key(EKeys::PageUp));
    UI->OnKeyDown(Geometry, Key(EKeys::PageUp));
    TestEqual(TEXT("Rapid pace keys accumulate against pending request"), UI->DisplayPace(), 2);
    UI->ApplySnapshot(Snapshot);
    TestEqual(TEXT("Lagging server snapshot cannot erase pending pace"), UI->DisplayPace(), 2);
    UI->OnMouseWheel(Geometry, Wheel(FVector2D(900, 450), 1));
    TestEqual(TEXT("Local wheel increases requested pace"), UI->DisplayPace(), 3);
    for (int32 I = 0; I < 20; ++I)
        UI->OnKeyDown(Geometry, Key(EKeys::PageUp));
    TestEqual(TEXT("Pace clamps at sprint ceiling"), UI->DisplayPace(), 10);
    TestEqual(TEXT("Repeated max pace emits no redundant commands"), Commands.Num(), 10);
    TestTrue(TEXT("Pace command carries the absolute notch"), Commands.Last().Contains(TEXT("\"pace\":10")));
    TestTrue(TEXT("Pace never becomes a movement command"), Commands.Last().Contains(TEXT("\"type\":\"pace\"")));
    TestEqual(TEXT("Client input cannot change authoritative stamina"), Self->GetNumberField(TEXT("stamina")), 42.);
    int32 Before = Commands.Num();
    UI->OnMouseWheel(Geometry, Wheel(FVector2D(200, 450), 1));
    TestEqual(TEXT("Transcript wheel remains transcript scrolling"), UI->TranscriptScroll, 85);
    TestEqual(TEXT("Transcript wheel never changes pace"), Commands.Num(), Before);
    UI->OnMouseWheel(Geometry, Wheel(FVector2D(900, 450), 1, true));
    UI->OnMouseWheel(Geometry, Wheel(FVector2D(900, 450), 1, false, true));
    TestTrue(TEXT("Modified wheel preserves large-cell panning"), UI->MapPan.Equals(FVector2D(60, 60)));
    TestEqual(TEXT("Panning does not change pace"), Commands.Num(), Before);
    UI->OnMouseWheel(Geometry, Wheel(FVector2D(1500, 950), -1));
    TestEqual(TEXT("Wheel outside map never changes pace"), Commands.Num(), Before);
    Self->SetNumberField(TEXT("pace"), 10);
    UI->ApplySnapshot(Snapshot);
    TestEqual(TEXT("Matching server pace clears pending accumulator"), UI->RequestedPace, -1);
    UI->bChat = true;
    UI->Composer->SetText(FText::FromString(TEXT("A long roleplay draft.\nAnother paragraph.")));
    Before = Commands.Num();
    UI->OnKeyDown(Geometry, Key(EKeys::PageDown));
    UI->OnMouseWheel(Geometry, Wheel(FVector2D(900, 450), -1));
    UI->Activate({FSlateRect(), TEXT("pace"), TEXT("0")});
    TestEqual(TEXT("Chat keys, wheel, and stale pace hit cannot alter pace"), Commands.Num(), Before);
    TestTrue(TEXT("Multiline draft survives pace keys"), UI->Composer->GetText().ToString().Contains(TEXT("\n")));
    UI->bChat = false;
    UI->Modal = TEXT("settings");
    UI->OnKeyDown(Geometry, Key(EKeys::PageDown));
    UI->OnMouseWheel(Geometry, Wheel(FVector2D(900, 450), -1));
    UI->Activate({FSlateRect(), TEXT("pace"), TEXT("0")});
    TestEqual(TEXT("Modal screens suppress every pace entry point"), Commands.Num(), Before);
    UI->Modal.Empty();
    UI->OnKeyDown(Geometry, Key(EKeys::PageDown));
    UI->Clock += 2;
    TestEqual(TEXT("Unacknowledged request falls back to authoritative pace"), UI->DisplayPace(), 10);
    auto Visit = [](const TCHAR* Id, const TCHAR* Knowledge, double X) {
        auto O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("id"), Id);
        O->SetStringField(TEXT("name"), Id);
        O->SetStringField(TEXT("knowledge"), Knowledge);
        O->SetNumberField(TEXT("x"), X);
        O->SetNumberField(TEXT("width"), 32);
        O->SetNumberField(TEXT("height"), 24);
        return MakeShared<FJsonValueObject>(O);
    };
    Snapshot->SetArrayField(TEXT("travelMap"),
                            {Visit(TEXT("home"), TEXT("visited"), 0), Visit(TEXT("field"), TEXT("visited"), 32),
                             Visit(TEXT("outline"), TEXT("seen"), 64), Visit(TEXT("secret"), TEXT("unknown"), 96),
                             MakeShared<FJsonValueString>(TEXT("malformed"))});
    Snapshot->SetArrayField(TEXT("worldMap"), {Visit(TEXT("live_neighbor"), TEXT("visited"), 0)});
    const auto Travel = MakeShared<FJsonObject>();
    Travel->SetBoolField(TEXT("active"), true);
    Travel->SetStringField(TEXT("destination"), TEXT("field"));
    Snapshot->SetObjectField(TEXT("travel"), Travel);
    UI->ApplySnapshot(Snapshot);
    UI->SetPresentationPage(TEXT("travel"));
    TestTrue(TEXT("Travel presentation selects explicit known atlas"), UI->bWorldMap && UI->bTravelAtlas);
    TestTrue(TEXT("A remembered visited cell is a travel choice"), UI->CanTravelTo(TEXT("field")));
    TestFalse(TEXT("An outline cannot become a travel destination"), UI->CanTravelTo(TEXT("outline")));
    TestFalse(TEXT("Unvisited cells stay unavailable"), UI->CanTravelTo(TEXT("secret")));
    TestFalse(TEXT("Neighborhood data cannot stand in for visited atlas"), UI->CanTravelTo(TEXT("live_neighbor")));
    TestFalse(TEXT("Current cell is not a cross-cell travel target"), UI->CanTravelTo(TEXT("home")));
    const TSharedRef<SWindow> Window = SNew(SWindow).ClientSize(FVector2D(1600, 1000));
    FSlateWindowElementList Elements(Window);
    UI->Hits.Empty();
    UI->DrawTravelAtlas(Geometry, Elements, 0);
    int32 TravelHits = 0;
    for (const auto& Hit : UI->Hits)
        if (Hit.Action == TEXT("travel"))
        {
            ++TravelHits;
            TestEqual(TEXT("Only visited remote geometry receives travel hits"), Hit.Target, FString(TEXT("field")));
        }
    TestEqual(TEXT("Visited destination has map and list hit targets"), TravelHits, 2);
    auto OverlappingPlaces = Snapshot->GetArrayField(TEXT("travelMap"));
    OverlappingPlaces.Add(Visit(TEXT("shelter"), TEXT("visited"), 0));
    Snapshot->SetArrayField(TEXT("travelMap"), OverlappingPlaces);
    UI->ApplySnapshot(Snapshot);
    UI->Hits.Empty();
    UI->DrawTravelAtlas(Geometry, Elements, 0);
    int32 ShelterHits = 0, FieldHits = 0;
    for (const auto& Hit : UI->Hits)
        if (Hit.Action == TEXT("travel"))
        {
            ShelterHits += Hit.Target == TEXT("shelter") ? 1 : 0;
            FieldHits += Hit.Target == TEXT("field") ? 1 : 0;
        }
    TestEqual(TEXT("An interior sharing another cell's origin keeps its own map and list targets"), ShelterHits, 2);
    TestEqual(TEXT("Grouping overlapping labels does not remove unrelated travel choices"), FieldHits, 2);
    Before = Commands.Num();
    UI->Activate({FSlateRect(), TEXT("travel"), TEXT("secret")});
    TestEqual(TEXT("Stale or forged unvisited target is ignored"), Commands.Num(), Before);
    UI->Activate({FSlateRect(), TEXT("travel"), TEXT("field")});
    TestEqual(TEXT("Visited click emits one request"), Commands.Num(), Before + 1);
    TestTrue(TEXT("Travel command uses destination id and not teleport coordinates"),
             Commands.Last().Contains(TEXT("\"type\":\"travel\"")) &&
                 Commands.Last().Contains(TEXT("\"target\":\"field\"")));
    Before = Commands.Num();
    UI->OnMouseWheel(Geometry, Wheel(FVector2D(900, 450), -1));
    TestEqual(TEXT("Known-map wheel does not change movement pace"), Commands.Num(), Before);
    UI->OnKeyDown(Geometry, Key(EKeys::Escape));
    TestTrue(TEXT("Navigation Escape cancels route"), Commands.Last().Contains(TEXT("cancel_travel")));
    Before = Commands.Num();
    UI->bChat = true;
    UI->Activate({FSlateRect(), TEXT("travel"), TEXT("field")});
    UI->Activate({FSlateRect(), TEXT("cancel_travel"), TEXT("")});
    TestEqual(TEXT("Route clicks behind chat do not start or cancel travel"), Commands.Num(), Before);
    UI->OnKeyDown(Geometry, Key(EKeys::Escape));
    for (int32 I = Before; I < Commands.Num(); ++I)
        TestFalse(TEXT("Chat Escape never resets or cancels the overland journey"),
                  Commands[I].Contains(TEXT("cancel_travel")) || Commands[I].Contains(TEXT("\"type\":\"move\"")));
    TestTrue(TEXT("Chat Escape preserves the multiline draft during travel"),
             UI->Composer->GetText().ToString().Contains(TEXT("\n")));
    Before = Commands.Num();
    UI->bChat = false;
    UI->SetPresentationPage(TEXT("world"));
    TestTrue(TEXT("Ordinary world presentation remains neighborhood"), UI->bWorldMap && !UI->bTravelAtlas);
    UI->Activate({FSlateRect(), TEXT("travel"), TEXT("field")});
    TestEqual(TEXT("Old atlas hits cannot activate in neighborhood mode"), Commands.Num(), Before);
    Self->SetNumberField(TEXT("pace"), 9999);
    Self->SetNumberField(TEXT("stamina"), -1.e9);
    Self->SetNumberField(TEXT("staminaRate"), 1.e9);
    UI->Clock += 2;
    UI->ApplySnapshot(Snapshot);
    UI->DrawPace(Geometry, Elements, 0);
    TestEqual(TEXT("Malformed pace is bounded for safe rendering"), UI->DisplayPace(), 10);
    TestEqual(TEXT("Rendering malformed values sends no simulation command"), Commands.Num(), Before);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwUIWeatherTest, "RATW.UI.EnvironmentalPresentation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwUIWeatherTest::RunTest(const FString&)
{
    TestTrue(TEXT("Soft fog uses horizontal Slate stops for a vertical fade"),
             RatwUI::GradientStopOrientation(Orient_Vertical) == Orient_Horizontal);
    TestTrue(TEXT("Left-to-right lighting uses vertical Slate stops"),
             RatwUI::GradientStopOrientation(Orient_Horizontal) == Orient_Vertical);
    TArray<FString> Commands;
    const TSharedRef<SRatwGame> UI = SNew(SRatwGame).OnCommand([&](const FString& S) { Commands.Add(S); });
    const auto Snapshot = MakeShared<FJsonObject>(), Cell = MakeShared<FJsonObject>(), Env = MakeShared<FJsonObject>(),
               Wind = MakeShared<FJsonObject>(), Self = MakeShared<FJsonObject>();
    Cell->SetStringField(TEXT("id"), TEXT("weather_test"));
    Cell->SetBoolField(TEXT("outdoors"), true);
    Cell->SetStringField(TEXT("weather"), TEXT("rain"));
    Cell->SetArrayField(TEXT("tiles"), {MakeShared<FJsonValueString>(FString::ChrN(32, TEXT('.')))});
    Cell->SetObjectField(TEXT("environment"), Env);
    Cell->SetObjectField(TEXT("wind"), Wind);
    Env->SetNumberField(TEXT("hour"), 18.25);
    Env->SetStringField(TEXT("phase"), TEXT("dusk"));
    Env->SetNumberField(TEXT("daylight"), .35);
    Env->SetNumberField(TEXT("illumination"), .28);
    Env->SetNumberField(TEXT("sight"), .48);
    Env->SetNumberField(TEXT("hearing"), .62);
    Env->SetNumberField(TEXT("scent"), .5);
    Env->SetNumberField(TEXT("movement"), .85);
    // Weather metadata cannot become a visual source or identity.
    Env->SetStringField(TEXT("id"), TEXT("hidden_wolf"));
    Env->SetStringField(TEXT("name"), TEXT("Hidden identity"));
    Env->SetNumberField(TEXT("x"), 12);
    Wind->SetNumberField(TEXT("direction"), 0);
    Wind->SetNumberField(TEXT("strength"), .7);
    Self->SetStringField(TEXT("id"), TEXT("self"));
    Self->SetNumberField(TEXT("x"), 6);
    Self->SetNumberField(TEXT("y"), 5);
    Snapshot->SetObjectField(TEXT("cell"), Cell);
    Snapshot->SetObjectField(TEXT("self"), Self);
    Snapshot->SetArrayField(TEXT("visibility"), {MakeShared<FJsonValueString>(FString::ChrN(32, TEXT('1')))});
    UI->ApplySnapshot(Snapshot);
    UI->MapRect = FSlateRect(584, 199, 1544, 816);
    UI->MapOrigin = FVector2D(690, 230);
    UI->TileSize = 23;
    TestEqual(TEXT("Authoritative dusk is retained"), UI->Environment.Phase, FString(TEXT("dusk")));
    TestEqual(TEXT("Header includes authoritative local time and condition"), UI->EnvironmentLabel(),
              FString(TEXT("18:15 DUSK · RAIN")));
    TestTrue(TEXT("Condition summary uses server hearing and movement multipliers"),
             UI->EnvironmentEffectsLabel().Contains(TEXT("HEARING 62%")) &&
                 UI->EnvironmentEffectsLabel().Contains(TEXT("FOOTING 85%")));
    const auto Rain = UI->WeatherMarks();
    TestTrue(TEXT("Rain has multiple dense layers and ground splashes"), Rain.Num() > 100);
    TestTrue(TEXT("Rain follows eastward wind"), Rain.Num() && Rain[0].End.X > Rain[0].Position.X);
    TestTrue(TEXT("Rain includes static readable ground splash cues"),
             Rain.ContainsByPredicate([](const SRatwGame::FWeatherMark& M) { return M.bSplash; }));
    for (const auto& Mark : Rain)
    {
        TestTrue(TEXT("Rain starts inside the map, never over narrative or controls"),
                 UI->MapRect.ContainsPoint(Mark.Position));
        TestTrue(TEXT("Rain streak endpoints stay within the map"), UI->MapRect.ContainsPoint(Mark.End));
        TestTrue(TEXT("Rain is restricted to the cell, not empty map canvas"),
                 UI->CellBounds().ContainsPoint(Mark.Position) && UI->CellBounds().ContainsPoint(Mark.End));
        TestTrue(TEXT("Rain opacity remains translucent"), Mark.Alpha >= 0 && Mark.Alpha < .7f);
    }
    UI->Clock = 3;
    const auto MovingRain = UI->WeatherMarks();
    TestFalse(TEXT("Ordinary rain moves with time"), MovingRain[0].Position.Equals(Rain[0].Position));
    Wind->SetNumberField(TEXT("direction"), PI);
    UI->ApplySnapshot(Snapshot);
    const auto WestRain = UI->WeatherMarks();
    TestTrue(TEXT("Wind reversal reverses rain slant"), WestRain[0].End.X < WestRain[0].Position.X);
    UI->bReducedMotion = true;
    const auto StaticRain = UI->WeatherMarks();
    UI->Clock = 500;
    const auto LaterRain = UI->WeatherMarks();
    TestEqual(TEXT("Reduced motion retains rain and splash count"), LaterRain.Num(), StaticRain.Num());
    for (int I = 0; I < StaticRain.Num(); ++I)
        TestTrue(TEXT("Reduced-motion precipitation has no time-dependent geometry"),
                 StaticRain[I].Position.Equals(LaterRain[I].Position) && StaticRain[I].End.Equals(LaterRain[I].End) &&
                     StaticRain[I].Size == LaterRain[I].Size && StaticRain[I].Alpha == LaterRain[I].Alpha);
    TestTrue(TEXT("Reduced motion remains clearly identified as weather, not clear conditions"),
             UI->EnvironmentEffectsLabel().Contains(TEXT("STATIC WEATHER")));
    Cell->SetStringField(TEXT("weather"), TEXT("snow"));
    UI->ApplySnapshot(Snapshot);
    const auto Snow = UI->WeatherMarks();
    TestTrue(TEXT("Snow has its own particle presentation"), Snow.Num() > 100 && Snow[0].bSnow);
    TestFalse(TEXT("Snow never borrows rain splashes"),
              Snow.ContainsByPredicate([](const SRatwGame::FWeatherMark& M) { return M.bSplash; }));
    UI->Clock += 9;
    const auto LaterSnow = UI->WeatherMarks();
    TestTrue(TEXT("Reduced-motion snow stays static"), Snow[0].Position.Equals(LaterSnow[0].Position));
    Cell->SetStringField(TEXT("weather"), TEXT("fog"));
    UI->ApplySnapshot(Snapshot);
    TestTrue(TEXT("Fog does not pretend to be precipitation"), UI->WeatherMarks().IsEmpty());
    const auto StaticFog = UI->FogVeils();
    TestEqual(TEXT("Fog has layered veils instead of one flat wash"), StaticFog.Num(), 8);
    for (const auto& Veil : StaticFog)
        TestTrue(TEXT("Fog side edges remain beyond the map to prevent hard rectangle seams"),
                 Veil.Position.X <= UI->VisibleCellBounds().Left &&
                     Veil.Position.X + Veil.Size.X >= UI->VisibleCellBounds().Right);
    UI->Clock += 90;
    TestTrue(TEXT("Reduced-motion fog keeps its static veils"),
             StaticFog[0].Position.Equals(UI->FogVeils()[0].Position));
    UI->bReducedMotion = false;
    TestFalse(TEXT("Normal fog slowly drifts with time"), StaticFog[0].Position.Equals(UI->FogVeils()[0].Position));
    const auto Geometry = FGeometry::MakeRoot(FVector2D(1600, 1000), FSlateLayoutTransform());
    const TSharedRef<SWindow> Window = SNew(SWindow).ClientSize(FVector2D(1600, 1000));
    FSlateWindowElementList Elements(Window);
    UI->DrawEnvironment(Geometry, Elements, 0, false);
    UI->DrawEnvironment(Geometry, Elements, 3, true);
    TestTrue(TEXT("Environmental rendering cannot create click targets"), UI->Hits.IsEmpty());
    UI->DrawLocal(Geometry, Elements, 0);
    for (const auto& Hit : UI->Hits)
        TestFalse(TEXT("Weather metadata cannot reveal or target a hidden wolf"), Hit.Target == TEXT("hidden_wolf"));
    TestEqual(TEXT("Weather cannot invent visual entities"), UI->EntityViews.Num(), 1);
    TestEqual(TEXT("Weather does not alter the server's remembered-only visibility"), UI->VisibilityRows[0],
              FString::ChrN(32, TEXT('1')));
    TestTrue(TEXT("Environmental presentation cannot issue simulation commands"), Commands.IsEmpty());
    UI->SetPresentationPage(TEXT("world"));
    TestTrue(TEXT("Remembered world atlas is not obscured by local fog"), UI->FogVeils().IsEmpty());
    Cell->SetStringField(TEXT("weather"), TEXT("rain"));
    UI->ApplySnapshot(Snapshot);
    TestTrue(TEXT("Local rain is not rendered over remembered world geometry"), UI->WeatherMarks().IsEmpty());
    UI->SetPresentationPage(TEXT("balanced"));
    const FString Phases[] = {TEXT("dawn"), TEXT("day"), TEXT("dusk"), TEXT("night")};
    for (const FString& Phase : Phases)
    {
        Env->SetStringField(TEXT("phase"), Phase);
        UI->ApplySnapshot(Snapshot);
        TestEqual(TEXT("Every supported lighting phase is selectable"), UI->Environment.Phase, Phase);
        UI->DrawEnvironment(Geometry, Elements, 0, false);
        UI->DrawEnvironment(Geometry, Elements, 3, true);
        UI->Activate({FSlateRect(), TEXT("time"), Phase});
    }
    TestTrue(TEXT("Normal players cannot emit development clock mutations"), Commands.IsEmpty());
    Snapshot->SetBoolField(TEXT("devTools"), true);
    UI->ApplySnapshot(Snapshot);
    UI->Activate({FSlateRect(), TEXT("time"), TEXT("night")});
    TestEqual(TEXT("Developer clock emits one explicit request"), Commands.Num(), 1);
    if (Commands.Num())
        TestTrue(TEXT("Clock presets use the same value wire contract as weather"),
                 Commands.Last().Contains(TEXT("\"type\":\"time\"")) &&
                     Commands.Last().Contains(TEXT("\"value\":\"night\"")));
    UI->Hits.Empty();
    UI->Modal = TEXT("settings");
    UI->DrawModal(Geometry, Elements, 0);
    int ClockControls = 0;
    for (const auto& Hit : UI->Hits)
        if (Hit.Action == TEXT("time"))
        {
            ++ClockControls;
            TestTrue(TEXT("Clock controls remain inside their settings column"),
                     Hit.Rect.Left >= 326 && Hit.Rect.Right < 834 && Hit.Rect.Top >= 675 && Hit.Rect.Bottom <= 730);
        }
    TestEqual(TEXT("Developer settings expose four phase presets"), ClockControls, 4);
    Snapshot->SetBoolField(TEXT("devTools"), false);
    UI->ApplySnapshot(Snapshot);
    UI->Hits.Empty();
    UI->DrawModal(Geometry, Elements, 0);
    TestFalse(TEXT("Regular settings contain no development clock hit regions"),
              UI->Hits.ContainsByPredicate([](const SRatwGame::FHit& H) { return H.Action == TEXT("time"); }));
    Cell->SetBoolField(TEXT("outdoors"), false);
    Env->SetNumberField(TEXT("illumination"), .08);
    UI->ApplySnapshot(Snapshot);
    TestEqual(TEXT("Shelter does not turn an unlit room into full daylight"), UI->Environment.Illumination, .08);
    TestTrue(TEXT("Rain remains outside rather than falling through the roof"), UI->WeatherMarks().IsEmpty());
    TestTrue(TEXT("Indoor header names shelter rather than inventing adjacent exterior weather"),
             UI->EnvironmentLabel().EndsWith(TEXT("SHELTERED")) && !UI->EnvironmentLabel().Contains(TEXT("RAIN")));
    Env->SetStringField(TEXT("phase"), TEXT("day"));
    Env->SetNumberField(TEXT("hour"), 12.5);
    UI->ApplySnapshot(Snapshot);
    TestEqual(TEXT("Shelter has an explicit clock without revealing any outside weather"), UI->EnvironmentLabel(),
              FString(TEXT("12:30 DAY · SHELTERED")));
    Cell->SetStringField(TEXT("weather"), TEXT("fog"));
    UI->ApplySnapshot(Snapshot);
    TestTrue(TEXT("Exterior fog cannot fill a sheltered room"), UI->FogVeils().IsEmpty());
    Cell->SetBoolField(TEXT("outdoors"), true);
    Cell->SetStringField(TEXT("weather"), TEXT("unsupported condition"));
    Env->SetStringField(TEXT("phase"), TEXT("unsupported phase"));
    Env->SetNumberField(TEXT("hour"), std::numeric_limits<double>::infinity());
    Env->SetNumberField(TEXT("illumination"), std::numeric_limits<double>::quiet_NaN());
    Env->SetBoolField(TEXT("daylight"), false);
    Env->SetStringField(TEXT("hearing"), TEXT("0.1"));
    Env->SetNumberField(TEXT("sight"), 1.e20);
    Env->SetNumberField(TEXT("scent"), -1.e20);
    UI->ApplySnapshot(Snapshot);
    TestEqual(TEXT("Nonfinite clock safely defaults to midday"), UI->Environment.Hour, 12.);
    TestEqual(TEXT("Unknown phase derives a safe day label"), UI->Environment.Phase, FString(TEXT("day")));
    TestEqual(TEXT("Unknown weather falls back without creating an effect"), UI->Environment.Weather,
              FString(TEXT("clear")));
    TestEqual(TEXT("Nonfinite light cannot enter rendering math"), UI->Environment.Illumination, 1.);
    TestEqual(TEXT("Booleans are not accepted as daylight numbers"), UI->Environment.Daylight, 1.);
    TestEqual(TEXT("Numeric strings cannot override hearing presentation"), UI->Environment.Hearing, 1.);
    TestEqual(TEXT("Extreme positive factors are bounded"), UI->Environment.Sight, 4.);
    TestEqual(TEXT("Extreme negative factors are bounded"), UI->Environment.Scent, 0.);
    Env->SetNumberField(TEXT("hour"), 24);
    UI->ApplySnapshot(Snapshot);
    TestEqual(TEXT("Hour 24 displays the next midnight rather than an invalid clock"), UI->Environment.Hour, 0.);
    Cell->RemoveField(TEXT("environment"));
    UI->ApplySnapshot(Snapshot);
    TestEqual(TEXT("Legacy snapshots have a safe noon clock"), UI->Environment.Hour, 12.);
    TestEqual(TEXT("Missing modifiers do not retain stale weather penalties"), UI->Environment.Sight, 1.);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwUIAtmosphereTest, "RATW.UI.CellAtmosphere",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwUIAtmosphereTest::RunTest(const FString&)
{
    TArray<FString> Commands;
    const TSharedRef<SRatwGame> UI = SNew(SRatwGame).OnCommand([&](const FString& S) { Commands.Add(S); });
    const auto Snapshot = MakeShared<FJsonObject>(), Cell = MakeShared<FJsonObject>(), Env = MakeShared<FJsonObject>(),
               Self = MakeShared<FJsonObject>();
    Cell->SetStringField(TEXT("id"), TEXT("small_tavern"));
    Cell->SetNumberField(TEXT("width"), 16);
    Cell->SetNumberField(TEXT("height"), 12);
    Cell->SetBoolField(TEXT("outdoors"), false);
    Cell->SetObjectField(TEXT("environment"), Env);
    Snapshot->SetObjectField(TEXT("cell"), Cell);
    Snapshot->SetObjectField(TEXT("self"), Self);
    Env->SetNumberField(TEXT("hour"), 23);
    Env->SetStringField(TEXT("phase"), TEXT("night"));
    Env->SetNumberField(TEXT("daylight"), 0);
    Env->SetNumberField(TEXT("illumination"), 1);
    Env->SetNumberField(TEXT("artificialLight"), 1);
    Env->SetNumberField(TEXT("daylightAccess"), 1);
    Env->SetNumberField(TEXT("glowStrength"), 1);
    Env->SetStringField(TEXT("lightingTone"), TEXT("warm"));
    Env->SetStringField(TEXT("lightSource"), TEXT("artificial"));
    UI->ApplySnapshot(Snapshot);
    UI->MapRect = FSlateRect(584, 199, 1544, 816);
    UI->MapOrigin = FVector2D(840, 340);
    UI->TileSize = 28;
    const auto Warm = UI->CellAtmosphere();
    TestEqual(TEXT("Night tavern retains full authoritative interior light"), UI->Environment.Illumination, 1.);
    TestEqual(TEXT("Warm light has no artificial dark vignette"), Warm.Darkness, 0.);
    TestEqual(TEXT("Night tavern receives the full supplied glow"), Warm.GlowStrength, 1.);
    TestTrue(TEXT("Warm halo uses amber rather than outdoor blue"), Warm.GlowColor.R > Warm.GlowColor.B);
    TestTrue(TEXT("Halo bounds follow the small room, not the larger viewport"),
             Warm.Bounds.Left == 840 && Warm.Bounds.Top == 340 && Warm.Bounds.Right == 1288 &&
                 Warm.Bounds.Bottom == 676 && Warm.Bounds.Left > UI->MapRect.Left);
    TestTrue(TEXT("Edge feather leaves the center of a small room untouched"),
             Warm.Feather > 0 && Warm.Feather < (Warm.Bounds.Bottom - Warm.Bounds.Top) * .5);
    UI->MapOrigin += FVector2D(-170, 50);
    const auto Panned = UI->CellAtmosphere();
    TestTrue(TEXT("Panning moves all four atmosphere edges with terrain"),
             Panned.Bounds.Left == Warm.Bounds.Left - 170 && Panned.Bounds.Right == Warm.Bounds.Right - 170 &&
                 Panned.Bounds.Top == Warm.Bounds.Top + 50 && Panned.Bounds.Bottom == Warm.Bounds.Bottom + 50);
    UI->CellWidth = 96;
    UI->CellHeight = 64;
    UI->MapOrigin = FVector2D(-100, -500);
    const auto Large = UI->CellAtmosphere();
    TestTrue(TEXT("A large cell keeps true off-screen edges without a viewport-edge halo"),
             Large.Bounds.Left + Large.Feather < UI->MapRect.Left &&
                 Large.Bounds.Right - Large.Feather > UI->MapRect.Right &&
                 Large.Bounds.Top + Large.Feather < UI->MapRect.Top &&
                 Large.Bounds.Bottom - Large.Feather > UI->MapRect.Bottom);
    TestTrue(TEXT("Only the precipitation clip uses the viewport intersection"),
             UI->VisibleCellBounds().Left == UI->MapRect.Left + 1 &&
                 UI->VisibleCellBounds().Right == UI->MapRect.Right - 1);
    UI->bReducedMotion = true;
    UI->Clock = 1000;
    TestEqual(TEXT("Reduced motion retains the static halo"), UI->CellAtmosphere().GlowStrength, Large.GlowStrength);
    UI->bWorldMap = true;
    TestTrue(TEXT("The remembered world map receives no local lighting overlay"),
             UI->CellAtmosphere().GlowStrength == 0 && UI->CellAtmosphere().Darkness == 0 &&
                 UI->CellAtmosphere().WeatherStrength == 0);
    UI->bWorldMap = false;
    Env->SetNumberField(TEXT("hour"), 12);
    Env->SetStringField(TEXT("phase"), TEXT("day"));
    Env->SetNumberField(TEXT("daylight"), 1);
    Env->SetNumberField(TEXT("glowStrength"), 0);
    Env->SetStringField(TEXT("lightSource"), TEXT("mixed"));
    UI->ApplySnapshot(Snapshot);
    TestTrue(TEXT("The same lit tavern at noon has neither glow, tint, nor darkness"),
             UI->CellAtmosphere().GlowStrength == 0 && UI->CellAtmosphere().Darkness == 0 &&
                 UI->CellAtmosphere().WeatherStrength == 0);
    Env->SetNumberField(TEXT("illumination"), .08);
    Env->SetNumberField(TEXT("artificialLight"), 0);
    Env->SetNumberField(TEXT("daylightAccess"), 0);
    Env->SetStringField(TEXT("lightSource"), TEXT("dark"));
    UI->ApplySnapshot(Snapshot);
    TestTrue(TEXT("A windowless unlit interior is dark even at noon"),
             UI->CellAtmosphere().Darkness > .9 && UI->CellAtmosphere().GlowStrength == 0);
    TestTrue(TEXT("The summary identifies an unlit shelter"), UI->EnvironmentEffectsLabel().Contains(TEXT("UNLIT")));
    Env->SetNumberField(TEXT("illumination"), 1);
    Env->SetNumberField(TEXT("glowStrength"), 1);
    Env->SetStringField(TEXT("lightingTone"), TEXT("cool"));
    UI->ApplySnapshot(Snapshot);
    TestTrue(TEXT("A cool light has its own blue halo"),
             UI->CellAtmosphere().GlowColor.B > UI->CellAtmosphere().GlowColor.R);
    Cell->SetStringField(TEXT("weather"), TEXT("rain"));
    UI->ApplySnapshot(Snapshot);
    TestTrue(TEXT("Rain stays outside a roofed room regardless of its lighting"),
             UI->CellAtmosphere().WeatherStrength == 0 && UI->WeatherMarks().IsEmpty());
    Cell->SetBoolField(TEXT("outdoors"), true);
    const FString Conditions[] = {TEXT("rain"), TEXT("snow"), TEXT("fog")};
    for (const auto& Condition : Conditions)
    {
        Cell->SetStringField(TEXT("weather"), Condition);
        UI->ApplySnapshot(Snapshot);
        TestTrue(TEXT("Outdoor weather has a static edge atmosphere, not an indoor artificial glow"),
                 UI->CellAtmosphere().WeatherStrength > 0 && UI->CellAtmosphere().GlowStrength == 0);
    }
    Env->SetNumberField(TEXT("glowStrength"), std::numeric_limits<double>::infinity());
    Env->SetBoolField(TEXT("artificialLight"), true);
    Env->SetStringField(TEXT("daylightAccess"), TEXT("1"));
    Env->SetStringField(TEXT("lightingTone"), TEXT("hidden_wolf"));
    Env->SetStringField(TEXT("lightSource"), TEXT("Hidden identity"));
    UI->ApplySnapshot(Snapshot);
    TestEqual(TEXT("Nonfinite glow never enters gradient geometry"), UI->Environment.GlowStrength, 0.);
    TestEqual(TEXT("Booleans cannot create artificial light"), UI->Environment.ArtificialLight, 0.);
    TestEqual(TEXT("String daylight access uses the safe legacy default"), UI->Environment.DaylightAccess, 1.);
    TestEqual(TEXT("Invalid light tone becomes neutral"), UI->Environment.LightingTone, FString(TEXT("neutral")));
    TestEqual(TEXT("Arbitrary source metadata cannot become an identity"), UI->Environment.LightSource,
              FString(TEXT("daylight")));
    const auto Geometry = FGeometry::MakeRoot(FVector2D(1600, 1000), FSlateLayoutTransform());
    const TSharedRef<SWindow> Window = SNew(SWindow).ClientSize(FVector2D(1600, 1000));
    FSlateWindowElementList Elements(Window);
    UI->DrawEnvironment(Geometry, Elements, 0, false);
    UI->DrawEnvironment(Geometry, Elements, 3, true);
    TestTrue(TEXT("Atmosphere rendering creates no entities, interactions, or simulation commands"),
             UI->EntityViews.IsEmpty() && UI->Hits.IsEmpty() && Commands.IsEmpty());
    UI->Activate({FSlateRect(), TEXT("lighting"), TEXT("warm")});
    TestTrue(TEXT("Normal players cannot issue lighting presets"), Commands.IsEmpty());
    Snapshot->SetBoolField(TEXT("devTools"), true);
    UI->ApplySnapshot(Snapshot);
    UI->Activate({FSlateRect(), TEXT("lighting"), TEXT("warm")});
    TestTrue(TEXT("Developer lighting presets use explicit type and value fields"),
             Commands.Num() == 1 && Commands[0].Contains(TEXT("\"type\":\"lighting\"")) &&
                 Commands[0].Contains(TEXT("\"value\":\"warm\"")));
    UI->Modal = TEXT("settings");
    UI->DrawModal(Geometry, Elements, 0);
    int32 LightingControls = 0;
    for (const auto& Hit : UI->Hits)
        if (Hit.Action == TEXT("lighting"))
        {
            ++LightingControls;
            TestTrue(TEXT("Lighting settings stay in their own row above the keyboard help"),
                     Hit.Rect.Left >= 326 && Hit.Rect.Right <= 834 && Hit.Rect.Top == 755 && Hit.Rect.Bottom == 789);
            for (const auto& Other : UI->Hits)
                if (&Other != &Hit)
                    TestFalse(TEXT("Lighting presets do not overlap other settings hit targets"),
                              Hit.Rect.Left < Other.Rect.Right && Hit.Rect.Right > Other.Rect.Left &&
                                  Hit.Rect.Top < Other.Rect.Bottom && Hit.Rect.Bottom > Other.Rect.Top);
        }
    TestEqual(TEXT("Developer settings include four room lighting presets"), LightingControls, 4);
    Snapshot->SetBoolField(TEXT("devTools"), false);
    Cell->RemoveField(TEXT("environment"));
    UI->ApplySnapshot(Snapshot);
    UI->Hits.Empty();
    UI->DrawModal(Geometry, Elements, 0);
    TestFalse(TEXT("Ordinary settings contain no room-light mutation controls"),
              UI->Hits.ContainsByPredicate([](const SRatwGame::FHit& H) { return H.Action == TEXT("lighting"); }));
    TestTrue(TEXT("Legacy snapshots do not retain a prior room's glow or darkness"),
             UI->Environment.GlowStrength == 0 && UI->Environment.ArtificialLight == 0 &&
                 UI->Environment.Illumination == 1 && UI->Environment.LightingTone == TEXT("neutral"));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwUICalendarEconomyTest, "RATW.UI.CalendarAndEconomy",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwUICalendarEconomyTest::RunTest(const FString&)
{
    TArray<FString> Commands;
    const TSharedRef<SRatwGame> UI = SNew(SRatwGame).OnCommand([&](const FString& S) { Commands.Add(S); });
    const auto Snapshot = MakeShared<FJsonObject>(), Cell = MakeShared<FJsonObject>(), Env = MakeShared<FJsonObject>(),
               Calendar = MakeShared<FJsonObject>(), Self = MakeShared<FJsonObject>(),
               Merchant = MakeShared<FJsonObject>(), Herbs = MakeShared<FJsonObject>(),
               Meal = MakeShared<FJsonObject>(), Resource = MakeShared<FJsonObject>();
    Cell->SetStringField(TEXT("id"), TEXT("clearing"));
    Cell->SetBoolField(TEXT("outdoors"), true);
    Cell->SetObjectField(TEXT("environment"), Env);
    Env->SetObjectField(TEXT("calendar"), Calendar);
    Calendar->SetNumberField(TEXT("year"), 3);
    Calendar->SetNumberField(TEXT("dayOfYear"), 191);
    Calendar->SetNumberField(TEXT("dayOfSeason"), 7);
    Calendar->SetStringField(TEXT("season"), TEXT("autumn"));
    Calendar->SetStringField(TEXT("moonName"), TEXT("full moon"));
    Calendar->SetNumberField(TEXT("moonIllumination"), 1);
    Calendar->SetNumberField(TEXT("moonPhase"), .5);
    Calendar->SetNumberField(TEXT("absoluteDays"), 920);
    Self->SetStringField(TEXT("id"), TEXT("self"));
    Self->SetNumberField(TEXT("cash"), 20);
    Self->SetNumberField(TEXT("age"), 70);
    Self->SetNumberField(TEXT("strength"), 64);
    Self->SetNumberField(TEXT("dexterity"), 60);
    Self->SetNumberField(TEXT("effectiveDexterity"), 56.4);
    Self->SetNumberField(TEXT("wisdom"), 85);
    Self->SetNumberField(TEXT("x"), 16);
    Self->SetNumberField(TEXT("y"), 7.5);
    Merchant->SetStringField(TEXT("id"), TEXT("npc_keeper"));
    Merchant->SetStringField(TEXT("name"), TEXT("The keeper"));
    Merchant->SetNumberField(TEXT("cash"), 40);
    for (const auto& Item : {Herbs, Meal})
    {
        Item->SetStringField(TEXT("id"), Item == Herbs ? TEXT("herbs") : TEXT("meal"));
        Item->SetStringField(TEXT("icon"), TEXT("food"));
        Item->SetNumberField(TEXT("quantity"), Item == Herbs ? 3 : 1);
        Item->SetNumberField(TEXT("stock"), 6);
        Item->SetNumberField(TEXT("owned"), Item == Herbs ? 3 : 1);
        Item->SetNumberField(TEXT("buyPrice"), Item == Herbs ? 2 : 6);
        Item->SetNumberField(TEXT("sellPrice"), Item == Herbs ? 1 : 3);
        Item->SetBoolField(TEXT("canBuy"), true);
        Item->SetBoolField(TEXT("canSell"), true);
    }
    Merchant->SetArrayField(TEXT("items"), {MakeShared<FJsonValueObject>(Herbs), MakeShared<FJsonValueObject>(Meal)});
    Snapshot->SetArrayField(TEXT("inventory"), {MakeShared<FJsonValueObject>(Herbs), MakeShared<FJsonValueObject>(Meal),
                                                MakeShared<FJsonValueString>(TEXT("invalid inventory row"))});
    Resource->SetStringField(TEXT("id"), TEXT("herb_patch"));
    Resource->SetNumberField(TEXT("x"), 17.5);
    Resource->SetNumberField(TEXT("y"), 7.5);
    Resource->SetNumberField(TEXT("remaining"), 8);
    Snapshot->SetObjectField(TEXT("cell"), Cell);
    Snapshot->SetObjectField(TEXT("self"), Self);
    Snapshot->SetObjectField(TEXT("merchant"), Merchant);
    Snapshot->SetObjectField(TEXT("resource"), Resource);
    UI->ApplySnapshot(Snapshot);
    TestEqual(TEXT("The calendar uses authoritative year and season day"), UI->CalendarLabel(),
              FString(TEXT("YEAR 3 · AUTUMN 7 · DAY 191 / 365")));
    TestEqual(TEXT("Moon phase and illumination are visible in text"), UI->MoonLabel(),
              FString(TEXT("FULL MOON · 100% LIT")));
    Calendar->SetBoolField(TEXT("year"), true);
    Calendar->SetStringField(TEXT("moonName"), TEXT("Hidden NPC identity"));
    TestEqual(TEXT("Malformed dates do not enter numeric presentation"), UI->CalendarLabel(),
              FString(TEXT("THE SHARED WORLD")));
    TestEqual(TEXT("Unknown phase names cannot become arbitrary header text"), UI->MoonLabel(),
              FString(TEXT("MOON · UNKNOWN")));
    Calendar->SetNumberField(TEXT("year"), 3);
    Calendar->SetStringField(TEXT("moonName"), TEXT("waxing crescent"));
    Calendar->SetNumberField(TEXT("moonIllumination"), std::numeric_limits<double>::infinity());
    TestEqual(TEXT("Nonfinite lunar illumination safely resets"), UI->MoonLabel(),
              FString(TEXT("WAXING CRESCENT · 0% LIT")));
    TestTrue(TEXT("Valid one-unit buy and sell offers are enabled"),
             UI->CanTradeItem(TEXT("herbs"), true) && UI->CanTradeItem(TEXT("meal"), false));
    TestFalse(TEXT("Unknown items never become trade targets"), UI->CanTradeItem(TEXT("secret_item"), true));
    UI->ContextTarget = TEXT("npc_keeper");
    UI->Activate({FSlateRect(), TEXT("context"), TEXT("trade")});
    TestEqual(TEXT("The keeper's context action opens the trade modal"), UI->Modal, FString(TEXT("trade")));
    TestFalse(TEXT("Opening trade does not buy or sell anything"),
              Commands.ContainsByPredicate(
                  [](const FString& Command) { return Command.Contains(TEXT("\"type\":\"trade\"")); }));
    Commands.Empty();
    UI->Activate({FSlateRect(), TEXT("trade_buy"), TEXT("meal")});
    TestEqual(TEXT("A buy click issues exactly one command"), Commands.Num(), 1);
    if (Commands.Num() == 1)
    {
        TSharedPtr<FJsonObject> Wire;
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Commands[0]), Wire);
        TestTrue(TEXT("Trade command uses the exact bounded server contract without a client price"),
                 Wire && Wire->GetStringField(TEXT("type")) == TEXT("trade") &&
                     Wire->GetStringField(TEXT("target")) == TEXT("npc_keeper") &&
                     Wire->GetStringField(TEXT("item")) == TEXT("meal") &&
                     Wire->GetNumberField(TEXT("quantity")) == 1 && Wire->GetBoolField(TEXT("buy")) &&
                     !Wire->HasField(TEXT("price")));
    }
    Commands.Empty();
    UI->Activate({FSlateRect(), TEXT("trade_sell"), TEXT("herbs")});
    TestTrue(TEXT("Selling has its own direction flag"),
             Commands.Num() == 1 && Commands[0].Contains(TEXT("\"buy\":false")));
    Commands.Empty();
    Self->SetNumberField(TEXT("cash"), 1);
    TestFalse(TEXT("The player cannot select a quote they cannot afford"), UI->CanTradeItem(TEXT("herbs"), true));
    Self->SetNumberField(TEXT("cash"), 20);
    Merchant->SetNumberField(TEXT("cash"), 0);
    TestFalse(TEXT("A broke keeper cannot offer to buy"), UI->CanTradeItem(TEXT("meal"), false));
    Merchant->SetNumberField(TEXT("cash"), 40);
    Herbs->SetStringField(TEXT("canBuy"), TEXT("true"));
    TestFalse(TEXT("String booleans cannot authorize an offer"), UI->CanTradeItem(TEXT("herbs"), true));
    Herbs->SetBoolField(TEXT("canBuy"), true);
    Herbs->SetNumberField(TEXT("buyPrice"), .5);
    TestFalse(TEXT("Fractional pennies do not become prices"), UI->CanTradeItem(TEXT("herbs"), true));
    Herbs->SetNumberField(TEXT("buyPrice"), 2);
    Herbs->SetNumberField(TEXT("stock"), 0);
    TestFalse(TEXT("Empty stock disables buying"), UI->CanTradeItem(TEXT("herbs"), true));
    Herbs->SetNumberField(TEXT("stock"), 6);
    Herbs->SetBoolField(TEXT("canSell"), false);
    TestFalse(TEXT("Server demand limits disable selling despite cash and owned goods"),
              UI->CanTradeItem(TEXT("herbs"), false));
    Herbs->SetBoolField(TEXT("canSell"), true);
    const auto Geometry = FGeometry::MakeRoot(FVector2D(1600, 1000), FSlateLayoutTransform());
    const TSharedRef<SWindow> Window = SNew(SWindow).ClientSize(FVector2D(1600, 1000));
    FSlateWindowElementList Elements(Window);
    UI->Hits.Empty();
    UI->DrawModal(Geometry, Elements, 0);
    int32 OfferButtons = 0;
    for (const auto& Hit : UI->Hits)
        if (Hit.Action == TEXT("trade_buy") || Hit.Action == TEXT("trade_sell"))
        {
            ++OfferButtons;
            TestTrue(TEXT("Trade offers are confined to the modal goods rows"),
                     Hit.Rect.Left >= 326 && Hit.Rect.Right <= 1234 && Hit.Rect.Top >= 347 && Hit.Rect.Bottom < 737);
        }
    TestEqual(TEXT("Only two real goods generate four offer buttons"), OfferButtons, 4);
    Snapshot->RemoveField(TEXT("merchant"));
    UI->ApplySnapshot(Snapshot);
    UI->Hits.Empty();
    UI->DrawModal(Geometry, Elements, 0);
    TestFalse(TEXT("A departed, hidden, or sleeping keeper leaves no stale offer hit regions"),
              UI->Hits.ContainsByPredicate([](const SRatwGame::FHit& Hit) {
                  return Hit.Action == TEXT("trade_buy") || Hit.Action == TEXT("trade_sell");
              }));
    UI->Activate({FSlateRect(), TEXT("trade_buy"), TEXT("meal")});
    TestTrue(TEXT("A queued click cannot trade after the keeper disappears"), Commands.IsEmpty());
    UI->Activate({FSlateRect(), TEXT("eat"), TEXT("")});
    TestTrue(TEXT("Eating uses the explicit meal command"),
             Commands.Num() == 1 && Commands[0].Contains(TEXT("\"type\":\"eat\"")));
    Commands.Empty();
    Meal->SetNumberField(TEXT("quantity"), 0);
    UI->Activate({FSlateRect(), TEXT("eat"), TEXT("")});
    TestTrue(TEXT("An absent meal cannot be eaten from a stale button"), Commands.IsEmpty());
    TestTrue(TEXT("A visible supplied patch within reach can be gathered"), UI->CanGather());
    UI->Activate({FSlateRect(), TEXT("gather"), TEXT("")});
    TestTrue(TEXT("Gather uses the explicit server command"),
             Commands.Num() == 1 && Commands[0].Contains(TEXT("\"type\":\"gather\"")));
    Commands.Empty();
    Self->SetNumberField(TEXT("x"), 15.7);
    TestFalse(TEXT("Gather range matches the 1.7-tile authority limit"), UI->CanGather());
    Self->SetNumberField(TEXT("x"), 16);
    Resource->SetNumberField(TEXT("remaining"), 0);
    TestFalse(TEXT("An exhausted patch is not an infinite resource"), UI->CanGather());
    Resource->SetNumberField(TEXT("remaining"), 8);
    Resource->SetNumberField(TEXT("x"), std::numeric_limits<double>::quiet_NaN());
    TestFalse(TEXT("Malformed resource coordinates cannot enter map geometry"), UI->VisibleResource().IsValid());
    Resource->SetNumberField(TEXT("x"), 17.5);
    UI->MapRect = FSlateRect(584, 199, 1544, 816);
    UI->Hits.Empty();
    UI->DrawLocal(Geometry, Elements, 0);
    TestTrue(TEXT("An explicitly visible resource receives a map context target"),
             UI->Hits.ContainsByPredicate([](const SRatwGame::FHit& Hit) { return Hit.Target == TEXT("herb_patch"); }));
    Snapshot->RemoveField(TEXT("resource"));
    UI->ApplySnapshot(Snapshot);
    UI->Hits.Empty();
    UI->DrawLocal(Geometry, Elements, 0);
    TestFalse(
        TEXT("An out-of-sight resource never leaves a remembered live target"),
        UI->Hits.ContainsByPredicate([](const SRatwGame::FHit& Hit) { return Hit.Target == TEXT("herb_patch"); }));
    UI->Activate({FSlateRect(), TEXT("gather"), TEXT("")});
    TestTrue(TEXT("A hidden resource cannot be gathered from a stale action"), Commands.IsEmpty());
    UI->SetPresentationPage(TEXT("character"));
    UI->DrawModal(Geometry, Elements, 0);
    UI->SetPresentationPage(TEXT("inventory"));
    UI->DrawModal(Geometry, Elements, 0);
    UI->Activate({FSlateRect(), TEXT("calendar"), TEXT("year")});
    TestTrue(TEXT("Players cannot advance calendar or award age milestones"), Commands.IsEmpty());
    Snapshot->SetBoolField(TEXT("devTools"), true);
    UI->ApplySnapshot(Snapshot);
    UI->Activate({FSlateRect(), TEXT("calendar"), TEXT("day")});
    TestTrue(TEXT("Developer day controls use explicit calendar wire fields"),
             Commands.Num() == 1 && Commands[0].Contains(TEXT("\"type\":\"calendar\"")) &&
                 Commands[0].Contains(TEXT("\"value\":\"day\"")));
    Commands.Empty();
    UI->Activate({FSlateRect(), TEXT("calendar"), TEXT("unsupported")});
    TestTrue(TEXT("The calendar controls offer only bounded known jumps"), Commands.IsEmpty());
    UI->SetPresentationPage(TEXT("settings"));
    UI->Hits.Empty();
    UI->DrawModal(Geometry, Elements, 0);
    int32 CalendarButtons = 0;
    for (const auto& Hit : UI->Hits)
        if (Hit.Action == TEXT("calendar") || (Hit.Action == TEXT("weather") && Hit.Target == TEXT("seasonal")))
        {
            ++CalendarButtons;
            TestTrue(TEXT("Calendar and seasonal controls fit their separate settings row"),
                     Hit.Rect.Left >= 886 && Hit.Rect.Right <= 1234 && Hit.Rect.Top == 800 && Hit.Rect.Bottom == 832);
            for (const auto& Other : UI->Hits)
                if (&Other != &Hit)
                    TestFalse(TEXT("New developer controls do not overlap other buttons"),
                              Hit.Rect.Left < Other.Rect.Right && Hit.Rect.Right > Other.Rect.Left &&
                                  Hit.Rect.Top < Other.Rect.Bottom && Hit.Rect.Bottom > Other.Rect.Top);
        }
    TestEqual(TEXT("Two calendar jumps and seasonal weather are available to developers"), CalendarButtons, 3);
    UI->SetPresentationPage(TEXT("trade"));
    TestEqual(TEXT("Native screenshots can select the trade presentation"), UI->Modal, FString(TEXT("trade")));
    Env->RemoveField(TEXT("calendar"));
    TestTrue(TEXT("Legacy snapshots do not retain a stale date or moon"),
             UI->CalendarLabel() == TEXT("THE SHARED WORLD") && UI->MoonLabel().IsEmpty());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwUIFrontDoorTest, "RATW.UI.FrontDoorAccountAndCreation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwUIFrontDoorTest::RunTest(const FString&)
{
    TArray<FString> Commands;
    const auto UI = SNew(SRatwFrontDoor).OnCommand([&](const FString& Json) { Commands.Add(Json); });
    TestEqual(TEXT("Front door begins at login, without a spawned character"), UI->GetPresentationPage(), FString(TEXT("login")));
    UI->Username->SetText(FText::FromString(TEXT("sample_account")));
    UI->Password->SetText(FText::FromString(TEXT("short")));
    UI->SubmitAuth();
    TestTrue(TEXT("Short test passwords are rejected before sending"), Commands.IsEmpty());
    UI->Password->SetText(FText::FromString(TEXT("test-only-secret-123")));
    UI->SubmitAuth();
    TestEqual(TEXT("One account request is emitted"), Commands.Num(), 1);
    TestTrue(TEXT("The password field is cleared immediately after submission"), UI->Password->GetText().IsEmpty());
    TestTrue(TEXT("Waiting for authority disables repeat submission"), UI->bBusy);
    UI->SubmitAuth();
    TestEqual(TEXT("A second click cannot duplicate an in-flight request"), Commands.Num(), 1);
    TSharedPtr<FJsonObject> Request;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Commands[0]), Request);
    TestEqual(TEXT("Auth uses the agreed account command"), Request->GetStringField(TEXT("type")), FString(TEXT("auth_login")));
    TestEqual(TEXT("Account name is not confused with character identity"), Request->GetStringField(TEXT("username")), FString(TEXT("sample_account")));

    const auto Lobby = MakeShared<FJsonObject>();
    Lobby->SetStringField(TEXT("type"), TEXT("lobby"));
    Lobby->SetStringField(TEXT("stage"), TEXT("characters"));
    Lobby->SetBoolField(TEXT("ok"), true);
    Lobby->SetArrayField(TEXT("characters"), {});
    UI->ReceiveEvent(Lobby);
    TestEqual(TEXT("Authentication opens an empty real roster"), UI->Page, FString(TEXT("roster")));
    TestTrue(TEXT("No sample player is fabricated in an empty account"), UI->Characters.IsEmpty());
    UI->SetPresentationPage(TEXT("creator"));
    TestEqual(TEXT("Creator is a separate unpublished page"), UI->Page, FString(TEXT("creator")));
    TestEqual(TEXT("Starting age defaults to adult"), UI->DraftAge, 18);
    TestTrue(TEXT("Creator includes all nine persisted appearance fields"), UI->DraftAppearance->Values.Num() == 9);
    UI->DraftName = TEXT("  Briar  ");
    UI->DraftAge = 12;
    UI->DraftAppearance->SetStringField(TEXT("species"), TEXT("maned"));
    UI->DraftAppearance->SetStringField(TEXT("pattern"), TEXT("saddle"));
    UI->DraftAppearance->SetNumberField(TEXT("baseColor"), 6);
    UI->Review();
    TestEqual(TEXT("Review trims the character name"), UI->DraftName, FString(TEXT("Briar")));
    TestEqual(TEXT("Review is explicit before server creation"), UI->Page, FString(TEXT("review")));
    TestEqual(TEXT("Previewing and reviewing sent no creation commands"), Commands.Num(), 1);
    const auto Geometry = FGeometry::MakeRoot(FVector2D(1440, 940), FSlateLayoutTransform());
    UI->OnKeyDown(Geometry, Key(EKeys::Escape));
    TestEqual(TEXT("Escape returns from review to appearance"), UI->Page, FString(TEXT("creator")));
    TestEqual(TEXT("Back preserves the character draft"), UI->DraftAppearance->GetStringField(TEXT("species")), FString(TEXT("maned")));
    UI->Review(); UI->CreateCharacter();
    TestEqual(TEXT("Confirm emits exactly one character creation request"), Commands.Num(), 2);
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Commands[1]), Request);
    TestEqual(TEXT("Creation is not an enter command"), Request->GetStringField(TEXT("type")), FString(TEXT("character_create")));
    TestEqual(TEXT("Chosen age is sent authoritatively"), Request->GetNumberField(TEXT("age")), 12.0);
    TestEqual(TEXT("Chosen appearance is sent instead of preview-only art"),
              Request->GetObjectField(TEXT("appearance"))->GetStringField(TEXT("species")), FString(TEXT("maned")));
    const FString CreationId = Request->GetStringField(TEXT("commandId"));
    TestFalse(TEXT("Creation includes its own nonempty idempotency key"), CreationId.IsEmpty());
    UI->bBusy = false; // Simulate the bounded network timeout, without a server reply.
    UI->CreateCharacter();
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Commands.Last()), Request);
    TestEqual(TEXT("Retrying an unchanged draft reuses the original creation receipt"),
              Request->GetStringField(TEXT("commandId")), CreationId);
    UI->bBusy = false;
    UI->DraftAppearance->SetNumberField(TEXT("baseColor"), 7);
    UI->CreateCharacter();
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Commands.Last()), Request);
    TestTrue(TEXT("A changed appearance creates a new request identity"),
             Request->GetStringField(TEXT("commandId")) != CreationId);
    Lobby->SetBoolField(TEXT("ok"), false); Lobby->SetStringField(TEXT("message"), TEXT("Try again."));
    UI->ReceiveEvent(Lobby);
    TestEqual(TEXT("Failed creation retains the review and draft"), UI->Page, FString(TEXT("review")));
    TestEqual(TEXT("Failed creation retains name"), UI->DraftName, FString(TEXT("Briar")));
    const auto Character = MakeShared<FJsonObject>();
    Character->SetStringField(TEXT("id"), TEXT("owned_character_1")); Character->SetStringField(TEXT("name"), TEXT("Briar"));
    Character->SetNumberField(TEXT("age"), 12); Character->SetObjectField(TEXT("appearance"), UI->DraftAppearance);
    Lobby->SetBoolField(TEXT("ok"), true);
    Lobby->SetArrayField(TEXT("characters"), {MakeShared<FJsonValueObject>(Character)});
    UI->ReceiveEvent(Lobby);
    TestEqual(TEXT("Successful creation returns to selection, not auto-entry"), UI->Page, FString(TEXT("roster")));
    TestEqual(TEXT("Newly created real character is selected"), UI->SelectedId, FString(TEXT("owned_character_1")));
    UI->EnterCharacter();
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Commands.Last()), Request);
    TestEqual(TEXT("Entry uses owned generated identity"), Request->GetStringField(TEXT("id")), FString(TEXT("owned_character_1")));
    Lobby->SetStringField(TEXT("stage"), TEXT("login"));
    UI->ReceiveEvent(Lobby);
    TestTrue(TEXT("Logout clears private roster and appearance drafts"), UI->Characters.IsEmpty() && !UI->DraftAppearance);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRatwUIPortraitTest, "RATW.UI.OwnAndInspectedPortraits",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRatwUIPortraitTest::RunTest(const FString&)
{
    TArray<FString> Commands;
    const auto UI = SNew(SRatwGame).OnCommand([&](const FString& Json) { Commands.Add(Json); });
    const auto Snapshot = MakeShared<FJsonObject>();
    const auto Self = MakeShared<FJsonObject>();
    const auto OwnAppearance = MakeShared<FJsonObject>();
    OwnAppearance->SetStringField(TEXT("species"), TEXT("timber"));
    Self->SetObjectField(TEXT("appearance"), OwnAppearance); Self->SetNumberField(TEXT("age"), 73);
    Snapshot->SetObjectField(TEXT("self"), Self);
    UI->ApplySnapshot(Snapshot); UI->SetPresentationPage(TEXT("character"));
    TestTrue(TEXT("Own sheet reads the authoritative own appearance"), UI->PortraitAppearance() == OwnAppearance);
    TestEqual(TEXT("Own sheet can use its private exact age"), UI->PortraitAge(), 73.0);
    const auto Inspect = MakeShared<FJsonObject>();
    const auto OtherAppearance = MakeShared<FJsonObject>();
    OtherAppearance->SetStringField(TEXT("species"), TEXT("arctic"));
    Inspect->SetStringField(TEXT("type"), TEXT("inspect")); Inspect->SetStringField(TEXT("title"), TEXT("Visible wolf"));
    Inspect->SetObjectField(TEXT("appearance"), OtherAppearance);
    Inspect->SetStringField(TEXT("lifeStage"), TEXT("young")); Inspect->SetNumberField(TEXT("age"), 98);
    UI->ReceiveEvent(Inspect);
    TestTrue(TEXT("Inspection uses the other character's appearance, never a self portrait"), UI->PortraitAppearance() == OtherAppearance);
    TestEqual(TEXT("Inspect chooses the public stage and ignores private exact-age fields"), UI->PortraitAge(), 6.0);
    Inspect->RemoveField(TEXT("appearance"));
    UI->ReceiveEvent(Inspect);
    TestFalse(TEXT("Inspecting an object clears any stale character portrait"), UI->PortraitAppearance().IsValid());
    UI->LeaveCharacter();
    TestTrue(TEXT("Character selection emits leave instead of logging out account"),
             Commands.Last().Contains(TEXT("\"type\":\"character_leave\"")));
    return true;
}
#endif
