#include "UI/SRatwFrontDoor.h"
#include "UI/SRatwWolfDoll.h"
#include "Runtime/RatwJson.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateColorBrush.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
FLinearColor Color(uint32 C)
{
    return FLinearColor(FColor((C >> 16) & 255, (C >> 8) & 255, C & 255));
}
const FLinearColor Ink = Color(0x11191b), Panel = Color(0x182122), Raised = Color(0x29372f);
const FLinearColor Paper = Color(0xded5c3), Muted = Color(0x9aa99f), Gold = Color(0xd9b67b), Sage = Color(0xa8c2a6);
const TCHAR* ColorNames[] = {TEXT("Ivory"), TEXT("Silver"),   TEXT("Ash"),  TEXT("Stone"),
                             TEXT("Sable"), TEXT("Charcoal"), TEXT("Rust"), TEXT("Sand")};
const uint32 CoatColors[] = {0xE1D9C6, 0xADB3B2, 0x777D7B, 0x8E8271, 0x65513F, 0x303534, 0xA26843, 0xBEAA84};
const FButtonStyle* FlatButtonStyle()
{
    // The core button has a dark textured brush; multiplying it by a coat tint
    // produces a misleading near-black palette. White brushes preserve the tint.
    static const FButtonStyle Style = [] {
        FButtonStyle Result = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>(TEXT("Button"));
        Result.SetNormal(FSlateColorBrush(FLinearColor::White));
        Result.SetHovered(FSlateColorBrush(FLinearColor(1.08f, 1.08f, 1.08f, 1.f)));
        Result.SetPressed(FSlateColorBrush(FLinearColor::White));
        Result.SetDisabled(FSlateColorBrush(FLinearColor(1.f, 1.f, 1.f, .35f)));
        return Result;
    }();
    return &Style;
}
FString Read(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, const FString& Default = TEXT(""))
{
    FString Value;
    return O && O->TryGetStringField(Field, Value) ? Value : Default;
}
double Number(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, double Default = 0)
{
    double Value = Default;
    return O && O->TryGetNumberField(Field, Value) && FMath::IsFinite(Value) ? Value : Default;
}
TSharedPtr<FJsonObject> Appearance(const TSharedPtr<FJsonObject>& Character)
{
    const TSharedPtr<FJsonObject>* Value = nullptr;
    return Character && Character->TryGetObjectField(TEXT("appearance"), Value) ? *Value : nullptr;
}
FText Txt(const FString& Text)
{
    return FText::FromString(Text);
}
FSlateFontInfo Font(int32 Size, bool Bold = false)
{
    return FCoreStyle::GetDefaultFontStyle(Bold ? TEXT("Bold") : TEXT("Regular"), Size);
}
TSharedRef<STextBlock> Label(const FString& Text, int32 Size = 13, FLinearColor Tint = Paper)
{
    return SNew(STextBlock).Text(Txt(Text)).Font(Font(Size)).ColorAndOpacity(Tint).AutoWrapText(true);
}
FString LifeStage(double Age)
{
    return Age < 13 ? TEXT("Young") : Age < 18 ? TEXT("Adolescent") : Age < 65 ? TEXT("Adult") : TEXT("Old");
}
FString Title(const FString& Value)
{
    return Value.IsEmpty() ? Value : Value.Left(1).ToUpper() + Value.Mid(1);
}
} // namespace

void SRatwFrontDoor::Construct(const FArguments& Args)
{
    Command = Args._OnCommand;
    Choices.Add(TEXT("species"), {MakeShared<FString>(TEXT("timber")), MakeShared<FString>(TEXT("maned")),
                                  MakeShared<FString>(TEXT("arctic")), MakeShared<FString>(TEXT("red")),
                                  MakeShared<FString>(TEXT("ethiopian"))});
    Choices.Add(TEXT("sex"), {MakeShared<FString>(TEXT("female")), MakeShared<FString>(TEXT("male"))});
    Choices.Add(TEXT("stature"), {MakeShared<FString>(TEXT("short")), MakeShared<FString>(TEXT("average")),
                                  MakeShared<FString>(TEXT("tall"))});
    Choices.Add(TEXT("pattern"), {MakeShared<FString>(TEXT("solid")), MakeShared<FString>(TEXT("saddle")),
                                  MakeShared<FString>(TEXT("mantle")), MakeShared<FString>(TEXT("piebald"))});
    ChildSlot
        [SNew(SBorder)
             .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
             .BorderBackgroundColor(
                 Ink)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride(1440).HeightOverride(
                 940)[SNew(SBorder)
                          .Padding(42)
                          .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                          .BorderBackgroundColor(Ink)
                              [SNew(SVerticalBox) +
                               SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 24)
                                   [SNew(SHorizontalBox) +
                                    SHorizontalBox::Slot().AutoWidth().Padding(0,
                                                                               0, 20, 0)[Label(TEXT("W›"), 48, Gold)] +
                                    SHorizontalBox::Slot().FillWidth(1)
                                        [SNew(SVerticalBox) +
                                         SVerticalBox::Slot().AutoHeight()[Label(TEXT("RUNS AGAINST THE WORLD"), 29)] +
                                         SVerticalBox::Slot().AutoHeight().Padding(0, 7, 0, 0)[Label(
                                             TEXT("A living world. A story of your own."), 14, Muted)]] +
                                    SHorizontalBox::Slot().AutoWidth().VAlign(
                                        VAlign_Center)[Label(TEXT("NATIVE CLIENT  /  EARLY DEVELOPMENT"), 11, Gold)]] +
                               SVerticalBox::Slot().FillHeight(1)[SAssignNew(Body, SBox)] +
                               SVerticalBox::Slot().AutoHeight().Padding(
                                   0, 17, 0, 0)[SNew(STextBlock)
                                                    .Text_Lambda([this]() { return Txt(Message); })
                                                    .ColorAndOpacity_Lambda([this]() { return bError ? Gold : Sage; })
                                                    .Font(Font(13))
                                                    .AutoWrapText(true)
                                                    .MinDesiredWidth(1200)] +
                               SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)[Label(
                                   TEXT("LOCAL TEST ACCOUNTS ONLY · Network transport is not production TLS. Use a "
                                        "unique test password, never a real one."),
                                   11, Muted)]]]]]];
    Show(TEXT("login"));
}

TSharedRef<SWidget> SRatwFrontDoor::Button(const FString& Text, TFunction<void()> Action, bool Primary)
{
    return SNew(SButton)
        .ButtonStyle(FlatButtonStyle())
        .ContentPadding(FMargin(18, 12))
        .ButtonColorAndOpacity(Primary ? Gold : Raised)
        .IsEnabled_Lambda([this]() { return !bBusy; })
        .OnClicked_Lambda([Action]() {
            Action();
            return FReply::Handled();
        })[SNew(STextBlock).Text(Txt(Text)).Font(Font(13, true)).ColorAndOpacity(Primary ? Ink : Paper)];
}

void SRatwFrontDoor::Show(const FString& Next)
{
    Page = Next;
    if (Page == TEXT("login") || Page == TEXT("register"))
        Body->SetContent(LoginPage());
    else if (Page == TEXT("roster"))
        Body->SetContent(RosterPage());
    else
        Body->SetContent(CreatorPage(Page == TEXT("review")));
}

void SRatwFrontDoor::SetPresentationPage(const FString& Next)
{
    if (Next == TEXT("creator"))
        StartCreation();
    else if (Next == TEXT("roster") && Stage == TEXT("characters"))
        Show(Next);
    else if (Next == TEXT("review") && DraftAppearance)
        Review();
    else if ((Next == TEXT("login") || Next == TEXT("register")) && Stage == TEXT("login"))
        Show(Next);
}

void SRatwFrontDoor::SetCharacterDraft(const FString& Name, int32 Age, TSharedPtr<FJsonObject> Value)
{
    ratw::Appearance Parsed;
    if (bBusy || Age < 6 || Age > 99 || !ratwjson::ReadAppearance(Value, Parsed))
        return;
    DraftName = Name.TrimStartAndEnd().Left(32);
    DraftAge = Age;
    DraftAppearance = ratwjson::Appearance(Parsed); // Copy; preview edits never mutate caller/server data.
    CreationRequestId.Empty();
    CreationFingerprint.Empty();
    SetMessage(TEXT("Appearance preview only. Review and confirm to create this character."));
    Show(TEXT("creator"));
}

void SRatwFrontDoor::SetMessage(const FString& Text, bool Error)
{
    Message = Text;
    bError = Error;
}

void SRatwFrontDoor::Send(const TSharedRef<FJsonObject>& Object)
{
    if (bBusy || !Command)
        return;
    bBusy = true;
    SentAt = FPlatformTime::Seconds();
    FString Json;
    FJsonSerializer::Serialize(Object, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json));
    Command(Json);
}

void SRatwFrontDoor::ReceiveEvent(const TSharedPtr<FJsonObject>& Event)
{
    if (Read(Event, TEXT("type")) != TEXT("lobby"))
        return;
    bBusy = false;
    bool Ok = false;
    Event->TryGetBoolField(TEXT("ok"), Ok);
    const FString NextStage = Read(Event, TEXT("stage"), TEXT("login"));
    SetMessage(Read(Event, TEXT("message")), !Ok);
    if (NextStage == TEXT("characters"))
    {
        if (Password)
            Password->SetText(FText::GetEmpty());
        Characters.Empty();
        const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
        if (Event->TryGetArrayField(TEXT("characters"), Rows))
            for (const auto& Row : *Rows)
                if (Row.IsValid() && Row->Type == EJson::Object && Characters.Num() < 6)
                    Characters.Add(Row->AsObject());
        if (!SelectedCharacter() && !Characters.IsEmpty())
            SelectedId = Read(Characters[0], TEXT("id"));
        if (Ok && Page == TEXT("review"))
            for (const auto& Character : Characters)
                if (Read(Character, TEXT("name")) == DraftName)
                    SelectedId = Read(Character, TEXT("id"));
        const bool KeepDraft =
            !Ok && Stage == TEXT("characters") && (Page == TEXT("creator") || Page == TEXT("review"));
        Stage = NextStage;
        if (!KeepDraft)
            Show(TEXT("roster"));
    }
    else
    {
        const bool WasAuthenticated = Stage == TEXT("characters");
        Stage = TEXT("login");
        Characters.Empty();
        SelectedId.Empty();
        if (WasAuthenticated || (Page != TEXT("login") && Page != TEXT("register")))
        {
            DraftAppearance.Reset();
            DraftName.Empty();
            Show(TEXT("login"));
        }
        if (Password)
            Password->SetText(FText::GetEmpty());
    }
}

void SRatwFrontDoor::Tick(const FGeometry& Geometry, double Time, float Delta)
{
    SCompoundWidget::Tick(Geometry, Time, Delta);
    if (bBusy && FPlatformTime::Seconds() - SentAt > 20)
    {
        bBusy = false;
        SetMessage(TEXT("No response from the authority yet. Check your connection before retrying; a request may "
                        "still complete."),
                   true);
    }
}

FReply SRatwFrontDoor::OnKeyDown(const FGeometry&, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::Escape && !bBusy)
    {
        if (Page == TEXT("review"))
            Show(TEXT("creator"));
        else if (Page == TEXT("creator"))
            Show(TEXT("roster"));
        else if (Page == TEXT("register"))
            Show(TEXT("login"));
        return FReply::Handled();
    }
    return FReply::Unhandled();
}

TSharedRef<SWidget> SRatwFrontDoor::LoginPage()
{
    const bool Register = Page == TEXT("register");
    return SNew(SHorizontalBox) +
           SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 42, 0)
               [SNew(SBorder)
                    .Padding(34)
                    .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                    .BorderBackgroundColor(Panel)
                        [SNew(SVerticalBox) +
                         SVerticalBox::Slot().AutoHeight()[Label(TEXT("YOUR STORY BEGINS WITH A WOLF"), 12, Gold)] +
                         SVerticalBox::Slot().AutoHeight().Padding(
                             0, 22, 0, 0)[Label(TEXT("A name. A voice.\nA place in the world."), 38)] +
                         SVerticalBox::Slot().AutoHeight().Padding(0, 26, 0, 0)[Label(
                             TEXT("Explore through simple glyphs. Express yourself through words. The character sheet "
                                  "holds the portrait; the world leaves room for your imagination."),
                             17, Muted)] +
                         SVerticalBox::Slot().FillHeight(1).VAlign(VAlign_Center)[Label(
                             TEXT("W >   ·   ·   ·   #   +   #\n\nROLEPLAY  /  EXPLORATION  /  BELONGING"), 21, Sage)] +
                         SVerticalBox::Slot()
                             .AutoHeight()[Label(TEXT("Your account holds up to six characters. Characters are "
                                                      "persistent; choosing another does not erase their story."),
                                                 13, Muted)]]] +
           SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(
               490)[SNew(SBorder)
                        .Padding(34)
                        .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                        .BorderBackgroundColor(
                            Panel)[SNew(SVerticalBox) +
                                   SVerticalBox::Slot().AutoHeight()[Label(
                                       Register ? TEXT("Create a test account") : TEXT("Welcome back"), 29)] +
                                   SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 22)[Label(
                                       Register ? TEXT("An account is private. Character names are what others see in "
                                                       "the world.")
                                                : TEXT("Sign in, then choose whose story you will continue."),
                                       14, Muted)] +
                                   SVerticalBox::Slot().AutoHeight()[Label(TEXT("ACCOUNT NAME"), 11, Gold)] +
                                   SVerticalBox::Slot().AutoHeight().Padding(
                                       0, 9, 0, 20)[SAssignNew(Username, SEditableTextBox)
                                                        .Font(Font(18))
                                                        .Padding(FMargin(10))
                                                        .HintText(Txt(TEXT("3–32 letters, digits, _ or -")))
                                                        .IsEnabled_Lambda([this]() { return !bBusy; })] +
                                   SVerticalBox::Slot().AutoHeight()[Label(TEXT("TEST PASSWORD"), 11, Gold)] +
                                   SVerticalBox::Slot().AutoHeight().Padding(0, 9, 0, 24)
                                       [SAssignNew(Password, SEditableTextBox)
                                            .Font(Font(18))
                                            .Padding(FMargin(10))
                                            .IsPassword(true)
                                            .HintText(Txt(TEXT("Unique test password · 12–128 bytes")))
                                            .IsEnabled_Lambda([this]() { return !bBusy; })
                                            .OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type Method) {
                                                if (Method == ETextCommit::OnEnter)
                                                    SubmitAuth();
                                            })] +
                                   SVerticalBox::Slot().AutoHeight()[Button(
                                       Register ? TEXT("CREATE ACCOUNT") : TEXT("SIGN IN"), [this]() { SubmitAuth(); },
                                       true)] +
                                   SVerticalBox::Slot().AutoHeight().Padding(0, 14, 0, 0)[Button(
                                       Register ? TEXT("Back to sign in") : TEXT("New here? Create an account"),
                                       [this, Register]() {
                                           if (Password)
                                               Password->SetText(FText::GetEmpty());
                                           SetMessage(TEXT(""));
                                           Show(Register ? TEXT("login") : TEXT("register"));
                                       })]]]];
}

void SRatwFrontDoor::SubmitAuth()
{
    if (bBusy || !Username || !Password)
        return;
    const FString User = Username->GetText().ToString().TrimStartAndEnd();
    const FString Secret = Password->GetText().ToString();
    if (User.IsEmpty() || Secret.IsEmpty())
    {
        SetMessage(TEXT("Enter an account name and a test password."), true);
        return;
    }
    const FTCHARToUTF8 Utf8Secret(*Secret);
    if (User.Len() < 3 || User.Len() > 32 || Utf8Secret.Length() < 12 || Utf8Secret.Length() > 128)
    {
        SetMessage(TEXT("Account names need 3–32 characters; test passwords need 12–128 UTF-8 bytes."), true);
        return;
    }
    auto Object = MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("type"), Page == TEXT("register") ? TEXT("auth_register") : TEXT("auth_login"));
    Object->SetStringField(TEXT("username"), User);
    Object->SetStringField(TEXT("password"), Secret);
    Password->SetText(FText::GetEmpty());
    SetMessage(TEXT("Contacting the world authority…"));
    Send(Object);
}

TSharedPtr<FJsonObject> SRatwFrontDoor::SelectedCharacter() const
{
    for (const auto& Character : Characters)
        if (Read(Character, TEXT("id")) == SelectedId)
            return Character;
    return nullptr;
}

TSharedRef<SWidget> SRatwFrontDoor::RosterPage()
{
    auto Slots = SNew(SVerticalBox);
    Slots->AddSlot().AutoHeight().Padding(0, 0, 0, 15)[Label(TEXT("CHOOSE YOUR CHARACTER"), 12, Gold)];
    for (int32 Index = 0; Index < 6; ++Index)
    {
        const auto Character = Characters.IsValidIndex(Index) ? Characters[Index] : nullptr;
        const FString Id = Read(Character, TEXT("id"));
        const FString Text = Character ? Read(Character, TEXT("name")) + TEXT("\n") +
                                             FString::Printf(TEXT("Age %.0f · %s"), Number(Character, TEXT("age"), 18),
                                                             *LifeStage(Number(Character, TEXT("age"), 18)))
                                       : FString::Printf(TEXT("+  Create a character\nEmpty slot %d of 6"), Index + 1);
        Slots->AddSlot().AutoHeight().Padding(
            0, 0, 0, 9)[SNew(SButton)
                            .ButtonStyle(FlatButtonStyle())
                            .ContentPadding(FMargin(20, 13))
                            .ButtonColorAndOpacity_Lambda(
                                [this, Id]() { return !Id.IsEmpty() && Id == SelectedId ? Gold : Raised; })
                            .IsEnabled_Lambda([this]() { return !bBusy; })
                            .OnClicked_Lambda([this, Id]() {
                                if (Id.IsEmpty())
                                    StartCreation();
                                else
                                    SelectedId = Id;
                                return FReply::Handled();
                            })[SNew(STextBlock).Text(Txt(Text)).Font(Font(15)).ColorAndOpacity_Lambda([this, Id]() {
                                return !Id.IsEmpty() && Id == SelectedId ? Ink : Paper;
                            })]];
    }
    Slots->AddSlot().FillHeight(1)[SNew(SBox)];
    Slots->AddSlot().AutoHeight().Padding(0, 12, 0, 0)[Button(TEXT("Sign out of account"), [this]() {
        auto Object = MakeShared<FJsonObject>();
        Object->SetStringField(TEXT("type"), TEXT("auth_logout"));
        SetMessage(TEXT("Signing out…"));
        Send(Object);
    })];
    return SNew(SHorizontalBox) +
           SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 30, 0)[SNew(SBox).WidthOverride(390)[Slots]] +
           SHorizontalBox::Slot().FillWidth(
               1)[SNew(SBorder)
                      .Padding(30)
                      .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                      .BorderBackgroundColor(Panel)
                          [SNew(SVerticalBox) +
                           SVerticalBox::Slot().AutoHeight()[SNew(STextBlock)
                                                                 .Text_Lambda([this]() {
                                                                     return Txt(Read(SelectedCharacter(), TEXT("name"),
                                                                                     TEXT("An unwritten story")));
                                                                 })
                                                                 .Font(Font(34))
                                                                 .ColorAndOpacity(Paper)] +
                           SVerticalBox::Slot().AutoHeight().Padding(
                               0, 10, 0, 0)[SNew(STextBlock)
                                                .Text_Lambda([this]() {
                                                    auto C = SelectedCharacter();
                                                    return Txt(
                                                        C ? FString::Printf(TEXT("%s · age %.0f · %s wolf"),
                                                                            *LifeStage(Number(C, TEXT("age"), 18)),
                                                                            Number(C, TEXT("age"), 18),
                                                                            *Title(Read(Appearance(C), TEXT("species"),
                                                                                        TEXT("timber"))))
                                                          : TEXT("Choose an empty slot to make your first wolf."));
                                                })
                                                .Font(Font(15))
                                                .ColorAndOpacity(Muted)] +
                           SVerticalBox::Slot().FillHeight(
                               1)[SNew(SRatwWolfDoll)
                                      .Appearance_Lambda([this]() { return Appearance(SelectedCharacter()); })
                                      .Age_Lambda([this]() { return Number(SelectedCharacter(), TEXT("age"), 18); })
                                      .Visibility_Lambda([this]() {
                                          return SelectedCharacter() ? EVisibility::HitTestInvisible
                                                                     : EVisibility::Hidden;
                                      })] +
                           SVerticalBox::Slot().AutoHeight().Padding(
                               0, 15, 0, 20)[Label(TEXT("The portrait belongs to the sheet. On the map, your presence "
                                                        "stays W> — simple, expressive, and yours to imagine."),
                                                   14, Muted)] +
                           SVerticalBox::Slot().AutoHeight()[SNew(SBox).IsEnabled_Lambda([this]() {
                               return SelectedCharacter().IsValid();
                           })[Button(TEXT("ENTER THE WORLD"), [this]() { EnterCharacter(); }, true)]]]];
}

void SRatwFrontDoor::EnterCharacter()
{
    if (!SelectedCharacter() || bBusy)
        return;
    auto Object = MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("type"), TEXT("character_enter"));
    Object->SetStringField(TEXT("id"), SelectedId);
    SetMessage(TEXT("Joining the world…"));
    Send(Object);
}

void SRatwFrontDoor::StartCreation()
{
    if (bBusy || Characters.Num() >= 6)
        return;
    DraftName.Empty();
    DraftAge = 18;
    CreationRequestId.Empty();
    CreationFingerprint.Empty();
    DraftAppearance = MakeShared<FJsonObject>();
    DraftAppearance->SetStringField(TEXT("species"), TEXT("timber"));
    DraftAppearance->SetStringField(TEXT("sex"), TEXT("female"));
    DraftAppearance->SetStringField(TEXT("stature"), TEXT("average"));
    DraftAppearance->SetStringField(TEXT("pattern"), TEXT("solid"));
    DraftAppearance->SetNumberField(TEXT("baseColor"), 2);
    DraftAppearance->SetNumberField(TEXT("gradientColor"), 0);
    DraftAppearance->SetNumberField(TEXT("markingColor"), 5);
    DraftAppearance->SetNumberField(TEXT("gradientAmount"), .35);
    DraftAppearance->SetNumberField(TEXT("patternAmount"), .65);
    SetMessage(TEXT("Appearance changes are a live preview. Nothing is saved until you confirm creation."));
    Show(TEXT("creator"));
}

TSharedRef<SWidget> SRatwFrontDoor::Choice(const FString& Field, const FString& Caption)
{
    auto& Options = Choices.FindChecked(Field);
    TSharedPtr<FString> Selected;
    for (const auto& Option : Options)
        if (*Option == Read(DraftAppearance, *Field))
            Selected = Option;
    return SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 7)[Label(Caption, 11, Gold)] +
           SVerticalBox::Slot().AutoHeight()
               [SNew(SComboBox<TSharedPtr<FString>>)
                    .OptionsSource(&Options)
                    .InitiallySelectedItem(Selected)
                    .ContentPadding(FMargin(10))
                    .IsEnabled_Lambda([this]() { return !bBusy; })
                    .OnGenerateWidget_Lambda(
                        [](TSharedPtr<FString> Value) { return Label(Value ? Title(*Value) : TEXT(""), 15); })
                    .OnSelectionChanged_Lambda([this, Field](TSharedPtr<FString> Value, ESelectInfo::Type) {
                        if (Value && DraftAppearance)
                            DraftAppearance->SetStringField(Field, *Value);
                    })[SNew(STextBlock)
                           .Text_Lambda([this, Field]() { return Txt(Title(Read(DraftAppearance, *Field))); })
                           .Font(Font(15))
                           .ColorAndOpacity(Paper)]];
}

TSharedRef<SWidget> SRatwFrontDoor::Palette(const FString& Field, const FString& Caption)
{
    auto Row = SNew(SHorizontalBox);
    for (int32 Index = 0; Index < 8; ++Index)
        Row->AddSlot().FillWidth(1).Padding(
            0, 0, 5,
            0)[SNew(SButton)
                   .ButtonStyle(FlatButtonStyle())
                   .ContentPadding(FMargin(2, 10))
                   .ButtonColorAndOpacity(Color(CoatColors[Index]))
                   .ToolTipText(Txt(Caption + TEXT(": ") + ColorNames[Index]))
                   .IsEnabled_Lambda([this]() { return !bBusy; })
                   .OnClicked_Lambda([this, Field, Index]() {
                       DraftAppearance->SetNumberField(Field, Index);
                       return FReply::Handled();
                   })[SNew(STextBlock)
                          .Text_Lambda([this, Field, Index]() {
                              return Txt((Number(DraftAppearance, *Field) == Index ? FString(TEXT("✓ ")) : FString()) +
                                         ColorNames[Index]);
                          })
                          .Font(Font(10, true))
                          .ColorAndOpacity(Index == 4 || Index == 5 ? FLinearColor::White : FLinearColor::Black)]];
    return SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[Label(Caption, 11, Gold)] +
           SVerticalBox::Slot().AutoHeight()[Row];
}

TSharedRef<SWidget> SRatwFrontDoor::Amount(const FString& Field, const FString& Caption)
{
    return SNew(SVerticalBox) +
           SVerticalBox::Slot().AutoHeight().Padding(
               0, 0, 0, 7)[SNew(STextBlock)
                               .Text_Lambda([this, Field, Caption]() {
                                   return Txt(Caption + FString::Printf(
                                                            TEXT(" · %d%%"),
                                                            FMath::RoundToInt(Number(DraftAppearance, *Field) * 100)));
                               })
                               .Font(Font(11))
                               .ColorAndOpacity(Gold)] +
           SVerticalBox::Slot()
               .AutoHeight()[SNew(SSlider)
                                 .Value_Lambda([this, Field]() { return float(Number(DraftAppearance, *Field)); })
                                 .StepSize(.05f)
                                 .SliderBarColor(Muted)
                                 .SliderHandleColor(Gold)
                                 .ToolTipText(Txt(Caption + TEXT(" (arrow keys adjust)")))
                                 .OnValueChanged_Lambda(
                                     [this, Field](float Value) { DraftAppearance->SetNumberField(Field, Value); })];
}

TSharedRef<SWidget> SRatwFrontDoor::CreatorPage(bool ReviewOnly)
{
    auto Fields = SNew(SVerticalBox);
    if (!ReviewOnly)
    {
        Fields->AddSlot().AutoHeight()[Label(TEXT("CHARACTER NAME"), 11, Gold)];
        Fields->AddSlot().AutoHeight().Padding(
            0, 8, 0,
            20)[SNew(SEditableTextBox)
                    .Text(Txt(DraftName))
                    .HintText(Txt(TEXT("The name others will know")))
                    .Font(Font(19))
                    .Padding(FMargin(10))
                    .IsEnabled_Lambda([this]() { return !bBusy; })
                    .OnTextChanged_Lambda([this](const FText& Text) { DraftName = Text.ToString().Left(64); })];
        Fields->AddSlot().AutoHeight().Padding(
            0, 0, 0,
            18)[SNew(SHorizontalBox) +
                SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 18, 0)[Choice(TEXT("species"), TEXT("SPECIES"))] +
                SHorizontalBox::Slot().FillWidth(1)[Choice(TEXT("sex"), TEXT("SEX"))]];
        Fields->AddSlot().AutoHeight().Padding(
            0, 0, 0,
            18)[SNew(SHorizontalBox) +
                SHorizontalBox::Slot().FillWidth(1).Padding(
                    0, 0, 18, 0)[SNew(SVerticalBox) +
                                 SVerticalBox::Slot().AutoHeight().Padding(
                                     0, 0, 0, 7)[Label(TEXT("STARTING AGE · 6–99"), 11, Gold)] +
                                 SVerticalBox::Slot().AutoHeight()[SNew(SSpinBox<int32>)
                                                                       .MinValue(6)
                                                                       .MaxValue(99)
                                                                       .MinSliderValue(6)
                                                                       .MaxSliderValue(99)
                                                                       .Value_Lambda([this]() { return DraftAge; })
                                                                       .Font(Font(15))
                                                                       .ContentPadding(FMargin(9))
                                                                       .OnValueChanged_Lambda([this](int32 Value) {
                                                                           DraftAge = FMath::Clamp(Value, 6, 99);
                                                                       })]] +
                SHorizontalBox::Slot().FillWidth(1)[Choice(TEXT("stature"), TEXT("STATURE · APPEARANCE ONLY"))]];
        Fields->AddSlot().AutoHeight().Padding(0, 0, 0, 18)[Palette(TEXT("baseColor"), TEXT("BASE COAT"))];
        Fields->AddSlot().AutoHeight().Padding(0, 0, 0, 14)[Palette(TEXT("gradientColor"), TEXT("GRADIENT COLOR"))];
        Fields->AddSlot().AutoHeight().Padding(0, 0, 0, 21)[Amount(TEXT("gradientAmount"), TEXT("GRADIENT STRENGTH"))];
        Fields->AddSlot().AutoHeight().Padding(0, 0, 0, 18)[Choice(TEXT("pattern"), TEXT("MARKING PATTERN"))];
        Fields->AddSlot().AutoHeight().Padding(0, 0, 0, 14)[Palette(TEXT("markingColor"), TEXT("MARKING COLOR"))];
        Fields->AddSlot().AutoHeight().Padding(0, 0, 0, 20)[Amount(TEXT("patternAmount"), TEXT("MARKING STRENGTH"))];
    }
    else
    {
        Fields->AddSlot().AutoHeight().Padding(0, 0, 0, 22)[Label(TEXT("Review your wolf"), 29)];
        Fields->AddSlot().AutoHeight().Padding(0, 0, 0, 25)[Label(DraftName, 25, Gold)];
        Fields->AddSlot().AutoHeight().Padding(0, 0, 0, 24)[Label(
            FString::Printf(TEXT("%s wolf · %s\nAge %d · %s\n%s stature · %s markings"),
                            *Title(Read(DraftAppearance, TEXT("species"))), *Title(Read(DraftAppearance, TEXT("sex"))),
                            DraftAge, *LifeStage(DraftAge), *Title(Read(DraftAppearance, TEXT("stature"))),
                            *Title(Read(DraftAppearance, TEXT("pattern")))),
            19)];
        for (const FString& Field :
             {FString(TEXT("baseColor")), FString(TEXT("gradientColor")), FString(TEXT("markingColor"))})
        {
            const FString Caption = Field == TEXT("baseColor")       ? TEXT("Base coat")
                                    : Field == TEXT("gradientColor") ? TEXT("Gradient")
                                                                     : TEXT("Markings");
            Fields->AddSlot().AutoHeight().Padding(
                0, 0, 0,
                12)[Label(Caption + TEXT(": ") + ColorNames[FMath::Clamp(int32(Number(DraftAppearance, *Field)), 0, 7)],
                          15, Muted)];
        }
        Fields->AddSlot().AutoHeight().Padding(
            0, 15, 0,
            0)[Label(TEXT("Creation saves this character to your account. You will return to character selection "
                          "before entering the world. Appearance does not grant free skill or stat bonuses."),
                     16, Muted)];
    }
    return SNew(SHorizontalBox) +
           SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 26, 0)[SNew(SBox).WidthOverride(
               515)[SNew(SBorder)
                        .Padding(26)
                        .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                        .BorderBackgroundColor(Panel)
                            [SNew(SVerticalBox) +
                             SVerticalBox::Slot().AutoHeight()[Label(
                                 ReviewOnly ? TEXT("CREATION REVIEW") : TEXT("LIVE APPEARANCE PREVIEW"), 12, Gold)] +
                             SVerticalBox::Slot().AutoHeight().Padding(0, 20, 0, 0)[SNew(STextBlock)
                                                                                        .Text_Lambda([this]() {
                                                                                            return Txt(
                                                                                                DraftName.IsEmpty()
                                                                                                    ? TEXT("Your wolf")
                                                                                                    : DraftName);
                                                                                        })
                                                                                        .Font(Font(30))
                                                                                        .ColorAndOpacity(Paper)
                                                                                        .AutoWrapText(true)] +
                             SVerticalBox::Slot().AutoHeight().Padding(
                                 0, 10, 0, 0)[SNew(STextBlock)
                                                  .Text_Lambda([this]() {
                                                      ratw::Appearance Parsed;
                                                      ratwjson::ReadAppearance(DraftAppearance, Parsed);
                                                      return Txt(FString::Printf(
                                                          TEXT("%s · age %d\n%s stature · %.0f cm at shoulder"),
                                                          *LifeStage(DraftAge), DraftAge,
                                                          *Title(Read(DraftAppearance, TEXT("stature"))),
                                                          ratw::shoulderHeightCm(Parsed, DraftAge)));
                                                  })
                                                  .Font(Font(16))
                                                  .ColorAndOpacity(Sage)] +
                             SVerticalBox::Slot().FillHeight(
                                 1)[SNew(SRatwWolfDoll)
                                        .Appearance_Lambda([this]() { return DraftAppearance; })
                                        .Age_Lambda([this]() { return double(DraftAge); })] +
                             SVerticalBox::Slot().AutoHeight()[Label(
                                 TEXT("Young 6–12 · Adolescent 13–17\nAdult 18–64 · Old 65+"), 13, Muted)] +
                             SVerticalBox::Slot().AutoHeight().Padding(
                                 0, 18, 0, 0)[Label(TEXT("A static sheet portrait. Map characters remain simple W> "
                                                         "glyphs; posture and storytelling stay in your hands."),
                                                    13, Muted)]]]] +
           SHorizontalBox::Slot().FillWidth(
               1)[SNew(SBorder)
                      .Padding(25)
                      .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                      .BorderBackgroundColor(
                          Panel)[SNew(SVerticalBox) +
                                 SVerticalBox::Slot().FillHeight(
                                     1)[SNew(SScrollBox) + SScrollBox::Slot().Padding(0, 0, 15, 8)[Fields]] +
                                 SVerticalBox::Slot().AutoHeight().Padding(
                                     0, 18, 0, 0)[SNew(SHorizontalBox) +
                                                  SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 16, 0)[Button(
                                                      ReviewOnly ? TEXT("Back to appearance") : TEXT("Cancel"),
                                                      [this, ReviewOnly]() {
                                                          Show(ReviewOnly ? TEXT("creator") : TEXT("roster"));
                                                      })] +
                                                  SHorizontalBox::Slot().FillWidth(1)[Button(
                                                      ReviewOnly ? TEXT("CONFIRM & CREATE") : TEXT("REVIEW CHARACTER"),
                                                      [this, ReviewOnly]() {
                                                          if (ReviewOnly)
                                                              CreateCharacter();
                                                          else
                                                              Review();
                                                      },
                                                      true)]]]];
}

void SRatwFrontDoor::Review()
{
    DraftName = DraftName.TrimStartAndEnd();
    if (DraftName.Len() < 2 || DraftName.Len() > 32)
    {
        SetMessage(TEXT("Choose a character name between 2 and 32 characters. The authority validates the final name."),
                   true);
        return;
    }
    SetMessage(
        TEXT("Review your choices. Confirming creates a persistent character; it does not enter the world yet."));
    Show(TEXT("review"));
}

void SRatwFrontDoor::CreateCharacter()
{
    if (bBusy || !DraftAppearance || Page != TEXT("review"))
        return;
    auto Object = MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("type"), TEXT("character_create"));
    Object->SetStringField(TEXT("name"), DraftName);
    Object->SetNumberField(TEXT("age"), DraftAge);
    Object->SetObjectField(TEXT("appearance"), DraftAppearance);
    // A timeout is an uncertain result, not a new creation intent. Keep the same
    // server receipt key for an unchanged draft; a revised/new draft gets a new key.
    FString Fingerprint;
    FJsonSerializer::Serialize(Object,
                               TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Fingerprint));
    if (CreationRequestId.IsEmpty() || CreationFingerprint != Fingerprint)
    {
        CreationRequestId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
        CreationFingerprint = Fingerprint;
    }
    Object->SetStringField(TEXT("commandId"), CreationRequestId);
    SetMessage(TEXT("Creating your character…"));
    Send(Object);
}
