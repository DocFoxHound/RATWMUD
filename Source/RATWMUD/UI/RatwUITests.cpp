#include "UI/SRatwGame.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Layout/Children.h"
#include "InputCoreTypes.h"

namespace
{
FKeyEvent Key(FKey Value, bool Shift = false)
{
    return FKeyEvent(Value, FModifierKeysState(Shift, false, false, false, false, false, false, false, false), 0, false,
                     0, 0);
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
#endif
