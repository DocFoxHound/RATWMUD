#include "UI/SRatwGame.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"
#include "Fonts/CompositeFont.h"
#include "Styling/CoreStyle.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/Paths.h"
#include "InputCoreTypes.h"

namespace RatwUI
{
FLinearColor RGB(uint32 Hex, float Alpha = 1.f)
{
    return FLinearColor(FColor((Hex >> 16) & 255, (Hex >> 8) & 255, Hex & 255)).CopyWithNewOpacity(Alpha);
}
const FLinearColor Ink = RGB(0x11191b), Panel = RGB(0x182122), Raised = RGB(0x24302d);
const FLinearColor Line = RGB(0x34443e), Paper = RGB(0xded5c3), Muted = RGB(0x8b9b91);
const FLinearColor Amber = RGB(0xd9b67b), Sage = RGB(0xa8c2a6), Blue = RGB(0x92bacd);
FString Str(const TSharedPtr<FJsonObject>& O, const TCHAR* K, const FString& Default = TEXT(""))
{
    FString R;
    return O && O->TryGetStringField(K, R) ? R : Default;
}
double Num(const TSharedPtr<FJsonObject>& O, const TCHAR* K, double Default = 0)
{
    double R;
    return O && O->TryGetNumberField(K, R) ? R : Default;
}
bool Bool(const TSharedPtr<FJsonObject>& O, const TCHAR* K, bool Default = false)
{
    bool R;
    return O && O->TryGetBoolField(K, R) ? R : Default;
}
TSharedPtr<FJsonObject> Obj(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
{
    const TSharedPtr<FJsonObject>* R = nullptr;
    return O && O->TryGetObjectField(K, R) ? *R : nullptr;
}
TArray<TSharedPtr<FJsonValue>> Arr(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
{
    const TArray<TSharedPtr<FJsonValue>>* R = nullptr;
    return O && O->TryGetArrayField(K, R) ? *R : TArray<TSharedPtr<FJsonValue>>();
}
FSlateFontInfo Font(int Size, bool Mono = false, bool Bold = false)
{
    static TMap<int32, FSlateFontInfo> Fonts;
    const int32 Key = Size * 4 + (Mono ? 2 : 0) + (Bold ? 1 : 0);
    if (const auto* Cached = Fonts.Find(Key))
        return *Cached;
    static TMap<int32, TSharedPtr<const FCompositeFont>> Families;
    const int32 FamilyKey = Mono ? 2 : (Bold ? 1 : 0);
    if (!Families.Contains(FamilyKey))
        Families.Add(FamilyKey, MakeShared<FCompositeFont>(
                                    FName(TEXT("Regular")),
                                    FPaths::EngineContentDir() / TEXT("Slate/Fonts/") /
                                        (Mono ? TEXT("DroidSansMono.ttf")
                                              : (Bold ? TEXT("Roboto-Medium.ttf") : TEXT("Roboto-Regular.ttf"))),
                                    EFontHinting::Default, EFontLoadingPolicy::LazyLoad));
    const FSlateFontInfo NewFont(Families[FamilyKey], Size, FName(TEXT("Regular")));
    Fonts.Add(Key, NewFont);
    return NewFont;
}
void Box(const FGeometry& G, FSlateWindowElementList& D, int L, FVector2D P, FVector2D S, FLinearColor C)
{
    FSlateDrawElement::MakeBox(D, L, G.ToPaintGeometry(S, FSlateLayoutTransform(P)),
                               FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, C);
}
void Text(const FGeometry& G, FSlateWindowElementList& D, int L, FVector2D P, const FString& T, int Size,
          FLinearColor C, bool Mono = false, bool Bold = false)
{
    FSlateDrawElement::MakeText(D, L, G.ToPaintGeometry(FVector2D(1600, 1000), FSlateLayoutTransform(P)), T,
                                Font(Size, Mono, Bold), ESlateDrawEffect::None, C);
}
FVector2D Measure(const FString& T, int Size, bool Mono = false)
{
    return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(T, Font(Size, Mono));
}
void Lines(const FGeometry& G, FSlateWindowElementList& D, int L, const TArray<FVector2D>& Points, FLinearColor C,
           float Width = 1)
{
    FSlateDrawElement::MakeLines(D, L, G.ToPaintGeometry(), Points, ESlateDrawEffect::None, C, true, Width);
}
void Frame(const FGeometry& G, FSlateWindowElementList& D, int L, FVector2D P, FVector2D S, FLinearColor C)
{
    Lines(G, D, L, {P, P + FVector2D(S.X, 0), P + S, P + FVector2D(0, S.Y), P}, C);
}
TArray<FString> Wrap(const FString& T, float Width, int Size)
{
    TArray<FString> Result, Paragraphs;
    T.ParseIntoArray(Paragraphs, TEXT("\n"), false);
    for (const FString& P : Paragraphs)
    {
        TArray<FString> Words;
        P.ParseIntoArray(Words, TEXT(" "), false);
        FString Row;
        for (const FString& W : Words)
        {
            const FString Next = Row.IsEmpty() ? W : Row + TEXT(" ") + W;
            if (!Row.IsEmpty() && Measure(Next, Size).X > Width)
            {
                Result.Add(Row);
                Row = W;
            }
            else
                Row = Next;
        }
        Result.Add(Row);
    }
    return Result;
}
float Paragraph(const FGeometry& G, FSlateWindowElementList& D, int L, FVector2D P, const FString& T, float Width,
                int Size, FLinearColor C, float Leading = 1.65f)
{
    const auto Rows = Wrap(T, Width, Size);
    for (const FString& R : Rows)
    {
        Text(G, D, L, P, R, Size, C);
        P.Y += Size * Leading;
    }
    return Rows.Num() * Size * Leading;
}
} // namespace RatwUI
using namespace RatwUI;

FLinearColor SRatwGame::SpeakingColor(int32 Index)
{
    static const uint32 Colors[32] = {0xDAC69D, 0xA8C7AD, 0xA8CEDA, 0xCEAEE0, 0xE1ABA2, 0xE6C481, 0xAEC4E4, 0xD5AFC6,
                                      0xBECF91, 0x8BD2C6, 0xD8BAA3, 0xD0D7AD, 0x9DB9D6, 0xC5B4E4, 0xE2AEB5, 0xD7CB98,
                                      0xB2D0BE, 0xA4C9C4, 0xA8BDE2, 0xD4AFE0, 0xD7A68F, 0xD9BC76, 0x95C3A0, 0x8EC4D7,
                                      0xB5A3D2, 0xD39CC0, 0xCAB497, 0xC0CFA9, 0x97BCB5, 0xB2C7D1, 0xCCC3DC, 0xE0D6BF};
    const uint32 C = Colors[FMath::Clamp(Index, 0, 31)];
    return FLinearColor(FColor((C >> 16) & 255, (C >> 8) & 255, C & 255));
}

void SRatwGame::Construct(const FArguments& Args)
{
    Command = Args._OnCommand;
    ChildSlot
        [SNew(SOverlay) +
         SOverlay::Slot()
             .HAlign(HAlign_Left)
             .VAlign(VAlign_Top)
             .Padding(TAttribute<FMargin>::CreateLambda([this]() {
                 return FMargin(CanvasOffset.X + 54 * CanvasScale, CanvasOffset.Y + 822 * CanvasScale, 0, 0);
             }))[SNew(SBox)
                     .WidthOverride_Lambda([this]() { return (466 + StoryExtra) * CanvasScale; })
                     .HeightOverride_Lambda([this]() {
                         return 79 * CanvasScale;
                     })[SAssignNew(Composer, SMultiLineEditableTextBox)
                            .Font_Lambda([this]() { return Font(FMath::Max(10, FMath::RoundToInt(15 * CanvasScale))); })
                            .ForegroundColor(FSlateColor(Paper))
                            .BackgroundColor(FSlateColor(Ink))
                            .HintText(FText::FromString(TEXT("Press Enter to write your part in the story…")))
                            .AllowMultiLine(true)
                            .AutoWrapText(true)
                            .ModiferKeyForNewLine(EModifierKey::Shift)
                            .IsReadOnly_Lambda([this]() { return !bChat; })
                            .Visibility_Lambda(
                                [this]() { return Modal.IsEmpty() ? EVisibility::Visible : EVisibility::Hidden; })
                            .Padding(FMargin(9))
                            .RevertTextOnEscape(false)
                            .IsCaretMovedWhenGainFocus(false)
                            .OnTextChanged(this, &SRatwGame::ComposerChanged)
                            .OnKeyDownHandler(this, &SRatwGame::ComposerKey)
                            .OnBeginTextEdit_Lambda([this](const FText&) {
                                if (!bChat)
                                    SetChat(true);
                            })]]];
}

void SRatwGame::SetPresentationPage(const FString& Page)
{
    if (Page == TEXT("text-first"))
        StoryExtra = 300;
    if (Page == TEXT("balanced"))
        StoryExtra = 0;
    bWorldMap = Page == TEXT("world");
    Modal = (Page == TEXT("character") || Page == TEXT("inventory") || Page == TEXT("settings")) ? Page : TEXT("");
    ContextTarget.Empty();
}

void SRatwGame::Send(const TSharedRef<FJsonObject>& O)
{
    if (!Command)
        return;
    FString Result;
    const auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Result);
    FJsonSerializer::Serialize(O, Writer);
    Command(Result);
}
void SRatwGame::SendAction(const FString& Action, const FString& Target)
{
    auto O = MakeShared<FJsonObject>();
    O->SetStringField(TEXT("type"), TEXT("action"));
    O->SetStringField(TEXT("action"), Action);
    O->SetStringField(TEXT("target"), Target);
    Send(O);
}
void SRatwGame::ShowToast(const FString& T)
{
    Toast = T;
    ToastUntil = Clock + 5;
}

void SRatwGame::ApplySnapshot(const TSharedPtr<FJsonObject>& S)
{
    if (!S)
        return;
    Snapshot = S;
    auto Cell = Obj(S, TEXT("cell"));
    if (!Cell)
        Cell = S;
    const FString NewId = Str(Cell, TEXT("id"), Str(S, TEXT("cellId"), CellId));
    if (NewId != CellId)
    {
        HeldKeys.Empty();
        MapPan = FVector2D::ZeroVector;
        ContextTarget.Empty();
    }
    CellId = NewId;
    CellName = Str(Cell, TEXT("name"), CellName);
    SceneDescription = Str(Cell, TEXT("description"), SceneDescription);
    CellWidth = FMath::Clamp((int32)Num(Cell, TEXT("width"), 32), 1, 256);
    CellHeight = FMath::Clamp((int32)Num(Cell, TEXT("height"), 24), 1, 256);
    SelfId = Str(S, TEXT("selfId"), Str(S, TEXT("playerId"), SelfId));
    auto Self = Obj(S, TEXT("self"));
    if (Self)
    {
        SelfId = Str(Self, TEXT("id"), SelfId);
        SelectedColor = (int32)Num(Self, TEXT("color"), SelectedColor);
    }
    TileRows.Empty();
    for (auto& V : Arr(Cell, TEXT("tiles")))
        if (V->Type == EJson::String)
            TileRows.Add(V->AsString());
    if (TileRows.IsEmpty())
        for (auto& V : Arr(Cell, TEXT("rows")))
            if (V->Type == EJson::String)
                TileRows.Add(V->AsString());
    VisibilityRows.Empty();
    for (auto& V : Arr(S, TEXT("visibility")))
        if (V->Type == EJson::String)
            VisibilityRows.Add(V->AsString());
    if (TileRows.IsEmpty())
    {
        for (int32 Y = 0; Y < CellHeight; ++Y)
        {
            TileRows.Add(FString::ChrN(CellWidth, TEXT(' ')));
            VisibilityRows.Add(FString::ChrN(CellWidth, TEXT('0')));
        }
        for (auto& V : Arr(Cell, TEXT("tiles")))
        {
            if (V->Type != EJson::Object)
                continue;
            auto T = V->AsObject();
            const int X = Num(T, TEXT("x")), Y = Num(T, TEXT("y"));
            if (X < 0 || Y < 0 || X >= CellWidth || Y >= CellHeight)
                continue;
            const FString Glyph = Str(T, TEXT("glyph"), TEXT(" "));
            TileRows[Y][X] = Glyph.IsEmpty() ? TEXT(' ') : Glyph[0];
            VisibilityRows[Y][X] =
                Bool(T, TEXT("visible")) ? TEXT('2') : (Bool(T, TEXT("remembered")) ? TEXT('1') : TEXT('0'));
        }
    }
    TSet<FString> Present;
    auto Entities = Arr(S, TEXT("entities"));
    if (Self)
        Entities.Add(MakeShared<FJsonValueObject>(Self));
    for (auto& V : Entities)
    {
        auto E = V->AsObject();
        if (!E)
            continue;
        const FString Id = Str(E, TEXT("id"));
        if (Id.IsEmpty())
            continue;
        Present.Add(Id);
        const bool Existing = EntityViews.Contains(Id);
        auto& View = EntityViews.FindOrAdd(Id);
        View.Id = Id;
        View.Name = Str(E, TEXT("name"));
        View.Kind = Str(E, TEXT("kind"), Bool(E, TEXT("npc")) ? TEXT("npc") : TEXT("player"));
        View.State = Str(E, TEXT("state"), Str(E, TEXT("posture"), TEXT("standing")));
        View.Actions.Empty();
        for (const auto& A : Arr(E, TEXT("actions")))
            if (A->Type == EJson::String)
                View.Actions.Add(A->AsString());
        if (View.Actions.IsEmpty())
            View.Actions.Add(TEXT("inspect"));
        View.Target = FVector2D(Num(E, TEXT("x")), Num(E, TEXT("y")));
        if (!Existing || FVector2D::Distance(View.Target, View.Position) > 8)
            View.Position = View.Target;
        View.Facing = Num(E, TEXT("facing"));
        View.Color = (int32)Num(E, TEXT("color"));
        View.bSelf = Id == SelfId || Bool(E, TEXT("self"));
        View.bTyping = Bool(E, TEXT("typing"));
        const bool Speaking = Bool(E, TEXT("speaking"));
        if (Speaking && !View.bSpeaking)
            View.SpokenAt = Clock;
        View.bSpeaking = Speaking;
    }
    for (auto It = EntityViews.CreateIterator(); It; ++It)
        if (!Present.Contains(It.Key()))
            It.RemoveCurrent();
}

void SRatwGame::ReceiveEvent(const TSharedPtr<FJsonObject>& E)
{
    if (!E)
        return;
    const FString Type = Str(E, TEXT("type"), TEXT("system"));
    if (Type == TEXT("chatAccepted"))
    {
        PendingDrafts.Remove(Str(E, TEXT("requestId")));
        return;
    }
    if (Type == TEXT("error") && Str(E, TEXT("context")) == TEXT("chat"))
    {
        const FString RequestId = Str(E, TEXT("requestId"));
        if (const auto* Draft = PendingDrafts.Find(RequestId))
        {
            FailedDraft = *Draft;
            PendingDrafts.Remove(RequestId);
            if (Composer->GetText().IsEmpty())
            {
                Composer->SetText(FText::FromString(FailedDraft));
                FailedDraft.Empty();
            }
            ShowToast(TEXT("Post not sent. Your writing has been preserved."));
        }
    }
    if (Type == TEXT("snapshot"))
    {
        ApplySnapshot(E);
        return;
    }
    const FString EventId = Str(E, TEXT("id"));
    if (!EventId.IsEmpty() && SeenPosts.Contains(EventId))
        return;
    if (!EventId.IsEmpty())
        SeenPosts.Add(EventId);
    if (Type == TEXT("inspect"))
    {
        InspectedText = Str(E, TEXT("title")) + TEXT("\n\n") + Str(E, TEXT("description"), Str(E, TEXT("text"))) +
                        TEXT("\n\n") + Str(E, TEXT("state"));
        Modal = TEXT("inspect");
        return;
    }
    FPost P;
    P.Id = EventId;
    P.Channel = Str(E, TEXT("channel"), Type == TEXT("ooc") ? TEXT("ooc") : TEXT("ic"));
    P.Kind = Type;
    P.Speaker =
        Str(E, TEXT("speaker"), Str(E, TEXT("name"), Type == TEXT("system") ? TEXT("THE WORLD") : TEXT("A voice")));
    P.Color = (int32)Num(E, TEXT("color"));
    P.Text = Str(E, TEXT("text"));
    P.PostedAt = Clock;
    const auto Segments = Arr(E, TEXT("segments"));
    if (!Segments.IsEmpty())
    {
        P.Text.Empty();
        for (const auto& V : Segments)
        {
            const auto Part = V->AsObject();
            if (!Part)
                continue;
            const FString Body = Str(Part, TEXT("text"));
            if (Body.IsEmpty())
                continue;
            if (!P.Text.IsEmpty())
                P.Text += TEXT(" ");
            P.Text += Str(Part, TEXT("kind")) == TEXT("speech") ? TEXT("\"") + Body + TEXT("\"") : Body;
        }
    }
    if (P.Text.IsEmpty())
        return;
    P.bSystem = Type == TEXT("system") || Type == TEXT("error");
    P.Revealed = P.bSystem || P.Channel == TEXT("ooc") ? P.Text.Len() : 0;
    if (Type == TEXT("error"))
        ShowToast(P.Text);
    Posts.Add(P);
    if (Posts.Num() > 300)
        Posts.RemoveAt(0, Posts.Num() - 300);
    TranscriptScroll = 0;
    const FString Source = Str(E, TEXT("sourceId"), Str(E, TEXT("entityId")));
    if (auto* View = EntityViews.Find(Source))
        View->SpokenAt = Clock;
}

void SRatwGame::Tick(const FGeometry& G, double Time, float Delta)
{
    SCompoundWidget::Tick(G, Time, Delta);
    Clock = Time;
    const FVector2D Size = G.GetLocalSize();
    CanvasScale = FMath::Min(Size.X / 1600.0, Size.Y / 1000.0);
    CanvasOffset = (Size - FVector2D(1600, 1000) * CanvasScale) * 0.5;
    for (auto& E : EntityViews)
        E.Value.Position = FMath::Vector2DInterpTo(E.Value.Position, E.Value.Target, Delta, 16);
    if (bTypingSent && Clock - LastTyping > 3)
        SetTyping(false);
    if (bChat && Clock - LastTyping < 3 && (!bTypingSent || Clock - LastTypingSent > 1))
        SetTyping(true);
    if (!bChat && !HeldKeys.IsEmpty() && Clock - LastMove > 0.075)
        SendMove();
    for (auto& P : Posts)
        if (P.Channel == TEXT("ic") && P.Revealed < P.Text.Len())
        {
            if (RevealSpeed == 0 || bReducedMotion)
                P.Revealed = P.Text.Len();
            else
            {
                RevealFraction += Delta * RevealSpeed;
                const int32 N = FMath::FloorToInt(RevealFraction);
                RevealFraction -= N;
                P.Revealed = FMath::Min(P.Text.Len(), P.Revealed + N);
            }
            break;
        }
}

void SRatwGame::SetTyping(bool Active)
{
    Active = Active && Channel == TEXT("ic");
    if (bTypingSent == Active && (!Active || Clock - LastTypingSent < 1))
        return;
    bTypingSent = Active;
    LastTypingSent = Clock;
    auto O = MakeShared<FJsonObject>();
    O->SetStringField(TEXT("type"), TEXT("typing"));
    O->SetBoolField(TEXT("active"), Active && Channel == TEXT("ic"));
    Send(O);
}
void SRatwGame::SetChat(bool Active)
{
    bChat = Active;
    HeldKeys.Empty();
    SendMove();
    ContextTarget.Empty();
    if (Active)
    {
        Modal.Empty();
        FSlateApplication::Get().SetKeyboardFocus(Composer, EFocusCause::SetDirectly);
    }
    else
    {
        SetTyping(false);
        FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
    }
}
void SRatwGame::SubmitPost()
{
    const FString T = Composer->GetText().ToString().TrimStartAndEnd();
    if (!T.IsEmpty())
    {
        const FString Id = FString::Printf(TEXT("post_%lld"), ++NextRequestId);
        PendingDrafts.Add(Id, T);
        auto O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("type"), TEXT("chat"));
        O->SetStringField(TEXT("requestId"), Id);
        O->SetStringField(TEXT("text"), T);
        O->SetStringField(TEXT("channel"), Channel);
        O->SetStringField(TEXT("volume"), Volume);
        Composer->SetText(FText::GetEmpty());
        Send(O);
    }
    SetChat(false);
}
void SRatwGame::ComposerChanged(const FText&)
{
    if (bChat)
    {
        LastTyping = Clock;
        if (Channel == TEXT("ic"))
            SetTyping(true);
    }
}
FReply SRatwGame::ComposerKey(const FGeometry&, const FKeyEvent& E)
{
    if (E.GetKey() == EKeys::Escape)
    {
        SetChat(false);
        return FReply::Handled();
    }
    if (E.GetKey() == EKeys::Enter && E.IsShiftDown() && bChat)
    {
        Composer->InsertTextAtCursor(FString(TEXT("\n")));
        return FReply::Handled();
    }
    if (E.GetKey() == EKeys::Enter && !E.IsShiftDown())
    {
        if (bChat)
            SubmitPost();
        else
            SetChat(true);
        return FReply::Handled();
    }
    return FReply::Unhandled();
}
void SRatwGame::SendMove()
{
    const double X = (HeldKeys.Contains(EKeys::D) ? 1. : 0) - (HeldKeys.Contains(EKeys::A) ? 1. : 0),
                 Y = (HeldKeys.Contains(EKeys::S) ? 1. : 0) - (HeldKeys.Contains(EKeys::W) ? 1. : 0);
    auto O = MakeShared<FJsonObject>();
    O->SetStringField(TEXT("type"), TEXT("move"));
    O->SetNumberField(TEXT("x"), bChat ? 0 : X);
    O->SetNumberField(TEXT("y"), bChat ? 0 : Y);
    Send(O);
    LastMove = Clock;
}
FReply SRatwGame::OnKeyDown(const FGeometry&, const FKeyEvent& E)
{
    const FKey K = E.GetKey();
    if (K == EKeys::Escape)
    {
        if (!Modal.IsEmpty())
        {
            Modal.Empty();
            return FReply::Handled();
        }
        ContextTarget.Empty();
        if (bChat)
            SetChat(false);
        return FReply::Handled();
    }
    if (K == EKeys::Enter)
    {
        SetChat(true);
        return FReply::Handled();
    }
    if (!Modal.IsEmpty())
        return FReply::Handled();
    if (bChat)
        return FReply::Unhandled();
    if (!ContextTarget.IsEmpty())
    {
        const FKey Choices[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six};
        for (int I = 0; I < 6; ++I)
            if (K == Choices[I] && ContextActions.IsValidIndex(I))
            {
                SendAction(ContextActions[I], ContextTarget);
                ContextTarget.Empty();
                return FReply::Handled();
            }
    }
    if (K == EKeys::E)
    {
        FVector2D SelfPosition;
        for (const auto& P : EntityViews)
            if (P.Value.bSelf)
                SelfPosition = P.Value.Position;
        double Best = 1e9;
        FString Target;
        FVector2D Position;
        for (const auto& P : EntityViews)
            if (!P.Value.bSelf)
            {
                const double Dist = FVector2D::Distance(SelfPosition, P.Value.Position);
                if (Dist < Best)
                {
                    Best = Dist;
                    Target = P.Key;
                    Position = P.Value.Position;
                }
            }
        for (const auto& V : Arr(Snapshot, TEXT("doors")))
        {
            const auto Door = V->AsObject();
            const FVector2D DP(Num(Door, TEXT("x")), Num(Door, TEXT("y")));
            const double Dist = FVector2D::Distance(SelfPosition, DP);
            if (Dist < Best)
            {
                Best = Dist;
                Target = Str(Door, TEXT("id"));
                Position = DP;
            }
        }
        if (!Target.IsEmpty())
        {
            const auto P = MapOrigin + Position * TileSize;
            Activate({FSlateRect(P.X - 14, P.Y - 14, P.X + 14, P.Y + 14), TEXT("target"), Target});
        }
        return FReply::Handled();
    }
    if (K == EKeys::W || K == EKeys::A || K == EKeys::S || K == EKeys::D)
    {
        HeldKeys.Add(K);
        SendMove();
        return FReply::Handled();
    }
    if (K == EKeys::M)
    {
        bWorldMap = !bWorldMap;
        ContextTarget.Empty();
        return FReply::Handled();
    }
    if (K == EKeys::I)
    {
        Modal = Modal == TEXT("inventory") ? TEXT("") : TEXT("inventory");
        return FReply::Handled();
    }
    if (K == EKeys::C)
    {
        Modal = Modal == TEXT("character") ? TEXT("") : TEXT("character");
        return FReply::Handled();
    }
    if (K == EKeys::L)
    {
        SendAction(TEXT("listen"));
        return FReply::Handled();
    }
    return FReply::Unhandled();
}
FReply SRatwGame::OnKeyUp(const FGeometry&, const FKeyEvent& E)
{
    if (HeldKeys.Remove(E.GetKey()) > 0)
    {
        SendMove();
        return FReply::Handled();
    }
    return FReply::Unhandled();
}
void SRatwGame::OnFocusLost(const FFocusEvent& E)
{
    SCompoundWidget::OnFocusLost(E);
    HeldKeys.Empty();
    SendMove();
}
FVector2D SRatwGame::ToCanvas(const FGeometry& G, const FVector2D& Screen) const
{
    return (G.AbsoluteToLocal(Screen) - CanvasOffset) / FMath::Max(0.01, CanvasScale);
}
FReply SRatwGame::OnMouseMove(const FGeometry& G, const FPointerEvent& E)
{
    HoverPoint = ToCanvas(G, E.GetScreenSpacePosition());
    return FReply::Unhandled();
}
FReply SRatwGame::OnMouseWheel(const FGeometry& G, const FPointerEvent& E)
{
    const auto P = ToCanvas(G, E.GetScreenSpacePosition());
    if (P.X < 550 + StoryExtra)
    {
        TranscriptScroll = FMath::Max(0, TranscriptScroll + FMath::RoundToInt(E.GetWheelDelta() * 85));
        return FReply::Handled();
    }
    if (!bWorldMap)
    {
        if (E.IsShiftDown())
            MapPan.X = FMath::Clamp(MapPan.X + E.GetWheelDelta() * 60., -1000., 1000.);
        else
            MapPan.Y = FMath::Clamp(MapPan.Y + E.GetWheelDelta() * 60., -1000., 1000.);
        return FReply::Handled();
    }
    return FReply::Unhandled();
}

FReply SRatwGame::OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E)
{
    const auto P = ToCanvas(G, E.GetScreenSpacePosition());
    if (E.IsControlDown() && Modal.IsEmpty() && !bWorldMap && MapRect.ContainsPoint(P))
    {
        const FVector2D World = (P - MapOrigin) / TileSize;
        auto O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("type"), TEXT("face"));
        O->SetNumberField(TEXT("x"), World.X);
        O->SetNumberField(TEXT("y"), World.Y);
        Send(O);
        ContextTarget.Empty();
        return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
    }
    for (int32 I = Hits.Num() - 1; I >= 0; --I)
        if (Hits[I].Rect.ContainsPoint(P))
        {
            Activate(Hits[I]);
            return FReply::Handled().SetUserFocus(bChat && Modal.IsEmpty()
                                                      ? StaticCastSharedRef<SWidget>(Composer.ToSharedRef())
                                                      : StaticCastSharedRef<SWidget>(SharedThis(this)),
                                                  EFocusCause::Mouse);
        }
    if (!Modal.IsEmpty())
        return FReply::Handled();
    if (P.X > 54 && P.X < 520 + StoryExtra && P.Y > 820 && P.Y < 902)
    {
        SetChat(true);
        return FReply::Handled();
    }
    if (!bWorldMap && MapRect.ContainsPoint(P))
    {
        ContextTarget.Empty();
        if (bChat)
            SetChat(false);
        const FVector2D World = (P - MapOrigin) / TileSize;
        if (World.X < 0 || World.Y < 0 || World.X >= CellWidth || World.Y >= CellHeight)
            return FReply::Handled();
        auto O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("type"), E.IsControlDown() ? TEXT("face") : TEXT("path"));
        O->SetNumberField(TEXT("x"), World.X);
        O->SetNumberField(TEXT("y"), World.Y);
        Send(O);
        return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
    }
    ContextTarget.Empty();
    return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

void SRatwGame::Activate(const FHit& H)
{
    if (H.Action == TEXT("local"))
    {
        bWorldMap = false;
        ContextTarget.Empty();
    }
    else if (H.Action == TEXT("world"))
    {
        bWorldMap = true;
        ContextTarget.Empty();
    }
    else if (H.Action == TEXT("ic") || H.Action == TEXT("ooc"))
    {
        SetTyping(false);
        Channel = H.Action;
        TranscriptScroll = 0;
    }
    else if (H.Action == TEXT("character") || H.Action == TEXT("inventory") || H.Action == TEXT("settings"))
    {
        if (bChat)
            SetChat(false);
        HeldKeys.Empty();
        SendMove();
        Modal = H.Action;
        ContextTarget.Empty();
    }
    else if (H.Action == TEXT("close"))
        Modal.Empty();
    else if (H.Action == TEXT("volume"))
    {
        Volume = Volume == TEXT("speak") ? TEXT("whisper") : (Volume == TEXT("whisper") ? TEXT("yell") : TEXT("speak"));
    }
    else if (H.Action == TEXT("send"))
    {
        if (bChat)
            SubmitPost();
        else
            SetChat(true);
    }
    else if (H.Action == TEXT("recover"))
    {
        const FString Draft = Composer->GetText().ToString();
        Composer->SetText(FText::FromString(Draft + (Draft.IsEmpty() ? TEXT("") : TEXT("\n\n")) + FailedDraft));
        FailedDraft.Empty();
        SetChat(true);
    }
    else if (H.Action == TEXT("speed"))
    {
        RevealSpeed = RevealSpeed == 64 ? 120 : (RevealSpeed == 120 ? 0 : 64);
    }
    else if (H.Action == TEXT("motion"))
        bReducedMotion = !bReducedMotion;
    else if (H.Action == TEXT("projection"))
        bFlatWorld = !bFlatWorld;
    else if (H.Action == TEXT("split"))
    {
        StoryExtra = StoryExtra == 0 ? 150 : (StoryExtra == 150 ? 300 : (StoryExtra == 300 ? -100 : 0));
        MapPan = FVector2D::ZeroVector;
        ContextTarget.Empty();
        Invalidate(EInvalidateWidgetReason::Layout);
    }
    else if (H.Action == TEXT("color"))
    {
        SelectedColor = FCString::Atoi(*H.Target);
        auto O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("type"), TEXT("color"));
        O->SetNumberField(TEXT("index"), SelectedColor);
        Send(O);
    }
    else if (H.Action == TEXT("target"))
    {
        ContextTarget = H.Target;
        ContextPoint = FVector2D(FMath::Min(1360., H.Rect.Right + 12), FMath::Min(670., H.Rect.Top));
        ContextName = H.Target;
        ContextActions = {TEXT("inspect")};
        ContextKind = TEXT("object");
        if (const auto* E = EntityViews.Find(H.Target))
        {
            ContextName = E->Name;
            ContextKind = E->Kind;
            ContextActions = E->Actions;
        }
        for (auto& V : Arr(Snapshot, TEXT("doors")))
        {
            auto F = V->AsObject();
            if (Str(F, TEXT("id")) == H.Target)
            {
                ContextName = Str(F, TEXT("name"));
                ContextKind = TEXT("door");
                ContextActions.Empty();
                for (auto& A : Arr(F, TEXT("actions")))
                    if (A->Type == EJson::String)
                        ContextActions.Add(A->AsString());
                if (ContextActions.IsEmpty())
                    ContextActions = {TEXT("inspect"), TEXT("open"), TEXT("knock")};
                break;
            }
        }
    }
    else if (H.Action == TEXT("context"))
    {
        SendAction(H.Target, ContextTarget);
        ContextTarget.Empty();
    }
    else if (H.Action == TEXT("weather"))
    {
        auto O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("type"), TEXT("weather"));
        O->SetStringField(TEXT("value"), H.Target);
        Send(O);
    }
    else
        SendAction(H.Action, H.Target);
}

int32 SRatwGame::OnPaint(const FPaintArgs& Args, const FGeometry& Allotted, const FSlateRect& Clip,
                         FSlateWindowElementList& D, int32 L, const FWidgetStyle& Style, bool Enabled) const
{
    Hits.Empty();
    Box(Allotted, D, L, FVector2D::ZeroVector, Allotted.GetLocalSize(), Ink);
    const FGeometry G = Allotted.MakeChild(FVector2D(1600, 1000), FSlateLayoutTransform(CanvasScale, CanvasOffset));
    const auto Self = Obj(Snapshot, TEXT("self"));
    const auto Cell = Obj(Snapshot, TEXT("cell"));
    auto Button = [&](FVector2D P, FVector2D S, const FString& Label, const FString& Action, bool Active = false,
                      const FString& Target = TEXT("")) {
        const FSlateRect R(P.X, P.Y, P.X + S.X, P.Y + S.Y);
        const bool Hover = R.ContainsPoint(HoverPoint);
        if (Active || Hover)
            Box(G, D, L + 2, P, S, Active ? RGB(0x303a2e) : Raised);
        if (Active)
            Box(G, D, L + 3, P + FVector2D(0, S.Y - 2), FVector2D(S.X, 2), Amber);
        Text(G, D, L + 3, P + FVector2D(14, 10), Label, 12, Active ? Amber : Paper, false, Active);
        Hits.Add({R, Action, Target});
    };
    // The restrained frame leaves the typography and spatial glyphs in the foreground.
    Box(G, D, L + 1, FVector2D(0, 0), FVector2D(1600, 96), RGB(0x151f20));
    Box(G, D, L + 2, FVector2D(0, 95), FVector2D(1600, 1), Line);
    Lines(G, D, L + 3,
          {FVector2D(43, 62), FVector2D(51, 32), FVector2D(61, 45), FVector2D(72, 29), FVector2D(83, 62),
           FVector2D(69, 54), FVector2D(61, 69), FVector2D(53, 54), FVector2D(43, 62)},
          Amber, 1.5);
    Text(G, D, L + 3, FVector2D(101, 27), TEXT("RUNS AGAINST THE WORLD"), 21, Paper, false, true);
    Text(G, D, L + 3, FVector2D(103, 58), TEXT("A LIVING WORLD.  A STORY OF YOUR OWN."), 9, Muted, true);
    Text(G, D, L + 3, FVector2D(720, 32), TEXT("THE SHARED WORLD"), 11, Sage, true);
    FString Weather = Str(Cell, TEXT("weather"), TEXT("clear"));
    if (!Weather.IsEmpty())
        Weather[0] = FChar::ToUpper(Weather[0]);
    Text(G, D, L + 3, FVector2D(720, 54), Weather + TEXT("  /  ") + TEXT("Shared world"), 12, Muted);
    Button(FVector2D(1115, 28), FVector2D(119, 39), TEXT("CHARACTER"), TEXT("character"));
    Button(FVector2D(1242, 28), FVector2D(119, 39), TEXT("INVENTORY"), TEXT("inventory"));
    Button(FVector2D(1369, 28), FVector2D(101, 39), TEXT("SETTINGS"), TEXT("settings"));
    Box(G, D, L + 3, FVector2D(1510, 44), FVector2D(5, 5), Snapshot ? Sage : Amber);
    Text(G, D, L + 3, FVector2D(1525, 38), Snapshot ? TEXT("LIVE") : TEXT("…"), 10, Sage, true);
    Box(G, D, L + 1, FVector2D(30, 117), FVector2D(525 + StoryExtra, 809), Panel);
    Frame(G, D, L + 2, FVector2D(30, 117), FVector2D(525 + StoryExtra, 809), Line);
    Text(G, D, L + 3, FVector2D(54, 140), TEXT("THE STORY"), 11, Amber, true);
    Button(FVector2D(331 + StoryExtra, 128), FVector2D(76, 38), TEXT("IN WORLD"), TEXT("ic"), Channel == TEXT("ic"));
    Button(FVector2D(412 + StoryExtra, 128), FVector2D(117, 38), TEXT("LOCAL OOC"), TEXT("ooc"),
           Channel == TEXT("ooc"));
    Box(G, D, L + 2, FVector2D(54, 181), FVector2D(477 + StoryExtra, 1), Line);
    Text(G, D, L + 3, FVector2D(54, 200), CellName, 25, Paper, false, true);
    const float SceneH =
        Paragraph(G, D, L + 3, FVector2D(54, 241),
                  SceneDescription.IsEmpty() ? TEXT("Connecting to the persistent world…") : SceneDescription,
                  463 + StoryExtra, 14, Muted, 1.65);
    const float FeedTop = FMath::Min(388.f, 263 + SceneH);
    Box(G, D, L + 2, FVector2D(54, FeedTop), FVector2D(477 + StoryExtra, 1), Line);
    Text(G, D, L + 3, FVector2D(54, FeedTop + 15),
         Channel == TEXT("ic") ? TEXT("NEARBY VOICES & ACTIONS") : TEXT("OUT OF CHARACTER · THIS CELL"), 9, Muted,
         true);

    struct FFeedEntry
    {
        const FPost* Post;
        TArray<FString> Lines;
        float Height;
    };
    TArray<FFeedEntry> Feed;
    float Total = 0;
    int Waiting = 0;
    for (const auto& P : Posts)
    {
        if (P.Channel != Channel && !P.bSystem)
            continue;
        if (P.Revealed == 0 && !P.bSystem)
        {
            ++Waiting;
            continue;
        }
        FFeedEntry F;
        F.Post = &P;
        F.Lines = Wrap(P.Text.Left(P.Revealed), 439 + StoryExtra, 14);
        F.Height = 38 + F.Lines.Num() * 23.f;
        Feed.Add(F);
        Total += F.Height;
    }
    const float Bottom = 744, Start = FeedTop + 45, Available = Bottom - Start;
    const float Scroll = FMath::Clamp((float)TranscriptScroll, 0.f, FMath::Max(0.f, Total - Available));
    float Y = Total > Available ? Bottom - Total + Scroll : Start;
    D.PushClip(FSlateClippingZone(
        G.MakeChild(FVector2D(480 + StoryExtra, Available), FSlateLayoutTransform(FVector2D(49, Start)))
            .GetLayoutBoundingRect()));
    if (Feed.IsEmpty())
    {
        Paragraph(G, D, L + 3, FVector2D(65, Start + 29),
                  TEXT("The scene is yours to enter. Listen to the room, approach someone, or press Enter to begin a "
                       "conversation."),
                  433 + StoryExtra, 15, Muted, 1.85);
    }
    for (const auto& F : Feed)
    {
        const FPost& P = *F.Post;
        const auto Color = P.bSystem ? Muted : SpeakingColor(P.Color);
        Box(G, D, L + 3, FVector2D(54, Y + 4), FVector2D(2, F.Height - 16), Color.CopyWithNewOpacity(.50));
        Text(G, D, L + 3, FVector2D(68, Y), P.Speaker.ToUpper(), 10, P.bSystem ? Muted : Color, true);
        float TY = Y + 23;
        for (const FString& Row : F.Lines)
        {
            Text(G, D, L + 3, FVector2D(68, TY), Row, 14, P.bSystem ? Muted : Color);
            TY += 23;
        }
        Y += F.Height;
    }
    D.PopClip();
    Box(G, D, L + 2, FVector2D(54, 756), FVector2D(477 + StoryExtra, 1), Line);
    Text(G, D, L + 3, FVector2D(55, 772),
         bChat ? TEXT("WRITING  /  YOUR DRAFT IS PRIVATE") : TEXT("NAVIGATION  /  ENTER TO WRITE"), 9,
         bChat ? Sage : Muted, true);
    if (Waiting > 0 && StoryExtra >= 0)
        Text(G, D, L + 3, FVector2D(390 + StoryExtra, 772), FString::Printf(TEXT("%d QUEUED"), Waiting), 9, Amber,
             true);
    Button(FVector2D(54, 792), FVector2D(95, 28), Volume.ToUpper(), TEXT("volume"));
    if (FailedDraft.IsEmpty() || StoryExtra >= 0)
        Text(G, D, L + 3, FVector2D(160, 802),
             Channel == TEXT("ic") ? TEXT("/pose  /me  /sit  /sigh") : TEXT("Visible to this cell only"), 11, Muted);
    Frame(G, D, L + 2, FVector2D(54, 822), FVector2D(466 + StoryExtra, 79), bChat ? Sage : Line);
    Text(G, D, L + 3, FVector2D(55, 909), TEXT("SHIFT + ENTER  newline     ESC  keep draft"), 9, Muted, true);
    if (!FailedDraft.IsEmpty())
        Button(FVector2D(343 + StoryExtra, 782), FVector2D(178, 35), TEXT("RECOVER PRIOR POST"), TEXT("recover"));

    // Map workspace, physically separate from the transcript.
    Text(G, D, L + 3, FVector2D(586 + StoryExtra, 130), TEXT("YOUR SURROUNDINGS"), 10, Amber, true);
    Text(G, D, L + 3, FVector2D(586 + StoryExtra, 154), CellName, 22, Paper, false, true);
    Button(FVector2D(1270, 139), FVector2D(113, 38), TEXT("LOCAL MAP"), TEXT("local"), !bWorldMap);
    Button(FVector2D(1390, 139), FVector2D(143, 38), TEXT("WORLD MAP"), TEXT("world"), bWorldMap);
    MapRect = FSlateRect(584 + StoryExtra, 199, 1544, 816);
    Box(G, D, L + 1, FVector2D(584 + StoryExtra, 199), FVector2D(960 - StoryExtra, 617), RGB(0x0f1718));
    Frame(G, D, L + 2, FVector2D(584 + StoryExtra, 199), FVector2D(960 - StoryExtra, 617), Line);
    D.PushClip(FSlateClippingZone(
        G.MakeChild(FVector2D(958 - StoryExtra, 615), FSlateLayoutTransform(FVector2D(585 + StoryExtra, 200)))
            .GetLayoutBoundingRect()));
    if (bWorldMap)
        DrawWorld(G, D, L + 3);
    else
        DrawLocal(G, D, L + 3);
    D.PopClip();
    Text(G, D, L + 3, FVector2D(602 + StoryExtra, 830), TEXT("W"), 13, Amber, true);
    Text(G, D, L + 3, FVector2D(625 + StoryExtra, 832), TEXT("YOU"), 9, Muted, true);
    Text(G, D, L + 3, FVector2D(692 + StoryExtra, 830), TEXT("W"), 13, Blue, true);
    Text(G, D, L + 3, FVector2D(715 + StoryExtra, 832), TEXT("PLAYER"), 9, Muted, true);
    Text(G, D, L + 3, FVector2D(805 + StoryExtra, 830), TEXT("W"), 13, Sage, true);
    Text(G, D, L + 3, FVector2D(828 + StoryExtra, 832), TEXT("RESIDENT"), 9, Muted, true);
    Text(G, D, L + 3, FVector2D(StoryExtra > 150 ? 1260 : 1194, 832),
         StoryExtra > 150
             ? TEXT("PERCEPTION FILTERED")
             : (bWorldMap ? TEXT("DIRECT NEIGHBORS · YOUR MEMORY")
                          : FString::Printf(TEXT("%d × %d   /   CONTINUOUS MOVEMENT"), CellWidth, CellHeight)),
         9, Muted, true);
    Box(G, D, L + 2, FVector2D(584 + StoryExtra, 865), FVector2D(960 - StoryExtra, 1), Line);
    const double ActionLeft = 584 + StoryExtra;
    Text(G, D, L + 3, FVector2D(ActionLeft, 889), TEXT("ACTIONS"), 9, Muted, true);
    Button(FVector2D(ActionLeft + 74, 876), FVector2D(90, 39), TEXT("Listen  L"), TEXT("listen"));
    Button(FVector2D(ActionLeft + 170, 876), FVector2D(70, 39), TEXT("Look"), TEXT("look"));
    Button(FVector2D(ActionLeft + 246, 876), FVector2D(72, 39), TEXT("Smell"), TEXT("smell"));
    Button(FVector2D(ActionLeft + 324, 876), FVector2D(62, 39), TEXT("Wait"), TEXT("wait"));
    Button(FVector2D(ActionLeft + 392, 876), FVector2D(54, 39), TEXT("Sit"), TEXT("sit"));
    Button(FVector2D(ActionLeft + 452, 876), FVector2D(106, 39), TEXT("End scene"), TEXT("session_end"));
    if (StoryExtra < 150)
    {
        Text(G, D, L + 3, FVector2D(1282, 886), Str(Self, TEXT("name"), TEXT("Connecting")), 13, Paper, false, true);
        Text(G, D, L + 3, FVector2D(1282, 907),
             (Str(Self, TEXT("posture"), TEXT("standing")) + TEXT("  ·  ") + Str(Self, TEXT("state"))).Left(42), 10,
             Muted);
    }
    Box(G, D, L + 1, FVector2D(0, 951), FVector2D(1600, 49), Panel);
    Box(G, D, L + 2, FVector2D(0, 951), FVector2D(1600, 1), Line);
    Text(G, D, L + 3, FVector2D(38, 969),
         TEXT("WASD  move    CLICK  path    CTRL + CLICK  face    M  map    I  inventory    C  character"), 10, Muted,
         true);
    Text(G, D, L + 3, FVector2D(1185, 969), Str(Snapshot, TEXT("connection"), TEXT("Connecting to the world…")), 10,
         Sage);

    if (!ContextTarget.IsEmpty() && Modal.IsEmpty())
    {
        const float H = 57 + ContextActions.Num() * 36;
        Box(G, D, L + 20, ContextPoint + FVector2D(5, 6), FVector2D(194, H), FLinearColor(0, 0, 0, .5));
        Box(G, D, L + 21, ContextPoint, FVector2D(194, H), Panel);
        Frame(G, D, L + 22, ContextPoint, FVector2D(194, H), Amber.CopyWithNewOpacity(.5));
        Text(G, D, L + 23, ContextPoint + FVector2D(14, 12), ContextName, 13, Paper, false, true);
        for (int I = 0; I < ContextActions.Num(); ++I)
        {
            auto P = ContextPoint + FVector2D(7, 45 + I * 36);
            FSlateRect R(P.X, P.Y, P.X + 180, P.Y + 33);
            if (R.ContainsPoint(HoverPoint))
                Box(G, D, L + 22, P, FVector2D(180, 33), Raised);
            FString Label = ContextActions[I];
            if (!Label.IsEmpty())
                Label[0] = FChar::ToUpper(Label[0]);
            Text(G, D, L + 23, P + FVector2D(10, 6), FString::Printf(TEXT("%d  "), I + 1) + Label, 13, Sage);
            Hits.Add({R, TEXT("context"), ContextActions[I]});
        }
    }
    if (Clock < ToastUntil)
    {
        Box(G, D, L + 30, FVector2D(620 + StoryExtra, 765), FVector2D(880 - StoryExtra, 38), Panel);
        Text(G, D, L + 31, FVector2D(635 + StoryExtra, 776), Toast.Left(StoryExtra > 150 ? 65 : 105), 12, Amber);
    }
    if (!Modal.IsEmpty())
        DrawModal(G, D, L + 50);
    return SCompoundWidget::OnPaint(Args, Allotted, Clip, D, L + 70, Style, Enabled);
}

void SRatwGame::DrawLocal(const FGeometry& G, FSlateWindowElementList& D, int32 L) const
{
    TileSize = FMath::Min(
        28.0, FMath::Min((882.0 - StoryExtra) / FMath::Min(CellWidth, 32), 548.0 / FMath::Min(CellHeight, 24)));
    MapOrigin = FVector2D(1064 + StoryExtra * .5 - TileSize * FMath::Min(CellWidth, 32) * .5,
                          508 - TileSize * FMath::Min(CellHeight, 24) * .5) +
                MapPan;
    const auto Cell = Obj(Snapshot, TEXT("cell"));
    for (int Y = 0; Y < TileRows.Num(); ++Y)
        for (int X = 0; X < TileRows[Y].Len(); ++X)
        {
            const TCHAR C = TileRows[Y][X];
            const TCHAR Visibility =
                VisibilityRows.IsValidIndex(Y) && VisibilityRows[Y].IsValidIndex(X) ? VisibilityRows[Y][X] : TEXT('2');
            if (Visibility == TEXT('0') || C == TEXT(' '))
                continue;
            const FVector2D P = MapOrigin + FVector2D(X, Y) * TileSize;
            const bool Known = Visibility == TEXT('1');
            FLinearColor Color = C == TEXT('#') ? RGB(0x8c937b) : (C == TEXT('.') ? RGB(0x506258) : Sage);
            if (C == TEXT('=') || C == TEXT('T') || C == TEXT('o') || C == TEXT('O'))
                Color = RGB(0xb69462);
            if (C == TEXT('~'))
                Color = Blue.CopyWithNewOpacity(.7);
            if (C == TEXT('*') || C == TEXT('!'))
                Color = Amber;
            if (C == TEXT('+') || C == TEXT('/'))
                Color = Amber;
            if (Known)
                Color = Color.CopyWithNewOpacity(.22);
            if (!Known && C != TEXT('#'))
                Box(G, D, L, P, FVector2D(TileSize - 1, TileSize - 1), RGB(0x283126, .27));
            const FString Glyph = FString::Chr(C);
            const int FontSize = C == TEXT('.') ? 12 : 15;
            const FVector2D Extent = Measure(Glyph, FontSize, true);
            Text(G, D, L + 1, P + FVector2D((TileSize - Extent.X) * .5, (TileSize - Extent.Y) * .5), Glyph, FontSize,
                 Color, true);
        }
    // Context actions are based only on server-visible doors and entities.
    for (auto& V : Arr(Snapshot, TEXT("doors")))
    {
        auto Door = V->AsObject();
        if (!Door)
            continue;
        const FVector2D P = MapOrigin + FVector2D(Num(Door, TEXT("x")), Num(Door, TEXT("y"))) * TileSize;
        Box(G, D, L + 2, P - FVector2D(8, 10), FVector2D(17, 22), Ink);
        Text(G, D, L + 3, P - FVector2D(7, 10), Bool(Door, TEXT("open")) ? TEXT("/") : TEXT("+"), 17, Amber, true);
        Frame(G, D, L + 2, P - FVector2D(11, 13), FVector2D(23, 27), Amber.CopyWithNewOpacity(.25));
        Hits.Add({FSlateRect(P.X - 15, P.Y - 16, P.X + 15, P.Y + 16), TEXT("target"), Str(Door, TEXT("id"))});
    }
    for (const auto& Pair : EntityViews)
    {
        const auto& E = Pair.Value;
        const FVector2D P = MapOrigin + E.Position * TileSize;
        const auto Color = E.bSelf ? Amber : (E.Kind == TEXT("npc") ? Sage : Blue);
        if (E.bSelf)
        {
            Frame(G, D, L + 3, P - FVector2D(17, 17), FVector2D(34, 34), Amber.CopyWithNewOpacity(.22));
            Box(G, D, L + 2, P - FVector2D(9, 10), FVector2D(18, 21), Ink);
        }
        const int WolfFont = FMath::Clamp(FMath::RoundToInt(TileSize * .55), 10, 13);
        const FVector2D Size = Measure(TEXT("W"), WolfFont, true);
        Text(G, D, L + 4, P - Size * .5, TEXT("W"), WolfFont, Color, true);
        const FVector2D Marker =
            P + FVector2D(FMath::Cos(E.Facing), FMath::Sin(E.Facing)) * FMath::Min(13., TileSize * .55) -
            FVector2D(5, 7);
        FSlateDrawElement::MakeText(D, L + 4,
                                    G.ToPaintGeometry(FVector2D(10, 14), FSlateLayoutTransform(Marker),
                                                      FSlateRenderTransform(FQuat2D(E.Facing)), FVector2D(.5, .5)),
                                    FString(TEXT(">")), Font(10, true), ESlateDrawEffect::None, Color);
        Hits.Add({FSlateRect(P.X - 14, P.Y - 14, P.X + 14, P.Y + 14), TEXT("target"), E.Id});
        if (E.bTyping || Clock - E.SpokenAt < 4)
        {
            const double Alpha = E.bTyping ? 1 : FMath::Clamp((4 - (Clock - E.SpokenAt)) / 1.2, 0., 1.);
            const FVector2D B = P + FVector2D(-16, -39);
            Box(G, D, L + 5, B, FVector2D(32, 20), Panel.CopyWithNewOpacity(Alpha));
            Frame(G, D, L + 6, B, FVector2D(32, 20), SpeakingColor(E.Color).CopyWithNewOpacity(Alpha * .6));
            Text(G, D, L + 7, B + FVector2D(6, -1), E.bTyping ? TEXT("...") : TEXT("''"), 13,
                 SpeakingColor(E.Color).CopyWithNewOpacity(Alpha), true);
        }
        if (FVector2D::Distance(P, HoverPoint) < 20)
        {
            const FString Label = E.bSelf ? E.Name + TEXT(" · you") : E.Name;
            const FVector2D Ext = Measure(Label, 11);
            Box(G, D, L + 8, P + FVector2D(-Ext.X * .5 - 6, 21), Ext + FVector2D(12, 7), Panel);
            Text(G, D, L + 9, P + FVector2D(-Ext.X * .5, 23), Label, 11, Color);
        }
    }
    const FString Weather = Str(Cell, TEXT("weather"));
    const bool Outdoors = Bool(Cell, TEXT("outdoors"));
    if (Outdoors && (Weather == TEXT("rain") || Weather == TEXT("snow")))
    {
        for (int I = 0; I < 65; ++I)
        {
            const double Anim = bReducedMotion ? 0 : Clock;
            const double X = FMath::Fmod(I * 79.37 + Anim * 17., 948. - StoryExtra) + 590 + StoryExtra,
                         Y = FMath::Fmod(I * 137.1 + Anim * 155., 600.) + 207;
            if (Weather == TEXT("snow"))
                Text(G, D, L + 11, FVector2D(X, Y), TEXT("·"), 12, Paper.CopyWithNewOpacity(.4), true);
            else
                Lines(G, D, L + 11, {FVector2D(X, Y), FVector2D(X - 3, Y + 12)}, Blue.CopyWithNewOpacity(.18));
        }
    }
    if (Outdoors && Weather == TEXT("fog"))
        Box(G, D, L + 11, FVector2D(585 + StoryExtra, 200), FVector2D(958 - StoryExtra, 615),
            FLinearColor(.45, .53, .52, .09));
    Text(G, D, L + 12, FVector2D(606 + StoryExtra, 218), TEXT("N ^"), 10, Muted, true);
    Text(G, D, L + 12, FVector2D(1415, 782), TEXT("LOCAL  /  Z ") + FString::FromInt(Num(Cell, TEXT("z"))), 9, Muted,
         true);
}

void SRatwGame::DrawWorld(const FGeometry& G, FSlateWindowElementList& D, int32 L) const
{
    const bool Iso = Bool(Snapshot, TEXT("isometric")) && !bFlatWorld;
    Text(G, D, L + 1, FVector2D(613 + StoryExtra, 225),
         Iso ? TEXT("VISIBLE VERTICAL CONNECTION") : TEXT("NEIGHBORHOOD"), 10, Sage, true);
    Text(G, D, L + 1, FVector2D(613 + StoryExtra, 251),
         Iso ? TEXT("The visible upper cell lifts into view.") : TEXT("What you can see. What you remember."), 13,
         Muted);
    const auto Cells = Arr(Snapshot, TEXT("worldMap"));
    double CurrentX = 0, CurrentY = 0, CurrentZ = 0;
    for (const auto& V : Cells)
    {
        auto C = V->AsObject();
        if (Bool(C, TEXT("current")))
        {
            CurrentX = Num(C, TEXT("x"));
            CurrentY = Num(C, TEXT("y"));
            CurrentZ = Num(C, TEXT("z"));
            break;
        }
    }
    for (int I = 0; I < Cells.Num(); ++I)
    {
        const auto C = Cells[I]->AsObject();
        if (!C)
            continue;
        const bool Current = Bool(C, TEXT("current")), Visible = Bool(C, TEXT("visible"));
        const FString Knowledge = Str(C, TEXT("knowledge"));
        if (!Visible && !Current && Knowledge == TEXT("unknown"))
            continue;
        const double X = Num(C, TEXT("x")) - CurrentX, Y = Num(C, TEXT("y")) - CurrentY,
                     Z = Num(C, TEXT("z")) - CurrentZ;
        const double RX = FMath::Clamp(X / FMath::Max(1, CellWidth), -1., 1.),
                     RY = FMath::Clamp(Y / FMath::Max(1, CellHeight), -1., 1.), RZ = FMath::Clamp(Z, -1., 1.);
        FVector2D P(961 + StoryExtra * .5 + RX * FMath::Min(235., (960. - StoryExtra - 230.) * .5),
                    435 + RY * 157 - RZ * 120);
        if (Iso)
            P += FVector2D(-RY * 52 - RZ * 24, RX * 37);
        const FVector2D Size(206, 145);
        const auto Color = Current ? Amber : (Visible ? Sage : Muted.CopyWithNewOpacity(.5));
        Box(G, D, L + 2, P, Size, Current ? RGB(0x242d23) : Panel.CopyWithNewOpacity(Visible ? 1 : .45));
        Frame(G, D, L + 3, P, Size, Color);
        if (Iso)
        {
            Lines(G, D, L + 2, {P, P + FVector2D(32, -19), P + FVector2D(Size.X + 32, -19), P + FVector2D(Size.X, 0)},
                  Color.CopyWithNewOpacity(.5));
            Lines(G, D, L + 2,
                  {P + FVector2D(Size.X, 0), P + FVector2D(Size.X + 32, -19), P + Size + FVector2D(32, -19), P + Size},
                  Color.CopyWithNewOpacity(.5));
        }
        if (Current || Visible || Knowledge == TEXT("visited"))
        {
            const FString Glyphs = Str(C, TEXT("glyphs"));
            const int Width = FMath::Max(1, (int)Num(C, TEXT("width"))),
                      Height = FMath::Max(1, (int)Num(C, TEXT("height")));
            for (int TY = 0; TY < 5; ++TY)
                for (int TX = 0; TX < 17; ++TX)
                {
                    const int SX = FMath::RoundToInt(TX * (Width - 1) / 16.),
                              SY = FMath::RoundToInt(TY * (Height - 1) / 4.), Index = SY * Width + SX;
                    if (!Glyphs.IsValidIndex(Index))
                        continue;
                    const TCHAR Ch = Glyphs[Index];
                    if (Ch == TEXT(' ') || Ch == TEXT('\n'))
                        continue;
                    Text(G, D, L + 3, P + FVector2D(10 + TX * 11, 34 + TY * 14), FString::Chr(Ch), 9,
                         Color.CopyWithNewOpacity(Visible || Current ? .4 : .17), true);
                }
        }
        Text(G, D, L + 4, P + FVector2D(11, 11), Str(C, TEXT("name")), 12, Color, false, true);
        Text(G, D, L + 4, P + FVector2D(11, 120),
             Current
                 ? TEXT("YOU ARE HERE")
                 : (Visible ? TEXT("IN SIGHT")
                            : (Knowledge == TEXT("visited") ? TEXT("VISITED · MEMORY") : TEXT("GLIMPSED · OUTLINE"))),
             8, Color, true);
        if (Z != 0)
            Text(G, D, L + 4, P + FVector2D(157, 120), Z > 0 ? TEXT("ABOVE") : TEXT("BELOW"), 8, Color, true);
        if (Current)
            Text(G, D, L + 5, P + FVector2D(95, 64), TEXT("W>"), 15, Amber, true);
    }
    Text(G, D, L + 2, FVector2D(614 + StoryExtra, 742),
         TEXT("Memories persist. Unseen changes and residents stay hidden."), 12, Muted);
    Text(G, D, L + 2, FVector2D(614 + StoryExtra, 771),
         TEXT("BRIGHT  currently seen      DIM  remembered      ABSENT  unexplored"), 9, Muted, true);
}

void SRatwGame::DrawModal(const FGeometry& G, FSlateWindowElementList& D, int32 L) const
{
    Box(G, D, L, FVector2D(0, 97), FVector2D(1600, 854), RGB(0x080e10, .94));
    const FVector2D Origin(288, 151), Size(1024, 697);
    Box(G, D, L + 1, Origin + FVector2D(10, 13), Size, FLinearColor(0, 0, 0, .45));
    Box(G, D, L + 2, Origin, Size, Panel);
    Frame(G, D, L + 3, Origin, Size, Line);
    auto Button = [&](FVector2D P, FVector2D S, const FString& Label, const FString& Action,
                      const FString& Target = TEXT(""), bool Active = false) {
        const FSlateRect R(P.X, P.Y, P.X + S.X, P.Y + S.Y);
        Box(G, D, L + 4, P, S, Active ? RGB(0x35402d) : Raised);
        if (R.ContainsPoint(HoverPoint))
            Frame(G, D, L + 5, P, S, Sage);
        Text(G, D, L + 5, P + FVector2D(12, 11), Label, 12, Active ? Amber : Paper);
        Hits.Add({R, Action, Target});
    };
    Button(FVector2D(1240, 173), FVector2D(47, 40), TEXT("×"), TEXT("close"));
    const auto Self = Obj(Snapshot, TEXT("self"));
    if (Modal == TEXT("character"))
    {
        Text(G, D, L + 4, FVector2D(324, 185), TEXT("CHARACTER / APPEARANCE"), 10, Amber, true);
        Text(G, D, L + 4, FVector2D(324, 221), Str(Self, TEXT("name"), TEXT("Your character")), 35, Paper, false, true);
        Text(G, D, L + 4, FVector2D(325, 271), TEXT("NORMAL  ·  A STORY STILL UNFOLDING"), 10, Muted, true);
        Box(G, D, L + 3, FVector2D(324, 318), FVector2D(509, 355), Ink);
        Frame(G, D, L + 4, FVector2D(324, 318), FVector2D(509, 355), Line);
        // Static sheet art is intentionally independent from the W map token.
        const FVector2D P(357, 379);
        const auto Fur = RGB(0x78938a), LightFur = RGB(0xc2cabc);
        TArray<FVector2D> Silhouette = {
            FVector2D(105, 106), FVector2D(78, 118),  FVector2D(57, 141),  FVector2D(22, 157),  FVector2D(29, 177),
            FVector2D(68, 167),  FVector2D(105, 141), FVector2D(120, 143), FVector2D(108, 177), FVector2D(115, 211),
            FVector2D(143, 219), FVector2D(147, 213), FVector2D(132, 207), FVector2D(131, 179), FVector2D(156, 151),
            FVector2D(187, 153), FVector2D(223, 145), FVector2D(231, 184), FVector2D(233, 216), FVector2D(257, 219),
            FVector2D(265, 213), FVector2D(249, 206), FVector2D(252, 171), FVector2D(271, 135), FVector2D(285, 113),
            FVector2D(296, 102), FVector2D(333, 89),  FVector2D(354, 80),  FVector2D(368, 73),  FVector2D(360, 61),
            FVector2D(338, 53),  FVector2D(319, 30),  FVector2D(308, 3),   FVector2D(294, 26),  FVector2D(282, 13),
            FVector2D(279, 45),  FVector2D(259, 76),  FVector2D(234, 92),  FVector2D(190, 95),  FVector2D(160, 92),
            FVector2D(129, 93),  FVector2D(105, 106)};
        for (auto& Point : Silhouette)
            Point += P;
        Lines(G, D, L + 5, Silhouette, LightFur, 1.8);
        Lines(G, D, L + 5,
              {P + FVector2D(153, 145), P + FVector2D(157, 180), P + FVector2D(160, 211), P + FVector2D(178, 215),
               P + FVector2D(180, 209), P + FVector2D(171, 204), P + FVector2D(171, 158)},
              Fur, 1.4);
        Lines(G, D, L + 5,
              {P + FVector2D(258, 148), P + FVector2D(272, 205), P + FVector2D(288, 211), P + FVector2D(290, 205),
               P + FVector2D(282, 200), P + FVector2D(275, 128)},
              Fur, 1.4);
        Lines(G, D, L + 6, {P + FVector2D(305, 14), P + FVector2D(304, 36), P + FVector2D(314, 36)}, Fur, 1.4);
        Lines(G, D, L + 6,
              {P + FVector2D(288, 67), P + FVector2D(276, 80), P + FVector2D(283, 79), P + FVector2D(273, 93),
               P + FVector2D(278, 91), P + FVector2D(265, 110)},
              Fur, 1.2);
        Lines(G, D, L + 6, {P + FVector2D(294, 79), P + FVector2D(318, 78), P + FVector2D(350, 74)}, Fur, 1.2);
        for (int I = 0; I < 8; ++I)
            Lines(G, D, L + 5, {P + FVector2D(134 + I * 13, 108), P + FVector2D(126 + I * 13, 134)},
                  Fur.CopyWithNewOpacity(.24), 1);
        Lines(G, D, L + 6,
              {P + FVector2D(179, 97), P + FVector2D(174, 147), P + FVector2D(220, 144), P + FVector2D(226, 95)},
              Amber.CopyWithNewOpacity(.65), 1.5);
        Frame(G, D, L + 6, P + FVector2D(164, 111), FVector2D(46, 31), Amber.CopyWithNewOpacity(.8));
        Lines(G, D, L + 6, {P + FVector2D(165, 112), P + FVector2D(186, 124), P + FVector2D(209, 112)}, Amber, 1);
        Box(G, D, L + 7, P + FVector2D(322, 54), FVector2D(4, 3), Amber);
        Lines(G, D, L + 6, {P + FVector2D(359, 62), P + FVector2D(365, 67)}, LightFur, 3);
        Lines(G, D, L + 5, {FVector2D(361, 621), FVector2D(798, 621)}, Line);
        Text(G, D, L + 5, FVector2D(342, 643), TEXT("STATIC PROFILE · PROTOTYPE COAT & SATCHEL"), 9, Muted, true);
        Text(G, D, L + 5, FVector2D(866, 328), TEXT("PRESENT STATE"), 10, Amber, true);
        Text(G, D, L + 5, FVector2D(866, 357), Str(Self, TEXT("posture"), TEXT("standing")), 22, Paper);
        Paragraph(G, D, L + 5, FVector2D(866, 397), Str(Self, TEXT("state"), TEXT("Set your current state with /me.")),
                  365, 14, Muted);
        Text(G, D, L + 5, FVector2D(866, 478), TEXT("ROLEPLAY PROGRESSION"), 10, Amber, true);
        Text(G, D, L + 5, FVector2D(866, 508),
             FString::Printf(TEXT("Level %d"), (int)Num(Self, TEXT("socialLevel"), 1)), 24, Paper);
        Text(G, D, L + 5, FVector2D(866, 550),
             FString::Printf(TEXT("%d social experience"), (int)Num(Self, TEXT("socialXp"))), 13, Sage);
        Box(G, D, L + 4, FVector2D(866, 582), FVector2D(354, 4), Line);
        Box(G, D, L + 5, FVector2D(866, 582),
            FVector2D(FMath::Clamp(Num(Self, TEXT("socialXp")) / 100., 0., 1.) * 354, 4), Sage);
        Text(G, D, L + 5, FVector2D(326, 706), TEXT("DESCRIPTION"), 10, Amber, true);
        Paragraph(G, D, L + 5, FVector2D(326, 736),
                  Str(Self, TEXT("description"),
                      TEXT("Your appearance belongs here. Map tokens remain simple, leaving actions and expression to "
                           "the imagination.")),
                  880, 14, Paper);
    }
    else if (Modal == TEXT("inventory"))
    {
        Text(G, D, L + 4, FVector2D(324, 185), TEXT("BELONGINGS / EQUIPMENT"), 10, Amber, true);
        Text(G, D, L + 4, FVector2D(324, 221), TEXT("What you carry"), 35, Paper, false, true);
        Text(G, D, L + 4, FVector2D(325, 271), TEXT("Each object has a place in the story."), 14, Muted);
        const auto Items = Arr(Snapshot, TEXT("inventory"));
        if (Items.IsEmpty())
            Paragraph(G, D, L + 5, FVector2D(326, 337),
                      TEXT("Your pack is empty. Objects you acquire will appear here, each with its own icon and "
                           "description."),
                      700, 16, Muted);
        for (int I = 0; I < Items.Num() && I < 6; ++I)
        {
            const auto Item = Items[I]->AsObject();
            if (!Item)
                continue;
            const FVector2D P(325 + (I % 2) * 478, 327 + (I / 2) * 142);
            Box(G, D, L + 4, P, FVector2D(455, 124), Ink);
            Frame(G, D, L + 5, P, FVector2D(455, 124), Line);
            Box(G, D, L + 5, P + FVector2D(14, 16), FVector2D(81, 89), Raised);
            const FVector2D Icon = P + FVector2D(27, 28);
            const FString Kind = Str(Item, TEXT("icon"), TEXT("bag"));
            if (Kind.Contains(TEXT("bag")) || Kind.Contains(TEXT("satchel")) || Kind.Contains(TEXT("pack")))
            {
                Frame(G, D, L + 6, Icon + FVector2D(5, 18), FVector2D(45, 39), Amber);
                Lines(G, D, L + 6,
                      {Icon + FVector2D(11, 18), Icon + FVector2D(14, 6), Icon + FVector2D(43, 6),
                       Icon + FVector2D(48, 18)},
                      Amber, 2);
                Lines(G, D, L + 6, {Icon + FVector2D(6, 20), Icon + FVector2D(28, 34), Icon + FVector2D(49, 20)},
                      Amber);
            }
            else if (Kind.Contains(TEXT("bowl")) || Kind.Contains(TEXT("food")))
            {
                Lines(G, D, L + 6,
                      {Icon + FVector2D(3, 25), Icon + FVector2D(11, 51), Icon + FVector2D(46, 51),
                       Icon + FVector2D(54, 25), Icon + FVector2D(3, 25)},
                      Amber, 2);
                Lines(G, D, L + 6, {Icon + FVector2D(18, 13), Icon + FVector2D(15, 4), Icon + FVector2D(18, -1)},
                      Muted);
            }
            else if (Kind.Contains(TEXT("knife")) || Kind.Contains(TEXT("weapon")))
            {
                Lines(G, D, L + 6,
                      {Icon + FVector2D(10, 57), Icon + FVector2D(41, 8), Icon + FVector2D(48, 3),
                       Icon + FVector2D(42, 26), Icon + FVector2D(21, 47)},
                      Paper, 2);
                Lines(G, D, L + 6, {Icon + FVector2D(10, 38), Icon + FVector2D(30, 51)}, Amber, 3);
            }
            else
            {
                Lines(G, D, L + 6,
                      {Icon + FVector2D(13, 15), Icon + FVector2D(39, 15), Icon + FVector2D(46, 53),
                       Icon + FVector2D(9, 53), Icon + FVector2D(13, 15)},
                      Sage, 2);
                Frame(G, D, L + 6, Icon + FVector2D(17, 5), FVector2D(19, 10), Sage);
            }
            Text(G, D, L + 6, P + FVector2D(113, 20), Str(Item, TEXT("name")), 17, Paper, false, true);
            Text(G, D, L + 6, P + FVector2D(114, 50), Bool(Item, TEXT("equipped")) ? TEXT("EQUIPPED") : TEXT("CARRIED"),
                 9, Bool(Item, TEXT("equipped")) ? Sage : Muted, true);
            Paragraph(G, D, L + 6, P + FVector2D(114, 72), Str(Item, TEXT("description")), 316, 12, Muted, 1.4);
        }
        Text(G, D, L + 5, FVector2D(326, 797), TEXT("Equipment appears on your sheet. Your map presence remains W>."),
             12, Muted);
    }
    else if (Modal == TEXT("settings"))
    {
        Text(G, D, L + 4, FVector2D(324, 185), TEXT("PREFERENCES / READING & PRESENCE"), 10, Amber, true);
        Text(G, D, L + 4, FVector2D(324, 221), TEXT("Make yourself heard"), 35, Paper, false, true);
        Text(G, D, L + 4, FVector2D(326, 291), TEXT("YOUR SPEAKING COLOR"), 10, Amber, true);
        Paragraph(G, D, L + 4, FVector2D(326, 319),
                  TEXT("One color follows your words, typing ellipsis, and speaking marker. Your map identity keeps "
                       "its own color."),
                  504, 14, Muted);
        for (int I = 0; I < 32; ++I)
        {
            const FVector2D P(326 + (I % 8) * 60, 395 + (I / 8) * 55);
            Box(G, D, L + 5, P, FVector2D(43, 36), SpeakingColor(I));
            if (I == SelectedColor)
                Frame(G, D, L + 6, P - FVector2D(4, 4), FVector2D(51, 44), Paper);
            Hits.Add({FSlateRect(P.X, P.Y, P.X + 43, P.Y + 36), TEXT("color"), FString::FromInt(I)});
        }
        Text(G, D, L + 5, FVector2D(326, 644), TEXT("\"There is always another story beyond the door.\""), 16,
             SpeakingColor(SelectedColor));
        Text(G, D, L + 5, FVector2D(888, 291), TEXT("STORY FLOW"), 10, Amber, true);
        Button(FVector2D(886, 323), FVector2D(347, 45),
               FString(TEXT("Reveal speed: ")) +
                   (RevealSpeed == 0 ? TEXT("Instant") : FString::Printf(TEXT("%d characters / second"), RevealSpeed)),
               TEXT("speed"));
        Paragraph(G, D, L + 5, FVector2D(886, 382),
                  TEXT("One post unfolds at a time. Later voices wait their turn without changing when events happen."),
                  342, 13, Muted);
        Button(FVector2D(886, 474), FVector2D(347, 45),
               bReducedMotion ? TEXT("Reduced motion: On") : TEXT("Reduced motion: Off"), TEXT("motion"));
        Button(FVector2D(886, 535), FVector2D(347, 45),
               bFlatWorld ? TEXT("World projection: Always flat") : TEXT("World projection: Automatic"),
               TEXT("projection"));
        const FString SplitName =
            StoryExtra < 0 ? TEXT("Compact narrative")
                           : (StoryExtra == 0 ? TEXT("Balanced")
                                              : (StoryExtra == 150 ? TEXT("Wide narrative") : TEXT("Text-first")));
        Button(FVector2D(886, 596), FVector2D(347, 45), TEXT("Pane balance: ") + SplitName, TEXT("split"));
        if (Bool(Snapshot, TEXT("devTools")))
        {
            Text(G, D, L + 5, FVector2D(886, 663), TEXT("DEVELOPMENT WEATHER"), 9, Muted, true);
            const FString Weathers[] = {TEXT("clear"), TEXT("rain"), TEXT("snow"), TEXT("fog")};
            for (int I = 0; I < 4; ++I)
                Button(FVector2D(886 + I * 88, 686), FVector2D(80, 39), Weathers[I], TEXT("weather"), Weathers[I]);
        }
        Text(G, D, L + 5, FVector2D(326, 746),
             TEXT("ENTER  write / send     SHIFT + ENTER  newline     ESC  preserve draft"), 11, Muted, true);
        Text(G, D, L + 5, FVector2D(326, 791),
             TEXT("Color choices are shared. Reading preferences affect only your view."), 12, Muted);
    }
    else
    {
        Text(G, D, L + 4, FVector2D(324, 185), TEXT("A CLOSER LOOK"), 10, Amber, true);
        Paragraph(G, D, L + 5, FVector2D(325, 254), InspectedText, 876, 19, Paper, 1.7);
        Text(G, D, L + 5, FVector2D(326, 787),
             TEXT("Only information your character is allowed to perceive appears here."), 12, Muted);
    }
}
