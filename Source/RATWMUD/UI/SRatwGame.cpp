#include "UI/SRatwGame.h"
#include "UI/SRatwWolfDoll.h"
#include "Core/RatwWorld.h"
#include "Runtime/RatwJson.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"
#include "Fonts/CompositeFont.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
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
const FLinearColor Scent = RGB(0xb6a3cf);
const TCHAR* CompassName(int32 Sector)
{
    static const TCHAR* Names[] = {TEXT("E"), TEXT("SE"), TEXT("S"), TEXT("SW"),
                                   TEXT("W"), TEXT("NW"), TEXT("N"), TEXT("NE")};
    return Names[FMath::Clamp(Sector, 0, 7)];
}
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
double BoundedNum(const TSharedPtr<FJsonObject>& O, const TCHAR* K, double Low, double High, double Default = 0)
{
    const double Value = Num(O, K, Default);
    return FMath::IsFinite(Value) ? FMath::Clamp(Value, Low, High) : Default;
}
double EnvironmentNumber(const TSharedPtr<FJsonObject>& O, const TCHAR* K, double Low, double High, double Default)
{
    // JSON booleans/strings must not become weather strengths or rendering coordinates.
    const auto Value = O ? O->TryGetField(K) : nullptr;
    if (!Value || Value->Type != EJson::Number)
        return Default;
    const double Number = Value->AsNumber();
    return FMath::IsFinite(Number) ? FMath::Clamp(Number, Low, High) : Default;
}
int32 WholeCount(const TSharedPtr<FJsonObject>& O, const TCHAR* K, int32 Default = -1, int32 Maximum = 1000000000)
{
    const auto Value = O ? O->TryGetField(K) : nullptr;
    if (!Value || Value->Type != EJson::Number)
        return Default;
    const double Number = Value->AsNumber();
    return FMath::IsFinite(Number) && Number >= 0 && Number <= Maximum && FMath::FloorToDouble(Number) == Number
               ? (int32)Number
               : Default;
}
FString CountText(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
{
    const int32 Count = WholeCount(O, K);
    return Count < 0 ? TEXT("—") : FString::FromInt(Count);
}
bool ExplicitTrue(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
{
    const auto Value = O ? O->TryGetField(K) : nullptr;
    return Value && Value->Type == EJson::Boolean && Value->AsBool();
}
double WrapWeatherCoordinate(double Value, double Extent)
{
    return Extent > 0 ? FMath::Fmod(FMath::Fmod(Value, Extent) + Extent, Extent) : 0.;
}
FString PaceLabel(int32 Pace)
{
    return Pace <= 0 ? TEXT("WALK") : (Pace <= 5 ? TEXT("TROT") : (Pace <= 8 ? TEXT("RUN") : TEXT("SPRINT")));
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
FString PostureLabel(const TSharedPtr<FJsonObject>& Self)
{
    const FString Posture = Str(Self, TEXT("posture"), TEXT("standing"));
    const double Remaining = Num(Self, TEXT("postureRemaining"));
    if (Remaining > 0)
        return FString::Printf(TEXT("rising to %s · %.1fs"), *Str(Self, TEXT("postureTarget"), TEXT("standing")),
                               Remaining);
    if (Posture == TEXT("crouching"))
        return Bool(Self, TEXT("moving")) ? TEXT("crouching · sneaking") : TEXT("crouching · low profile");
    return Posture + (Bool(Self, TEXT("turning")) ? TEXT(" · turning") : TEXT(""));
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
    {
        // Map glyphs are Unicode (box drawing, shades, symbols): the bundled DejaVu Sans Mono covers them all.
        const FString Glyphs = FPaths::ProjectDir() / TEXT("Data/Fonts/DejaVuSansMono.ttf");
        const FString Path = Mono && FPaths::FileExists(Glyphs)
                                 ? Glyphs
                                 : FPaths::EngineContentDir() / TEXT("Slate/Fonts/") /
                                       (Mono ? TEXT("DroidSansMono.ttf")
                                             : (Bold ? TEXT("Roboto-Medium.ttf") : TEXT("Roboto-Regular.ttf")));
        Families.Add(FamilyKey, MakeShared<FCompositeFont>(FName(TEXT("Regular")), Path, EFontHinting::Default,
                                                           EFontLoadingPolicy::LazyLoad));
    }
    const FSlateFontInfo NewFont(Families[FamilyKey], Size, FName(TEXT("Regular")));
    Fonts.Add(Key, NewFont);
    return NewFont;
}
void Box(const FGeometry& G, FSlateWindowElementList& D, int L, FVector2D P, FVector2D S, FLinearColor C, bool Subpixel = false)
{
    FSlateDrawElement::MakeBox(D, L, G.ToPaintGeometry(S, FSlateLayoutTransform(P)),
                               FCoreStyle::Get().GetBrush("WhiteBrush"), Subpixel ? ESlateDrawEffect::NoPixelSnapping : ESlateDrawEffect::None, C);
}
void Text(const FGeometry& G, FSlateWindowElementList& D, int L, FVector2D P, const FString& T, int Size,
          FLinearColor C, bool Mono = false, bool Bold = false, bool Subpixel = false)
{
    FSlateDrawElement::MakeText(D, L, G.ToPaintGeometry(FVector2D(1600, 1000), FSlateLayoutTransform(P)), T,
                                Font(Size, Mono, Bold), Subpixel ? ESlateDrawEffect::NoPixelSnapping : ESlateDrawEffect::None, C);
}
FVector2D Measure(const FString& T, int Size, bool Mono = false)
{
    return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(T, Font(Size, Mono));
}
void Lines(const FGeometry& G, FSlateWindowElementList& D, int L, const TArray<FVector2D>& Points, FLinearColor C,
           float Width = 1, bool Subpixel = false)
{
    FSlateDrawElement::MakeLines(D, L, G.ToPaintGeometry(), Points, Subpixel ? ESlateDrawEffect::NoPixelSnapping : ESlateDrawEffect::None, C, true, Width);
}
void Frame(const FGeometry& G, FSlateWindowElementList& D, int L, FVector2D P, FVector2D S, FLinearColor C, bool Subpixel = false)
{
    Lines(G, D, L, {P, P + FVector2D(S.X, 0), P + S, P + FVector2D(0, S.Y), P}, C, 1, Subpixel);
}
EOrientation GradientStopOrientation(EOrientation Axis)
{
    // Slate names the orientation of each stop, not the direction of the color change:
    // horizontal stops produce a top-to-bottom gradient. Keep callers axis-oriented.
    return Axis == Orient_Vertical ? Orient_Horizontal : Orient_Vertical;
}
void Gradient(const FGeometry& G, FSlateWindowElementList& D, int L, FVector2D P, FVector2D S, FLinearColor Start,
              FLinearColor Middle, FLinearColor End, EOrientation Orientation)
{
    FSlateDrawElement::MakeGradient(D, L, G.ToPaintGeometry(S, FSlateLayoutTransform(P)),
                                    {FSlateGradientStop(FVector2D::ZeroVector, Start),
                                     FSlateGradientStop(S * .5, Middle), FSlateGradientStop(S, End)},
                                    GradientStopOrientation(Orientation));
}
void EdgeFade(const FGeometry& G, FSlateWindowElementList& D, int L, const FSlateRect& R, double Feather,
              FLinearColor Color, EOrientation Axis)
{
    const FVector2D Size(R.Right - R.Left, R.Bottom - R.Top);
    const double Extent = Axis == Orient_Horizontal ? Size.X : Size.Y;
    const double Width = FMath::Min(Feather, Extent * .5);
    if (Width <= 0 || Color.A <= 0)
        return;
    auto At = [&](double Distance) {
        return Axis == Orient_Horizontal ? FVector2D(Distance, 0) : FVector2D(0, Distance);
    };
    // The middle stays exactly transparent. These stops are attached to CELL edges,
    // never to the clipped viewport, so scrolling does not fabricate a nearby wall.
    FSlateDrawElement::MakeGradient(
        D, L, G.ToPaintGeometry(Size, FSlateLayoutTransform(FVector2D(R.Left, R.Top))),
        {FSlateGradientStop(At(0), Color), FSlateGradientStop(At(Width * .35), Color.CopyWithNewOpacity(Color.A * .3)),
         FSlateGradientStop(At(Width), Color.CopyWithNewOpacity(0)),
         FSlateGradientStop(At(Extent - Width), Color.CopyWithNewOpacity(0)),
         FSlateGradientStop(At(Extent - Width * .35), Color.CopyWithNewOpacity(Color.A * .3)),
         FSlateGradientStop(At(Extent), Color)},
        GradientStopOrientation(Axis));
}
void CellHalo(const FGeometry& G, FSlateWindowElementList& D, int L, const FSlateRect& R, double Radius,
              FLinearColor Color)
{
    // Shader-rounded outlines avoid open-polyline end-cap seams and jagged joins.
    // Concentric transparent outlines feather the bloom without a bitmap or flicker.
    for (double Offset = Radius; Offset > 0; Offset -= .5)
    {
        const double Alpha = Color.A * .34 * FMath::Square(1. - Offset / (Radius + .5));
        const FVector2D Origin(R.Left - Offset, R.Top - Offset);
        const FVector2D Size(R.Right - R.Left + Offset * 2., R.Bottom - R.Top + Offset * 2.);
        const FSlateRoundedBoxBrush Brush(FLinearColor::Transparent, float(Offset + 3.),
                                          Color.CopyWithNewOpacity(Alpha), 1.8f);
        FSlateDrawElement::MakeBox(D, L, G.ToPaintGeometry(Size, FSlateLayoutTransform(Origin)), &Brush,
                                   ESlateDrawEffect::None, FLinearColor::Transparent);
    }
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
                            })]]
         + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
               .Padding(TAttribute<FMargin>::CreateLambda([this]() {
                   return FMargin(CanvasOffset.X + 324 * CanvasScale,
                                  CanvasOffset.Y + (Modal == TEXT("inspect") ? 285 : 318) * CanvasScale, 0, 0);
               }))
               [SNew(SBox).WidthOverride_Lambda([this]() { return (Modal == TEXT("inspect") ? 449 : 509) * CanvasScale; })
                    .HeightOverride_Lambda([this]() { return (Modal == TEXT("inspect") ? 355 : 300) * CanvasScale; })
                    .Visibility_Lambda([this]() {
                        return (Modal == TEXT("character") || Modal == TEXT("inspect")) && PortraitAppearance()
                                   ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
                    })
                        [SNew(SRatwWolfDoll).Appearance_Lambda([this]() { return PortraitAppearance(); })
                            .Age_Lambda([this]() { return PortraitAge(); })]]
         + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
               .Padding(TAttribute<FMargin>::CreateLambda([this]() {
                   return FMargin(CanvasOffset.X + 1044 * CanvasScale, CanvasOffset.Y + 789 * CanvasScale, 0, 0);
               }))
               [SNew(SBox).WidthOverride_Lambda([this]() { return 240 * CanvasScale; })
                    .HeightOverride_Lambda([this]() { return 40 * CanvasScale; })
                    .Visibility_Lambda([this]() {
                        return Modal == TEXT("character") ? EVisibility::Visible : EVisibility::Collapsed;
                    })
                        [SNew(SButton).ButtonColorAndOpacity(Raised)
                            .OnClicked_Lambda([this]() { Modal = TEXT("leave_character"); return FReply::Handled(); })
                                [SNew(STextBlock).Text(FText::FromString(TEXT("CHARACTER SELECTION")))
                                    .Font_Lambda([this]() { return Font(FMath::Max(8, FMath::RoundToInt(11 * CanvasScale))); })
                                    .ColorAndOpacity(Paper)]]]
         + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
               .Padding(TAttribute<FMargin>::CreateLambda([this]() {
                   return FMargin(CanvasOffset.X + 326 * CanvasScale, CanvasOffset.Y + 496 * CanvasScale, 0, 0);
               }))
               [SNew(SBox).WidthOverride_Lambda([this]() { return 580 * CanvasScale; })
                    .HeightOverride_Lambda([this]() { return 48 * CanvasScale; })
                    .Visibility_Lambda([this]() {
                        return Modal == TEXT("leave_character") ? EVisibility::Visible : EVisibility::Collapsed;
                    })
                        [SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 12, 0)
                                [SNew(SButton).ButtonColorAndOpacity(Raised)
                                    .OnClicked_Lambda([this]() { LeaveCharacter(); return FReply::Handled(); })
                                        [SNew(STextBlock).Text(FText::FromString(TEXT("RETURN TO SELECTION")))
                                            .Font_Lambda([this]() { return Font(FMath::Max(8, FMath::RoundToInt(12 * CanvasScale))); })
                                            .ColorAndOpacity(Paper)]]
                            + SHorizontalBox::Slot().FillWidth(1)
                                [SNew(SButton).ButtonColorAndOpacity(Raised)
                                    .OnClicked_Lambda([this]() { Modal = TEXT("character"); return FReply::Handled(); })
                                        [SNew(STextBlock).Text(FText::FromString(TEXT("KEEP PLAYING")))
                                            .Font_Lambda([this]() { return Font(FMath::Max(8, FMath::RoundToInt(12 * CanvasScale))); })
                                            .ColorAndOpacity(Paper)]]]]];
}

TSharedPtr<FJsonObject> SRatwGame::PortraitAppearance() const
{
    return Obj(Modal == TEXT("inspect") ? InspectedCharacter : Obj(Snapshot, TEXT("self")), TEXT("appearance"));
}

double SRatwGame::PortraitAge() const
{
    if (Modal != TEXT("inspect")) return Num(Obj(Snapshot, TEXT("self")), TEXT("age"), 18);
    // Public inspection reveals an age band, never the private exact age.
    const FString Stage = Str(InspectedCharacter, TEXT("lifeStage"), TEXT("adult"));
    return Stage == TEXT("young") ? 6 : Stage == TEXT("adolescent") ? 13 : Stage == TEXT("old") ? 65 : 18;
}

void SRatwGame::LeaveCharacter()
{
    SetTyping(false);
    HeldKeys.Empty();
    SendMove();
    auto Object = MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("type"), TEXT("character_leave"));
    Send(Object);
    ShowToast(TEXT("Returning to character selection…"));
}

void SRatwGame::SetPresentationPage(const FString& Page)
{
    bFacingPreview = false;
    if (Page == TEXT("text-first"))
        StoryExtra = 300;
    if (Page == TEXT("balanced"))
        StoryExtra = 0;
    bWorldMap = Page == TEXT("world");
    bTravelAtlas = Page == TEXT("travel");
    bWorldMap |= bTravelAtlas;
    Modal =
        (Page == TEXT("character") || Page == TEXT("inventory") || Page == TEXT("settings") || Page == TEXT("trade"))
            ? Page
            : TEXT("");
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
    const int32 Generation = (int32)Num(S, TEXT("cellGeneration"), -1);
    if (NewId != CellId || Generation != CellGeneration)
    {
        HeldKeys.Empty();
        MapPan = FVector2D::ZeroVector;
        ContextTarget.Empty();
        bFacingPreview = false;
        EntityViews.Empty(); MotionVisibleIds.Empty(); LatestMotionTime = -1;
        bMotionClockReady = false;
    }
    CellGeneration = Generation;
    CellId = NewId;
    const double PoseTime = Num(S, TEXT("time"), MotionClock);
    if (PoseTime >= LatestMotionTime) ObserveMotionTime(PoseTime);
    CellName = Str(Cell, TEXT("name"), CellName);
    SceneDescription = Str(Cell, TEXT("description"), SceneDescription);
    CellWidth = FMath::Clamp((int32)Num(Cell, TEXT("width"), 32), 1, 256);
    CellHeight = FMath::Clamp((int32)Num(Cell, TEXT("height"), 24), 1, 256);
    bOutdoors = Bool(Cell, TEXT("outdoors"));
    const auto Env = Obj(Cell, TEXT("environment"));
    Environment.Weather = Str(Cell, TEXT("weather"), TEXT("clear"));
    static const TSet<FString> KnownWeather = {TEXT("overcast"), TEXT("rain"), TEXT("storm"),
                                               TEXT("fog"),      TEXT("snow"), TEXT("sandstorm")};
    if (!KnownWeather.Contains(Environment.Weather))
        Environment.Weather = TEXT("clear");
    Environment.Hour = EnvironmentNumber(Env, TEXT("hour"), 0, 24, 12);
    if (Environment.Hour >= 24)
        Environment.Hour = 0;
    Environment.Phase = Str(Env, TEXT("phase"));
    if (Environment.Phase != TEXT("day") && Environment.Phase != TEXT("dawn") && Environment.Phase != TEXT("dusk") &&
        Environment.Phase != TEXT("night"))
        Environment.Phase = Environment.Hour < 5 || Environment.Hour >= 19 ? TEXT("night")
                            : Environment.Hour < 7                         ? TEXT("dawn")
                            : Environment.Hour < 17                        ? TEXT("day")
                                                                           : TEXT("dusk");
    Environment.Daylight = EnvironmentNumber(Env, TEXT("daylight"), 0, 1, 1);
    Environment.Illumination = EnvironmentNumber(Env, TEXT("illumination"), 0, 1, 1);
    Environment.ArtificialLight = EnvironmentNumber(Env, TEXT("artificialLight"), 0, 1, 0);
    Environment.DaylightAccess = EnvironmentNumber(Env, TEXT("daylightAccess"), 0, 1, 1);
    Environment.GlowStrength = EnvironmentNumber(Env, TEXT("glowStrength"), 0, 1, 0);
    Environment.LightingTone = Str(Env, TEXT("lightingTone"), TEXT("neutral"));
    if (Environment.LightingTone != TEXT("warm") && Environment.LightingTone != TEXT("cool"))
        Environment.LightingTone = TEXT("neutral");
    Environment.LightSource = Str(Env, TEXT("lightSource"), TEXT("daylight"));
    if (Environment.LightSource != TEXT("dark") && Environment.LightSource != TEXT("artificial") &&
        Environment.LightSource != TEXT("mixed"))
        Environment.LightSource = TEXT("daylight");
    Environment.Sight = EnvironmentNumber(Env, TEXT("sight"), 0, 4, 1);
    Environment.Hearing = EnvironmentNumber(Env, TEXT("hearing"), 0, 4, 1);
    Environment.Scent = EnvironmentNumber(Env, TEXT("scent"), 0, 4, 1);
    Environment.Movement = EnvironmentNumber(Env, TEXT("movement"), 0, 4, 1);
    const auto Wind = Obj(Cell, TEXT("wind"));
    const double Direction = Num(Wind, TEXT("direction"));
    const double Strength = Num(Wind, TEXT("strength"));
    WindDirection = FMath::IsFinite(Direction) ? FMath::Atan2(FMath::Sin(Direction), FMath::Cos(Direction)) : 0.;
    WindStrength = bOutdoors && FMath::IsFinite(Strength) ? FMath::Clamp(Strength, 0., 1.) : 0.;
    bWindVariable = bOutdoors && Bool(Wind, TEXT("variable"));
    const auto Senses = Obj(S, TEXT("senses"));
    bMovementHeard = Bool(Senses, TEXT("movementHeard"));
    ScentCues.Empty();
    for (const auto& Value : Arr(Senses, TEXT("scentCues")))
    {
        if (!Value || Value->Type != EJson::Object)
            continue;
        const auto Cue = Value->AsObject();
        const double Sector = Num(Cue, TEXT("sector"), -1);
        const double Intensity = Num(Cue, TEXT("strength"), -1);
        if (!FMath::IsFinite(Sector) || Sector < 0 || Sector > 7 || FMath::FloorToDouble(Sector) != Sector ||
            !FMath::IsFinite(Intensity) || Intensity < 1 || Intensity > 3 ||
            FMath::FloorToDouble(Intensity) != Intensity)
            continue;
        FScentCue* Existing = ScentCues.FindByPredicate([&](const FScentCue& C) { return C.Sector == (int32)Sector; });
        const bool Windborne = bOutdoors && WindStrength > .01 && Bool(Cue, TEXT("windborne"));
        if (Existing)
        {
            Existing->Strength = FMath::Max(Existing->Strength, (int32)Intensity);
            Existing->bWindborne |= Windborne;
        }
        else
            ScentCues.Add({(int32)Sector, (int32)Intensity, Windborne});
    }
    ScentCues.Sort([](const FScentCue& A, const FScentCue& B) { return A.Sector < B.Sector; });
    SelfId = Str(S, TEXT("selfId"), Str(S, TEXT("playerId"), SelfId));
    auto Self = Obj(S, TEXT("self"));
    if (Self)
    {
        SelfId = Str(Self, TEXT("id"), SelfId);
        SelectedColor = (int32)Num(Self, TEXT("color"), SelectedColor);
        const int32 Pace = (int32)BoundedNum(Self, TEXT("pace"), 0, 10);
        if (Pace == RequestedPace || Clock - LastPaceRequest > 1.5)
            RequestedPace = -1;
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
    TileHeights.Init(0.f, CellWidth * CellHeight);
    int32 HeightRow = 0;
    for (auto& V : Arr(Cell, TEXT("heights")))
    {
        if (V->Type != EJson::String || HeightRow >= CellHeight)
            break;
        const FString Row = V->AsString();
        for (int32 X = 0; X < FMath::Min(Row.Len(), CellWidth); ++X)
            TileHeights[HeightRow * CellWidth + X] = float(ratwjson::HeightFromChar(Row[X]));
        ++HeightRow;
    }
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
            // Presentation only: a malformed height draws level ground, never a false cliff.
            TileHeights[Y * CellWidth + X] = float(EnvironmentNumber(T, TEXT("height"), -16, 16, 0));
            VisibilityRows[Y][X] =
                Bool(T, TEXT("visible")) ? TEXT('2') : (Bool(T, TEXT("remembered")) ? TEXT('1') : TEXT('0'));
        }
    }
    TSet<FString> Present;
    auto Entities = Arr(S, TEXT("entities"));
    // Use exactly one self pose per timestamp. The private row includes queued
    // movement intent; the sanitized public duplicate must never win the sample.
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
        if (Self && Id == SelfId && E != Self) continue;
        if (PoseTime < LatestMotionTime && !MotionVisibleIds.Contains(Id)) continue;
        Present.Add(Id);
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
        ApplyPose(View, E, PoseTime);
        View.Color = (int32)Num(E, TEXT("color"));
        View.bSelf = Id == SelfId || Bool(E, TEXT("self"));
        if (PoseTime >= LatestMotionTime)
            View.bMoving = Bool(E, TEXT("moving")) || Num(E, TEXT("postureRemaining")) > 0;
        View.bTyping = Bool(E, TEXT("typing"));
        const bool Speaking = Bool(E, TEXT("speaking"));
        if (Speaking && !View.bSpeaking)
            View.SpokenAt = Clock;
        View.bSpeaking = Speaking;
    }
    for (auto It = EntityViews.CreateIterator(); It; ++It)
        if (!Present.Contains(It.Key()) && PoseTime >= LatestMotionTime)
            It.RemoveCurrent();
    bMovementPending = false;
    if (!CanFaceAt(HoverPoint))
        bFacingPreview = false;
}

void SRatwGame::ObserveMotionTime(double ServerTime)
{
    const double Offset = MotionClock - ServerTime;
    if (bMotionClockReady && Offset - MotionOffset > .5)
    {
        // A suspended window/server may advance wall time farther than its
        // capped simulation step. Rebase instead of starving the buffer forever.
        for (auto& Entry : EntityViews) Entry.Value.Motion.Samples.Empty();
        bMotionClockReady = false;
    }
    MotionOffset = bMotionClockReady ? FMath::Min(MotionOffset, Offset) : Offset;
    bMotionClockReady = true;
}

void SRatwGame::ApplyPose(FEntityView& View, const TSharedPtr<FJsonObject>& Pose, double Time)
{
    const FVector2D Position(Num(Pose, TEXT("x")), Num(Pose, TEXT("y")));
    const double Facing = Num(Pose, TEXT("facing"));
    if (!View.Motion.Add(Time, Position, Facing)) return;
    View.Target = Position; View.TargetFacing = Facing;
    if (View.Motion.Samples.Num() == 1)
    {
        View.Position = Position; View.Facing = Facing;
    }
}

void SRatwGame::ApplyMotion(const TSharedPtr<FJsonObject>& Frame)
{
    if (!Frame || Str(Frame, TEXT("cellId")) != CellId ||
        (int32)Num(Frame, TEXT("cellGeneration"), -1) != CellGeneration) return;
    const double Time = Num(Frame, TEXT("time"), -1);
    if (Time <= LatestMotionTime) return;
    LatestMotionTime = Time;
    ObserveMotionTime(Time);
    MotionVisibleIds.Empty();
    for (const auto& Value : Arr(Frame, TEXT("entities")))
    {
        const auto Pose = Value->AsObject();
        const FString Id = Str(Pose, TEXT("id"));
        MotionVisibleIds.Add(Id);
        // Metadata is observer-filtered too; don't invent actors from poses.
        if (auto* View = EntityViews.Find(Id))
        {
            ApplyPose(*View, Pose, Time);
            View->bMoving = Bool(Pose, TEXT("moving"));
        }
    }
    for (auto It = EntityViews.CreateIterator(); It; ++It)
        if (!MotionVisibleIds.Contains(It.Key())) It.RemoveCurrent();
    bMovementPending = false;
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
        InspectedCharacter = E;
        InspectedText = Str(E, TEXT("title")) + TEXT("\n\n") + Str(E, TEXT("description"), Str(E, TEXT("text"))) +
                        TEXT("\n\n") + Str(E, TEXT("state"));
        Modal = TEXT("inspect");
        bFacingPreview = false;
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
    MotionClock += FMath::Max(0.f, Delta);
    const FVector2D Size = G.GetLocalSize();
    CanvasScale = FMath::Min(Size.X / 1600.0, Size.Y / 1000.0);
    CanvasOffset = (Size - FVector2D(1600, 1000) * CanvasScale) * 0.5;
    for (auto& E : EntityViews)
    {
        const auto Pose = E.Value.Motion.At(MotionClock - MotionOffset - .1);
        E.Value.Position = Pose.Position;
        E.Value.Facing = Pose.Facing;
    }
    if (!CanFaceAt(HoverPoint))
        bFacingPreview = false;
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
    bFacingPreview = false;
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
    const auto Travel = Obj(Snapshot, TEXT("travel"));
    // Reading/writing and focus changes release WASD, but do not reset an explicit overland route.
    if (X == 0 && Y == 0 && (Bool(Travel, TEXT("active")) || Bool(Travel, TEXT("paused"))))
        return;
    if (!bChat && (X != 0 || Y != 0))
    {
        bMovementPending = true;
        bFacingPreview = false;
        MapPan = FVector2D::ZeroVector;     // Looking around ends when the wolf moves: the map follows it again.
    }
    auto O = MakeShared<FJsonObject>();
    O->SetStringField(TEXT("type"), TEXT("move"));
    O->SetNumberField(TEXT("x"), bChat ? 0 : X);
    O->SetNumberField(TEXT("y"), bChat ? 0 : Y);
    Send(O);
    LastMove = Clock;
}
int32 SRatwGame::DisplayPace() const
{
    if (RequestedPace >= 0 && Clock - LastPaceRequest <= 1.5)
        return RequestedPace;
    return (int32)BoundedNum(Obj(Snapshot, TEXT("self")), TEXT("pace"), 0, 10);
}
void SRatwGame::RequestPace(int32 Pace)
{
    if (bChat || !Modal.IsEmpty() || !bNavigationFocus || !Obj(Snapshot, TEXT("self")))
        return;
    Pace = FMath::Clamp(Pace, 0, 10);
    if (Pace == DisplayPace())
        return;
    // Keep rapid wheel/key repeats relative to the latest request, not a lagging snapshot.
    RequestedPace = Pace;
    LastPaceRequest = Clock;
    auto O = MakeShared<FJsonObject>();
    O->SetStringField(TEXT("type"), TEXT("pace"));
    O->SetNumberField(TEXT("pace"), Pace);
    Send(O);
}
bool SRatwGame::CanTravelTo(const FString& Id) const
{
    if (Id.IsEmpty() || Id == CellId)
        return false;
    for (const auto& Value : Arr(Snapshot, TEXT("travelMap")))
    {
        if (!Value || Value->Type != EJson::Object)
            continue;
        const auto Cell = Value->AsObject();
        if (Str(Cell, TEXT("id")) == Id && Str(Cell, TEXT("knowledge")) == TEXT("visited"))
            return true;
    }
    return false;
}
void SRatwGame::CancelTravel()
{
    const auto Travel = Obj(Snapshot, TEXT("travel"));
    if (!Bool(Travel, TEXT("active")) && !Bool(Travel, TEXT("paused")))
        return;
    auto O = MakeShared<FJsonObject>();
    O->SetStringField(TEXT("type"), TEXT("cancel_travel"));
    Send(O);
}
bool SRatwGame::CanFaceAt(const FVector2D& Point) const
{
    if (bChat || bWorldMap || !Modal.IsEmpty() || !bNavigationFocus || bMovementPending || !HeldKeys.IsEmpty() ||
        !MapRect.ContainsPoint(Point) || TileSize <= 0)
        return false;
    const FVector2D World = (Point - MapOrigin) / TileSize;
    if (World.X < 0 || World.Y < 0 || World.X >= CellWidth || World.Y >= CellHeight)
        return false;
    const auto* Self = EntityViews.Find(SelfId);
    return Self && !Self->bMoving && FVector2D::DistSquared(World, Self->Position) > .0001;
}
void SRatwGame::UpdateFacingPreview(const FVector2D& Point, bool Alt)
{
    bFacingPreview = Alt && CanFaceAt(Point);
    if (bFacingPreview)
    {
        const FVector2D Direction = (Point - MapOrigin) / TileSize - EntityViews[SelfId].Position;
        PreviewFacing = FMath::Atan2(Direction.Y, Direction.X);
    }
}
void SRatwGame::SendFacing(const FVector2D& Point)
{
    const FVector2D World = (Point - MapOrigin) / TileSize;
    auto O = MakeShared<FJsonObject>();
    O->SetStringField(TEXT("type"), TEXT("face"));
    O->SetNumberField(TEXT("x"), World.X);
    O->SetNumberField(TEXT("y"), World.Y);
    Send(O);
    ContextTarget.Empty();
}
FReply SRatwGame::OnKeyDown(const FGeometry&, const FKeyEvent& E)
{
    const FKey K = E.GetKey();
    if (K == EKeys::Escape)
    {
        bFacingPreview = false;
        if (!Modal.IsEmpty())
        {
            Modal.Empty();
            return FReply::Handled();
        }
        ContextTarget.Empty();
        if (bChat)
            SetChat(false);
        else
            CancelTravel();
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
    if (K == EKeys::PageUp || K == EKeys::PageDown)
    {
        RequestPace(DisplayPace() + (K == EKeys::PageUp ? 1 : -1));
        return FReply::Handled();
    }
    if (K == EKeys::LeftAlt || K == EKeys::RightAlt)
    {
        UpdateFacingPreview(HoverPoint, true);
        return FReply::Handled();
    }
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
        bFacingPreview = false;
        bWorldMap = !bWorldMap;
        ContextTarget.Empty();
        return FReply::Handled();
    }
    if (K == EKeys::I)
    {
        bFacingPreview = false;
        Modal = Modal == TEXT("inventory") ? TEXT("") : TEXT("inventory");
        return FReply::Handled();
    }
    if (K == EKeys::C)
    {
        bFacingPreview = false;
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
    if (E.GetKey() == EKeys::LeftAlt || E.GetKey() == EKeys::RightAlt)
    {
        bFacingPreview = false;
        return FReply::Handled();
    }
    if (HeldKeys.Remove(E.GetKey()) > 0)
    {
        SendMove();
        return FReply::Handled();
    }
    return FReply::Unhandled();
}
FReply SRatwGame::OnFocusReceived(const FGeometry&, const FFocusEvent&)
{
    bNavigationFocus = true;
    bFacingPreview = false;
    return FReply::Handled();
}
void SRatwGame::OnFocusLost(const FFocusEvent& E)
{
    SCompoundWidget::OnFocusLost(E);
    HeldKeys.Empty();
    bNavigationFocus = false;
    bFacingPreview = false;
    SendMove();
}
FVector2D SRatwGame::ToCanvas(const FGeometry& G, const FVector2D& Screen) const
{
    return (G.AbsoluteToLocal(Screen) - CanvasOffset) / FMath::Max(0.01, CanvasScale);
}
FReply SRatwGame::OnMouseMove(const FGeometry& G, const FPointerEvent& E)
{
    HoverPoint = ToCanvas(G, E.GetScreenSpacePosition());
    UpdateFacingPreview(HoverPoint, E.IsAltDown());
    return FReply::Unhandled();
}
void SRatwGame::OnMouseLeave(const FPointerEvent& E)
{
    SCompoundWidget::OnMouseLeave(E);
    bFacingPreview = false;
}
FReply SRatwGame::OnMouseWheel(const FGeometry& G, const FPointerEvent& E)
{
    const auto P = ToCanvas(G, E.GetScreenSpacePosition());
    bFacingPreview = false;
    if (!Modal.IsEmpty())
        return FReply::Unhandled();
    if (P.X < 550 + StoryExtra)
    {
        TranscriptScroll = FMath::Max(0, TranscriptScroll + FMath::RoundToInt(E.GetWheelDelta() * 85));
        return FReply::Handled();
    }
    if (MapRect.ContainsPoint(P) && !bWorldMap && !bChat)
    {
        if (E.IsControlDown())
            MapPan.X = FMath::Clamp(MapPan.X + E.GetWheelDelta() * 60., -1000., 1000.);
        else if (E.IsShiftDown())
            MapPan.Y = FMath::Clamp(MapPan.Y + E.GetWheelDelta() * 60., -1000., 1000.);
        else if (!FMath::IsNearlyZero(E.GetWheelDelta()))
            RequestPace(DisplayPace() + (E.GetWheelDelta() > 0 ? 1 : -1));
        return FReply::Handled();
    }
    if (MapRect.ContainsPoint(P) && bWorldMap && bTravelAtlas && !bChat)
    {
        const int32 LastPage = FMath::Max(0, (FMath::Min(256, Arr(Snapshot, TEXT("travelMap")).Num()) - 1) / 8);
        TravelPage = FMath::Clamp(TravelPage + (E.GetWheelDelta() > 0 ? -1 : 1), 0, LastPage);
        return FReply::Handled();
    }
    return FReply::Unhandled();
}

FReply SRatwGame::OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E)
{
    const auto P = ToCanvas(G, E.GetScreenSpacePosition());
    // Modifier clicks own map input, including entities and doors. An unavailable facing action
    // must never fall through into pathing, inspection, or an action menu.
    if ((E.IsAltDown() || E.IsControlDown()) && MapRect.ContainsPoint(P))
    {
        if (E.GetEffectingButton() == EKeys::LeftMouseButton && CanFaceAt(P))
        {
            SendFacing(P);
            UpdateFacingPreview(P, E.IsAltDown());
        }
        else
            bFacingPreview = false;
        return FReply::Handled();
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
        bFacingPreview = false;
        ContextTarget.Empty();
        if (bChat)
            SetChat(false);
        const FVector2D World = (P - MapOrigin) / TileSize;
        if (World.X < 0 || World.Y < 0 || World.X >= CellWidth || World.Y >= CellHeight)
            return FReply::Handled();
        auto O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("type"), TEXT("path"));
        O->SetNumberField(TEXT("x"), World.X);
        O->SetNumberField(TEXT("y"), World.Y);
        bMovementPending = true;
        Send(O);
        return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
    }
    ContextTarget.Empty();
    return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

void SRatwGame::Activate(const FHit& H)
{
    bFacingPreview = false;
    if (H.Action == TEXT("leave_confirm"))
    {
        LeaveCharacter();
    }
    else if (H.Action == TEXT("leave_cancel"))
    {
        Modal = TEXT("character");
    }
    else if (H.Action == TEXT("local"))
    {
        bWorldMap = false;
        ContextTarget.Empty();
    }
    else if (H.Action == TEXT("world"))
    {
        bWorldMap = true;
        ContextTarget.Empty();
    }
    else if (H.Action == TEXT("nearby") || H.Action == TEXT("atlas"))
    {
        bWorldMap = true;
        bTravelAtlas = H.Action == TEXT("atlas");
        ContextTarget.Empty();
    }
    else if (H.Action == TEXT("pace"))
    {
        if (H.Target.IsNumeric())
            RequestPace(FCString::Atoi(*H.Target));
    }
    else if (H.Action == TEXT("travel_page"))
    {
        const int32 LastPage = FMath::Max(0, (FMath::Min(256, Arr(Snapshot, TEXT("travelMap")).Num()) - 1) / 8);
        TravelPage = FMath::Clamp(TravelPage + FMath::Clamp(FCString::Atoi(*H.Target), -1, 1), 0, LastPage);
    }
    else if (H.Action == TEXT("travel"))
    {
        if (bChat || !Modal.IsEmpty() || !bWorldMap || !bTravelAtlas || !CanTravelTo(H.Target))
            return;
        HeldKeys.Empty();
        auto O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("type"), TEXT("travel"));
        O->SetStringField(TEXT("target"), H.Target);
        Send(O);
        ShowToast(TEXT("Finding a route through places you have visited…"));
    }
    else if (H.Action == TEXT("cancel_travel"))
    {
        if (!bChat && Modal.IsEmpty())
            CancelTravel();
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
    else if (H.Action == TEXT("glyphs"))
        bPlainGlyphs = !bPlainGlyphs;
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
    else if (H.Action == TEXT("trade_open"))
        OpenTrade(H.Target);
    else if (H.Action == TEXT("trade_buy") || H.Action == TEXT("trade_sell"))
    {
        const bool Buy = H.Action == TEXT("trade_buy");
        if (Modal != TEXT("trade") || !CanTradeItem(H.Target, Buy))
        {
            ShowToast(TEXT("That offer is no longer available. Check the current stock and purses."));
            return;
        }
        auto O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("type"), TEXT("trade"));
        O->SetStringField(TEXT("target"), Str(Obj(Snapshot, TEXT("merchant")), TEXT("id")));
        O->SetStringField(TEXT("item"), H.Target);
        O->SetNumberField(TEXT("quantity"), 1);
        O->SetBoolField(TEXT("buy"), Buy);
        Send(O);
    }
    else if (H.Action == TEXT("eat") || H.Action == TEXT("gather"))
    {
        if ((H.Action == TEXT("eat") && InventoryQuantity(TEXT("meal")) <= 0) ||
            (H.Action == TEXT("gather") && !CanGather()))
        {
            ShowToast(H.Action == TEXT("eat") ? TEXT("You have no prepared meal to eat.")
                                              : TEXT("Approach a visible herb patch with bundles remaining."));
            return;
        }
        auto O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("type"), H.Action);
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
        if (const auto Resource = VisibleResource(); Resource && Str(Resource, TEXT("id")) == H.Target)
        {
            ContextName = TEXT("Cooking herbs");
            ContextKind = TEXT("resource");
            ContextActions = {TEXT("gather")};
        }
    }
    else if (H.Action == TEXT("context"))
    {
        if (H.Target == TEXT("trade"))
            OpenTrade(ContextTarget);
        else if (H.Target == TEXT("gather"))
            Activate({FSlateRect(), TEXT("gather"), TEXT("")});
        else
            SendAction(H.Target, ContextTarget);
        ContextTarget.Empty();
    }
    else if (H.Action == TEXT("weather") || H.Action == TEXT("wind") || H.Action == TEXT("time") ||
             H.Action == TEXT("lighting") || H.Action == TEXT("calendar"))
    {
        if (!Bool(Snapshot, TEXT("devTools")))
            return;
        if (H.Action == TEXT("calendar") && H.Target != TEXT("day") && H.Target != TEXT("year"))
            return;
        auto O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("type"), H.Action);
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
    Text(G, D, L + 3, FVector2D(720, 24), CalendarLabel(), 9, Sage, true);
    Text(G, D, L + 3, FVector2D(720, 43), EnvironmentLabel(), 11, Muted);
    Text(G, D, L + 3, FVector2D(720, 64), MoonLabel(), 9, Muted, true);
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
                  SceneDescription.IsEmpty() ? (SelfId.IsEmpty() ? TEXT("Connecting to the persistent world…")
                                                                 : TEXT("No scene description has been authored yet."))
                                             : SceneDescription,
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
             Channel == TEXT("ic") ? TEXT("/pose  /me  /sit  /lay  /stand") : TEXT("Visible to this cell only"), 11,
             Muted);
    Frame(G, D, L + 2, FVector2D(54, 822), FVector2D(466 + StoryExtra, 79), bChat ? Sage : Line);
    Text(G, D, L + 3, FVector2D(55, 909), TEXT("SHIFT + ENTER  newline     ESC  keep draft"), 9, Muted, true);
    if (!FailedDraft.IsEmpty())
        Button(FVector2D(343 + StoryExtra, 782), FVector2D(178, 35), TEXT("RECOVER PRIOR POST"), TEXT("recover"));

    // Map workspace, physically separate from the transcript.
    Text(G, D, L + 3, FVector2D(586 + StoryExtra, 130), TEXT("YOUR SURROUNDINGS"), 10, Amber, true);
    Text(G, D, L + 3, FVector2D(586 + StoryExtra, 154), CellName, 22, Paper, false, true);
    Text(G, D, L + 3, FVector2D(586 + StoryExtra, 184), EnvironmentEffectsLabel(), 8, Muted, true);
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
    DrawPace(G, D, L + 3);
    Text(G, D, L + 3, FVector2D(602 + StoryExtra, 890), TEXT("SIGHT · map"), 9, Muted);
    Text(G, D, L + 3, FVector2D(722 + StoryExtra, 890),
         bMovementHeard ? TEXT("HEARING · unseen pawsteps") : TEXT("HEARING · no unseen steps heard"), 9,
         bMovementHeard ? Blue : Muted);
    Text(G, D, L + 3, FVector2D(942 + StoryExtra, 890), ScentLabel(), 9, ScentCues.IsEmpty() ? Muted : Scent);
    Box(G, D, L + 2, FVector2D(584 + StoryExtra, 906), FVector2D(960 - StoryExtra, 1), Line);
    const double ActionLeft = 584 + StoryExtra;
    Text(G, D, L + 3, FVector2D(ActionLeft, 923), TEXT("ACTIONS"), 9, Muted, true);
    Button(FVector2D(ActionLeft + 74, 912), FVector2D(90, 33), TEXT("Listen  L"), TEXT("listen"));
    Button(FVector2D(ActionLeft + 170, 912), FVector2D(70, 33), TEXT("Look"), TEXT("look"));
    Button(FVector2D(ActionLeft + 246, 912), FVector2D(72, 33), TEXT("Smell"), TEXT("smell"));
    Button(FVector2D(ActionLeft + 324, 912), FVector2D(62, 33), TEXT("Wait"), TEXT("wait"));
    Button(FVector2D(ActionLeft + 392, 912), FVector2D(54, 33), TEXT("Sit"), TEXT("sit"));
    Button(FVector2D(ActionLeft + 452, 912), FVector2D(106, 33), TEXT("End scene"), TEXT("session_end"));
    if (StoryExtra < 150)
    {
        Text(G, D, L + 3, FVector2D(1282, 912), Str(Self, TEXT("name"), TEXT("Connecting")), 12, Paper, false, true);
        Text(G, D, L + 3, FVector2D(1282, 933),
             (PostureLabel(Self) + TEXT("  ·  ") + Str(Self, TEXT("state"))).Left(42), 10, Muted);
    }
    Box(G, D, L + 1, FVector2D(0, 951), FVector2D(1600, 49), Panel);
    Box(G, D, L + 2, FVector2D(0, 951), FVector2D(1600, 1), Line);
    Text(G, D, L + 3, FVector2D(38, 969),
         TEXT("WASD move · CLICK path · ALT+CLICK turn · WHEEL / PgUp PgDn pace · SHIFT/CTRL+WHEEL pan · M map"), 10,
         Muted, true);
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

FString SRatwGame::ScentLabel() const
{
    if (ScentCues.IsEmpty())
        return TEXT("SCENT · no unseen scent detected");
    FString Directions;
    bool Windborne = false;
    for (int32 I = 0; I < ScentCues.Num(); ++I)
    {
        if (I < 2)
            Directions += (Directions.IsEmpty() ? TEXT("") : TEXT(" / ")) + FString(CompassName(ScentCues[I].Sector));
        Windborne |= ScentCues[I].bWindborne;
    }
    if (ScentCues.Num() > 2)
        Directions += TEXT(" …");
    return TEXT("SCENT · unseen wolf roughly ") + Directions + (Windborne ? TEXT(" · upwind") : TEXT(""));
}

FString SRatwGame::WindLabel() const
{
    if (!bOutdoors)
        return TEXT("SHELTERED · still air");
    if (WindStrength <= .01)
        return TEXT("AIR FLOW · calm");
    const int32 To = (FMath::RoundToInt(WindDirection / (PI / 4.)) + 8) % 8;
    const FString Force = WindStrength < .3 ? TEXT("light") : (WindStrength < .7 ? TEXT("breeze") : TEXT("strong"));
    return FString::Printf(TEXT("AIR FLOW · %s -> %s · %s%s"), CompassName((To + 4) % 8), CompassName(To), *Force,
                           bWindVariable ? TEXT(" · shifting") : TEXT(""));
}

FString SRatwGame::EnvironmentLabel() const
{
    const int32 Minutes = FMath::FloorToInt(Environment.Hour * 60.) % (24 * 60);
    return FString::Printf(TEXT("%02d:%02d %s · %s"), Minutes / 60, Minutes % 60, *Environment.Phase.ToUpper(),
                           bOutdoors ? *Environment.Weather.ToUpper() : TEXT("SHELTERED"));
}

FString SRatwGame::CalendarLabel() const
{
    const auto Calendar = Obj(Obj(Obj(Snapshot, TEXT("cell")), TEXT("environment")), TEXT("calendar"));
    const int32 Year = WholeCount(Calendar, TEXT("year")), Day = WholeCount(Calendar, TEXT("dayOfYear"), -1, 365),
                SeasonDay = WholeCount(Calendar, TEXT("dayOfSeason"), -1, 92);
    const FString Season = Str(Calendar, TEXT("season")).ToLower();
    if (Year < 1 || Day < 1 || SeasonDay < 1 ||
        (Season != TEXT("spring") && Season != TEXT("summer") && Season != TEXT("autumn") && Season != TEXT("winter")))
        return TEXT("THE SHARED WORLD");
    return FString::Printf(TEXT("YEAR %d · %s %d · DAY %d / 365"), Year, *Season.ToUpper(), SeasonDay, Day);
}

FString SRatwGame::MoonLabel() const
{
    const auto Calendar = Obj(Obj(Obj(Snapshot, TEXT("cell")), TEXT("environment")), TEXT("calendar"));
    if (!Calendar)
        return TEXT("");
    const FString Moon = Str(Calendar, TEXT("moonName")).ToLower();
    const TArray<FString> Names = {TEXT("new moon"),       TEXT("waxing crescent"), TEXT("first quarter"),
                                   TEXT("waxing gibbous"), TEXT("full moon"),       TEXT("waning gibbous"),
                                   TEXT("last quarter"),   TEXT("waning crescent")};
    if (!Names.Contains(Moon))
        return TEXT("MOON · UNKNOWN");
    const int32 Illumination = FMath::RoundToInt(EnvironmentNumber(Calendar, TEXT("moonIllumination"), 0, 1, 0) * 100.);
    return FString::Printf(TEXT("%s · %d%% LIT"), *Moon.ToUpper(), Illumination);
}

int32 SRatwGame::InventoryQuantity(const FString& Id) const
{
    for (const auto& Value : Arr(Snapshot, TEXT("inventory")))
    {
        const auto Item = Value && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
        if (Str(Item, TEXT("id")) == Id)
            return WholeCount(Item, TEXT("quantity"), 0);
    }
    return 0;
}

TSharedPtr<FJsonObject> SRatwGame::TradeItem(const FString& Id) const
{
    if (Id != TEXT("herbs") && Id != TEXT("meal"))
        return nullptr;
    const auto Merchant = Obj(Snapshot, TEXT("merchant"));
    if (Str(Merchant, TEXT("id")).IsEmpty())
        return nullptr;
    for (const auto& Value : Arr(Merchant, TEXT("items")))
    {
        const auto Item = Value && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
        if (Str(Item, TEXT("id")) == Id)
            return Item;
    }
    return nullptr;
}

bool SRatwGame::CanTradeItem(const FString& Id, bool Buy) const
{
    const auto Item = TradeItem(Id), Merchant = Obj(Snapshot, TEXT("merchant"));
    const int32 Price = WholeCount(Item, Buy ? TEXT("buyPrice") : TEXT("sellPrice"));
    return Item && ExplicitTrue(Item, Buy ? TEXT("canBuy") : TEXT("canSell")) && Price > 0 &&
           WholeCount(Item, Buy ? TEXT("stock") : TEXT("owned")) > 0 &&
           WholeCount(Buy ? Obj(Snapshot, TEXT("self")) : Merchant, TEXT("cash")) >= Price;
}

TSharedPtr<FJsonObject> SRatwGame::VisibleResource() const
{
    const auto Resource = Obj(Snapshot, TEXT("resource"));
    if (!bOutdoors || Str(Resource, TEXT("id")) != TEXT("herb_patch") || WholeCount(Resource, TEXT("remaining")) < 0)
        return nullptr;
    for (const auto* Key : {TEXT("x"), TEXT("y")})
    {
        const auto Value = Resource->TryGetField(Key);
        const double Limit = FCString::Strcmp(Key, TEXT("x")) == 0 ? CellWidth : CellHeight;
        if (!Value || Value->Type != EJson::Number || !FMath::IsFinite(Value->AsNumber()) || Value->AsNumber() < 0 ||
            Value->AsNumber() >= Limit)
            return nullptr;
    }
    return Resource;
}

bool SRatwGame::CanGather() const
{
    const auto Resource = VisibleResource(), Self = Obj(Snapshot, TEXT("self"));
    return Resource && WholeCount(Resource, TEXT("remaining")) > 0 && Self &&
           FVector2D::Distance(FVector2D(Num(Resource, TEXT("x")), Num(Resource, TEXT("y"))),
                               FVector2D(Num(Self, TEXT("x"), -1000), Num(Self, TEXT("y"), -1000))) <= 1.7;
}

void SRatwGame::OpenTrade(const FString& Target)
{
    const auto Merchant = Obj(Snapshot, TEXT("merchant"));
    if (Target.IsEmpty() || Str(Merchant, TEXT("id")) != Target)
    {
        ShowToast(TEXT("Trading requires the trader to be awake, visible, and nearby."));
        return;
    }
    if (bChat)
        SetChat(false);
    HeldKeys.Empty();
    SendMove();
    Modal = TEXT("trade");
    ContextTarget.Empty();
}

FString SRatwGame::EnvironmentEffectsLabel() const
{
    const FString Exposure = bOutdoors                        ? TEXT("EXPOSURE")
                             : Environment.Illumination < .25 ? TEXT("UNLIT SHELTER")
                             : Environment.GlowStrength > .05 ? Environment.LightingTone.ToUpper() + TEXT(" LIGHT")
                                                              : TEXT("SHELTERED");
    return FString::Printf(TEXT("%s · SIGHT %d%%  HEARING %d%%  SCENT %d%%  FOOTING %d%%%s"), *Exposure,
                           FMath::RoundToInt(Environment.Sight * 100.), FMath::RoundToInt(Environment.Hearing * 100.),
                           FMath::RoundToInt(Environment.Scent * 100.), FMath::RoundToInt(Environment.Movement * 100.),
                           bReducedMotion ? TEXT(" · STATIC WEATHER") : TEXT(""));
}

float SRatwGame::HeightAt(int32 X, int32 Y) const
{
    // Edges repeat their neighbour, so the rim of the cell never shades as a false slope.
    X = FMath::Clamp(X, 0, CellWidth - 1);
    Y = FMath::Clamp(Y, 0, CellHeight - 1);
    const int32 Index = Y * CellWidth + X;
    return TileHeights.IsValidIndex(Index) ? TileHeights[Index] : 0.f;
}

float SRatwGame::SelfHeight() const
{
    const auto* Self = EntityViews.Find(SelfId);
    return Self ? HeightAt(FMath::FloorToInt(Self->Position.X), FMath::FloorToInt(Self->Position.Y)) : 0.f;
}

FString SRatwGame::ElevationLabel() const
{
    const float Height = SelfHeight();
    const int32 Halves = FMath::RoundToInt(FMath::Abs(Height) * 2.f);
    const FString Amount = Halves == 0       ? TEXT("0")
                           : Halves == 1     ? TEXT("½")
                           : Halves % 2 == 0 ? FString::FromInt(Halves / 2)
                                             : FString::Printf(TEXT("%d½"), Halves / 2);
    return FString::Printf(TEXT("GROUND %s%s"), Halves == 0 ? TEXT("") : (Height > 0 ? TEXT("+") : TEXT("−")), *Amount);
}

FSlateRect SRatwGame::CellBounds() const
{
    return FSlateRect(MapOrigin.X, MapOrigin.Y, MapOrigin.X + CellWidth * TileSize,
                      MapOrigin.Y + CellHeight * TileSize);
}

FSlateRect SRatwGame::VisibleCellBounds() const
{
    const auto Cell = CellBounds();
    const double Left = FMath::Max(Cell.Left, MapRect.Left + 1), Top = FMath::Max(Cell.Top, MapRect.Top + 1);
    return FSlateRect(Left, Top, FMath::Max(Left, FMath::Min(Cell.Right, MapRect.Right - 1)),
                      FMath::Max(Top, FMath::Min(Cell.Bottom, MapRect.Bottom - 1)));
}

SRatwGame::FCellAtmosphere SRatwGame::CellAtmosphere() const
{
    FCellAtmosphere Result;
    Result.Bounds = CellBounds();
    if (bWorldMap)
        return Result;
    const double ShortSide = FMath::Min(CellWidth * TileSize, CellHeight * TileSize);
    Result.Feather = FMath::Min(ShortSide * .23, TileSize * 3.6);
    Result.HaloRadius = FMath::Min(34., ShortSide * .12);
    Result.Darkness = 1. - Environment.Illumination;
    Result.GlowStrength = bOutdoors ? 0 : Environment.GlowStrength;
    Result.GlowColor = Environment.LightingTone == TEXT("warm")   ? RGB(0xeaa34f)
                       : Environment.LightingTone == TEXT("cool") ? RGB(0x87b7dd)
                                                                  : RGB(0xd8d4bf);
    if (bOutdoors)
    {
        if (Environment.Weather == TEXT("rain"))
        {
            Result.WeatherColor = RGB(0x739bb2);
            Result.WeatherStrength = .17;
        }
        else if (Environment.Weather == TEXT("snow"))
        {
            Result.WeatherColor = RGB(0xc8dce5);
            Result.WeatherStrength = .24;
        }
        else if (Environment.Weather == TEXT("fog"))
        {
            Result.WeatherColor = RGB(0xbbcfca);
            Result.WeatherStrength = .22;
        }
        else if (Environment.Weather == TEXT("overcast"))
        {
            Result.WeatherColor = RGB(0x9aa3a8);
            Result.WeatherStrength = .12;
        }
        else if (Environment.Weather == TEXT("storm"))
        {
            Result.WeatherColor = RGB(0x4e6275);
            Result.WeatherStrength = .24;
        }
        else if (Environment.Weather == TEXT("sandstorm"))
        {
            Result.WeatherColor = RGB(0xc79a5c);
            Result.WeatherStrength = .28;
        }
        else if (Environment.Phase == TEXT("dawn") || Environment.Phase == TEXT("dusk"))
        {
            Result.WeatherColor = RGB(0xd59664);
            Result.WeatherStrength = .1;
        }
    }
    return Result;
}

TArray<SRatwGame::FWeatherMark> SRatwGame::WeatherMarks() const
{
    // Falling rain and snow are texture layers (WeatherLayers); these are the ground cues under them.
    TArray<FWeatherMark> Marks;
    const bool Storm = Environment.Weather == TEXT("storm");
    if (!bOutdoors || bWorldMap || (Environment.Weather != TEXT("rain") && !Storm))
        return Marks;
    const auto Area = VisibleCellBounds();
    const double Width = Area.Right - Area.Left, Height = Area.Bottom - Area.Top;
    if (Width < 80 || Height < 80)
        return Marks;
    const double Animation = bReducedMotion ? 0 : Clock;
    const int32 Count = Storm ? 56 : 32;
    for (int32 I = 0; I < Count; ++I)
    {
        const double Life = WrapWeatherCoordinate(Animation * (Storm ? 1.1 : .7) + I * .618, 1.);
        if (Life > .62)
            continue;
        FWeatherMark Mark;
        Mark.bSplash = true;
        Mark.Position = FVector2D(Area.Left + 24. + WrapWeatherCoordinate(I * 157.3, Width - 48.),
                                  Area.Top + 44. + WrapWeatherCoordinate(I * 91.7, Height - 68.));
        Mark.End = Mark.Position;
        Mark.Size = 1. + Life * 8.;
        Mark.Alpha = (1. - Life / .62) * .32;
        Marks.Add(Mark);
    }
    return Marks;
}

TArray<SRatwGame::FWeatherLayer> SRatwGame::WeatherLayers() const
{
    using RatwWeatherArt::EArt;
    TArray<FWeatherLayer> Layers;
    if (!bOutdoors || bWorldMap)
        return Layers;
    const double T = bReducedMotion ? 0 : Clock;
    const FVector2D Wind(FMath::Cos(WindDirection) * WindStrength, FMath::Sin(WindDirection) * WindStrength);
    const FString& Weather = Environment.Weather;
    // Drift is in screen pixels per second, Offset in screen pixels; a layer scrolls by them in its own
    // texture pixels.
    auto Add = [&](EArt Art, const FVector2D& Drift, double Scale, double Angle, const FLinearColor& Tint,
                   const FVector2D& Offset = FVector2D::ZeroVector) {
        FWeatherLayer Layer;
        Layer.Art = Art;
        Layer.Scroll = (Drift * T + Offset) / Scale;
        Layer.Scale = Scale;
        Layer.Angle = Angle;
        Layer.Tint = Tint;
        Layers.Add(Layer);
    };
    // Sunlight: the shadows of passing clouds cross the ground; they vanish with the sun.
    if ((Weather == TEXT("clear") || Weather == TEXT("overcast")) && Environment.Daylight > .05)
    {
        const bool Overcast = Weather == TEXT("overcast");
        const FVector2D Drift = Wind * 26. + FVector2D(7, 3);
        Add(EArt::Cloud, Drift, 2.2, 0, RGB(0x08100c, float((Overcast ? .24 : .16) * Environment.Daylight)));
        if (Overcast)
            Add(EArt::Cloud, Drift * 1.4, 1.35, 0, RGB(0x0c1216, float(.14 * Environment.Daylight)));
    }
    if (Weather == TEXT("rain") || Weather == TEXT("storm"))
    {
        const bool Storm = Weather == TEXT("storm");
        // The sheets' streaks run along +Y; turn them to fall where the wind pushes.
        const double Slant = Wind.X * (Storm ? .9 : .5);
        const double Angle = FMath::Atan2(-Slant, 1.);
        const double Boost = Storm ? 1.2 : 1.;
        Add(EArt::Rain, FVector2D(0, 420), .9, Angle, RGB(0xa9c4d2, float(.26 * Boost)));
        Add(EArt::Rain, FVector2D(0, 700), 1.35, Angle, RGB(0xb9d0dc, float(.36 * Boost)));
        if (Storm)
            Add(EArt::Rain, FVector2D(0, 980), 1.8, Angle, RGB(0xc7d9e2, .44f));
    }
    else if (Weather == TEXT("snow"))
    {
        const double Scales[] = {.8, 1.1, 1.5}, Fall[] = {20, 32, 48};
        const float Alphas[] = {.45f, .62f, .8f};
        for (int32 I = 0; I < 3; ++I)
        {
            // Each depth sways on its own phase, so the flakes never march in step.
            Add(EArt::Snow, FVector2D(Wind.X * 60., Fall[I] + Wind.Y * 40.), Scales[I], 0, RGB(0xe6eff2, Alphas[I]),
                FVector2D(FMath::Sin(T * .5 + I * 2.1) * 12., 0));
        }
    }
    else if (Weather == TEXT("fog"))
    {
        Add(EArt::Mist, Wind * 8. + FVector2D(5, 1), 2.6, 0, RGB(0xc3d1cf, .30f));
        Add(EArt::Mist, Wind * 14. + FVector2D(-4, 2), 1.7, 0, RGB(0xc8d6d3, .22f));
    }
    else if (Weather == TEXT("sandstorm"))
    {
        // Dust streaks run along the sheet's X axis, so the sheet turns to face the wind.
        const double Speed = 60. + 220. * WindStrength;
        Add(EArt::Mist, Wind * 30. + FVector2D(12, 0), 3., 0, RGB(0xb88f58, .28f));
        Add(EArt::Dust, FVector2D(Speed, 0), 1.9, WindDirection, RGB(0xc9a26a, .42f));
        Add(EArt::Dust, FVector2D(Speed * 1.6, 0), 1.2, WindDirection, RGB(0xdcb886, .30f));
    }
    return Layers;
}

double SRatwGame::LightningFlash() const
{
    // Brief, dim and never in reduced motion: a storm cue, not a strobe.
    if (!bOutdoors || bWorldMap || bReducedMotion || Environment.Weather != TEXT("storm"))
        return 0;
    const double Period = 7.;
    const double Slot = FMath::FloorToDouble(Clock / Period);
    const double Start = FMath::Frac(FMath::Sin(Slot * 12.9898) * 43758.5453) * (Period - 1.);
    const double Since = Clock - Slot * Period - Start;
    if (Since < 0 || Since > .6)
        return 0;
    const double First = Since < .07 ? 1. : FMath::Exp(-(Since - .07) * 10.);
    const double Second = Since > .18 && Since < .24 ? .7 : 0;
    return .2 * FMath::Max(First, Second);
}

void SRatwGame::DrawWeatherLayer(const FGeometry& G, FSlateWindowElementList& D, int32 L, const FSlateRect& Area,
                                 const FWeatherLayer& Layer) const
{
    const FSlateBrush* Brush = WeatherSheets.Brush(Layer.Art);
    if (!Brush || Layer.Tint.A <= .001f || Layer.Scale <= 0)
        return;
    const double Sheet = RatwWeatherArt::Size, Tile = Sheet * Layer.Scale;
    const FVector2D AreaSize(Area.Right - Area.Left, Area.Bottom - Area.Top);
    // Large enough that the turned, shifted box still covers every corner of the area.
    const double Extent = AreaSize.Size() + Tile * 3.;
    const FVector2D Shift(WrapWeatherCoordinate(Layer.Scroll.X, Sheet) * Layer.Scale,
                          WrapWeatherCoordinate(Layer.Scroll.Y, Sheet) * Layer.Scale);
    const double C = FMath::Cos(Layer.Angle), S = FMath::Sin(Layer.Angle);
    const FVector2D Center =
        FVector2D(Area.Left, Area.Top) + AreaSize * .5 + FVector2D(C * Shift.X - S * Shift.Y, S * Shift.X + C * Shift.Y);
    const FVector2D TopLeft = Center - FVector2D(Extent, Extent) * .5;
    FSlateDrawElement::MakeBox(
        D, L,
        G.ToPaintGeometry(FVector2f(float(Extent / Layer.Scale), float(Extent / Layer.Scale)),
                          FSlateLayoutTransform(float(Layer.Scale), FVector2f(TopLeft)),
                          FSlateRenderTransform(FQuat2f(float(Layer.Angle))), FVector2f(.5f, .5f)),
        Brush, ESlateDrawEffect::None, Layer.Tint);
}

void SRatwGame::DrawEnvironment(const FGeometry& G, FSlateWindowElementList& D, int32 L, bool Foreground) const
{
    if (bWorldMap)
        return;
    const FVector2D PanelOrigin(MapRect.Left + 1, MapRect.Top + 1);
    const FVector2D PanelSize(MapRect.Right - MapRect.Left - 2, MapRect.Bottom - MapRect.Top - 2);
    if (PanelSize.X <= 0 || PanelSize.Y <= 0)
        return;
    // This clip is deliberately local even when the renderer is called independently.
    // Exterior halos may enter empty map canvas, never roleplay text or controls.
    D.PushClip(FSlateClippingZone(G.MakeChild(PanelSize, FSlateLayoutTransform(PanelOrigin)).GetLayoutBoundingRect()));
    const auto Atmosphere = CellAtmosphere();
    const auto Bounds = Atmosphere.Bounds;
    const FVector2D Origin(Bounds.Left, Bounds.Top), Size(Bounds.Right - Bounds.Left, Bounds.Bottom - Bounds.Top);
    const float Darkness = Atmosphere.Darkness;
    if (!Foreground)
    {
        if (Atmosphere.GlowStrength > .001)
            CellHalo(G, D, L, Bounds, Atmosphere.HaloRadius,
                     Atmosphere.GlowColor.CopyWithNewOpacity(Atmosphere.GlowStrength * .42));
        if (Atmosphere.WeatherStrength > .001)
            CellHalo(G, D, L, Bounds, Atmosphere.HaloRadius * .7,
                     Atmosphere.WeatherColor.CopyWithNewOpacity(Atmosphere.WeatherStrength));
        if (bOutdoors)
        {
            const FLinearColor Ground = FMath::Lerp(RGB(0x1e2c22), RGB(0x0a1225), Darkness);
            const FLinearColor Horizon = FMath::Lerp(RGB(0x343629), RGB(0x152339), Darkness);
            Gradient(G, D, L, Origin, Size, Horizon, Ground, (Ground * .78f).CopyWithNewOpacity(1), Orient_Vertical);
            if (Environment.Phase == TEXT("dawn") || Environment.Phase == TEXT("dusk"))
                Gradient(G, D, L + 1, Origin, Size, RGB(0xd69864, .12), RGB(0xc0774b, .04), RGB(0x786992, .02),
                         Environment.Phase == TEXT("dawn") ? Orient_Horizontal : Orient_Vertical);
            if (Environment.Weather == TEXT("rain"))
                Gradient(G, D, L + 1, Origin, Size, RGB(0x617a92, .08), RGB(0x34495c, .07), RGB(0x294859, .11),
                         Orient_Vertical);
            else if (Environment.Weather == TEXT("snow"))
                Gradient(G, D, L + 1, Origin, Size, RGB(0xb4c3ce, .09), RGB(0x87a0b8, .07), RGB(0xb7c9ce, .12),
                         Orient_Vertical);
            else if (Environment.Weather == TEXT("fog"))
                Box(G, D, L + 1, Origin, Size, RGB(0x9fafac, .11));
            else if (Environment.Weather == TEXT("overcast"))
                Box(G, D, L + 1, Origin, Size, RGB(0x7d868c, .10));
            else if (Environment.Weather == TEXT("storm"))
                Gradient(G, D, L + 1, Origin, Size, RGB(0x3c4b5c, .18), RGB(0x223040, .14), RGB(0x1b2836, .2),
                         Orient_Vertical);
            else if (Environment.Weather == TEXT("sandstorm"))
                Gradient(G, D, L + 1, Origin, Size, RGB(0xc09a62, .20), RGB(0xa57b45, .16), RGB(0x8c6a3e, .22),
                         Orient_Horizontal);
            else if (Environment.Phase == TEXT("day"))
                Box(G, D, L + 1, Origin, Size, RGB(0xffd89a, float(.05 * Environment.Daylight)));
        }
        else if (Darkness > .01)
            Box(G, D, L, Origin, Size, RGB(0x020610, Darkness * .58));
    }
    else
    {
        // All fades follow the true room bounds, including bounds outside this viewport.
        // They are atmosphere only: no additional terrain, identities, or click targets.
        if (Darkness > .01)
        {
            const auto Edge = RGB(0x02050c, Darkness * .72f);
            EdgeFade(G, D, L, Bounds, Atmosphere.Feather, Edge, Orient_Horizontal);
            EdgeFade(G, D, L, Bounds, Atmosphere.Feather, Edge, Orient_Vertical);
        }
        const auto Glow = Atmosphere.GlowColor.CopyWithNewOpacity(Atmosphere.GlowStrength * .2);
        const auto WeatherEdge = Atmosphere.WeatherColor.CopyWithNewOpacity(Atmosphere.WeatherStrength);
        for (const auto Axis : {Orient_Horizontal, Orient_Vertical})
        {
            EdgeFade(G, D, L + 1, Bounds, Atmosphere.Feather * .75, Glow, Axis);
            EdgeFade(G, D, L + 1, Bounds, Atmosphere.Feather, WeatherEdge, Axis);
        }
        D.PushClip(FSlateClippingZone(G.MakeChild(Size, FSlateLayoutTransform(Origin)).GetLayoutBoundingRect()));
        for (const auto& Layer : WeatherLayers())
            DrawWeatherLayer(G, D, L + 1, Bounds, Layer);
        for (const auto& Mark : WeatherMarks())
            Lines(G, D, L + 1,
                  {Mark.Position + FVector2D(-Mark.Size, -1), Mark.Position + FVector2D(0, Mark.Size * .35),
                   Mark.Position + FVector2D(Mark.Size, -1)},
                  RGB(0x9dbaca, Mark.Alpha), .8f);
        // In the dark, the ground beyond a wolf's own sight sinks into night around it.
        const auto* SelfView = EntityViews.Find(SelfId);
        if (Darkness > .3f && SelfView)
        {
            const FLinearColor Night = RGB(0x02050c, (Darkness - .3f) / .7f * .6f);
            const double Radius = FMath::Clamp(27. * Environment.Sight, 4., 30.) * TileSize;
            const FVector2D Center = MapOrigin + SelfView->Position * TileSize;
            const FVector2D Low = Center - FVector2D(Radius, Radius), High = Center + FVector2D(Radius, Radius);
            if (const FSlateBrush* Pool = WeatherSheets.Brush(RatwWeatherArt::EArt::Pool))
                FSlateDrawElement::MakeBox(D, L + 2, G.ToPaintGeometry(High - Low, FSlateLayoutTransform(Low)), Pool,
                                           ESlateDrawEffect::None, Night);
            Box(G, D, L + 2, Origin, FVector2D(Size.X, FMath::Max(0., Low.Y - Origin.Y)), Night);
            Box(G, D, L + 2, FVector2D(Origin.X, High.Y), FVector2D(Size.X, FMath::Max(0., Bounds.Bottom - High.Y)),
                Night);
            Box(G, D, L + 2, FVector2D(Origin.X, Low.Y), FVector2D(FMath::Max(0., Low.X - Origin.X), High.Y - Low.Y),
                Night);
            Box(G, D, L + 2, FVector2D(High.X, Low.Y), FVector2D(FMath::Max(0., Bounds.Right - High.X), High.Y - Low.Y),
                Night);
        }
        if (const double Flash = LightningFlash(); Flash > 0)
            Box(G, D, L + 3, Origin, Size, RGB(0xdfe8f5, float(Flash)));
        D.PopClip();
    }
    D.PopClip();
}

void SRatwGame::DrawScent(const FGeometry& G, FSlateWindowElementList& D, int32 L, const FVector2D& SelfPoint) const
{
    // Fixed-radius compass hints, not source locations. Parent map clipping trims edge arcs;
    // the directional text remains outside the map even when the wolf is off-screen.
    if (bWorldMap || !MapRect.ContainsPoint(SelfPoint))
        return;
    for (const FScentCue& Cue : ScentCues)
    {
        const double Angle = Cue.Sector * PI / 4.;
        const float Alpha = (.26f + Cue.Strength * .13f) * 1.2f;
        for (int Ring = 0; Ring < 2; ++Ring)
        {
            TArray<FVector2D> Arc;
            for (int Step = 0; Step <= 12; ++Step)
            {
                const double Theta = Angle - PI / 8. + Step * (PI / 4.) / 12.;
                Arc.Add(SelfPoint + FVector2D(FMath::Cos(Theta), FMath::Sin(Theta)) * (38. + Ring * 5.));
            }
            Lines(G, D, L, Arc, Scent.CopyWithNewOpacity(Alpha * (Ring ? .45f : 1.f)), 1.2f);
        }
        const FVector2D P = SelfPoint + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * 40.;
        Box(G, D, L + 1, P - FVector2D(9, 6), FVector2D(18, 12), Ink.CopyWithNewOpacity(.8));
        Text(G, D, L + 2, P - FVector2D(8, 8), TEXT("~~"), 11, Scent.CopyWithNewOpacity(Alpha), true);
    }
}

void SRatwGame::DrawLocal(const FGeometry& G, FSlateWindowElementList& D, int32 L) const
{
    TileSize = FMath::Min(
        28.0, FMath::Min((882.0 - StoryExtra) / FMath::Min(CellWidth, 32), 548.0 / FMath::Min(CellHeight, 24)));
    // A cell that fits is centred; a larger one follows the wolf, stopping at the cell's edges so no empty canvas
    // shows. Shift/Ctrl + wheel still look around (MapPan) until the wolf next moves.
    const FVector2D ViewCenter(1064 + StoryExtra * .5, 508);
    const auto* Me = EntityViews.Find(SelfId);
    const auto Axis = [&](double Center, double Low, double High, int32 Tiles, double Self) {
        const double Span = Tiles * TileSize;
        if (Span <= High - Low || !Me)
            return Center - Span * .5;
        return FMath::Clamp(Center - Self * TileSize, High - Span, Low);
    };
    MapOrigin = FVector2D(Axis(ViewCenter.X, MapRect.Left, MapRect.Right, CellWidth, Me ? Me->Position.X : 0),
                          Axis(ViewCenter.Y, MapRect.Top, MapRect.Bottom, CellHeight, Me ? Me->Position.Y : 0)) +
                MapPan;
    const auto Cell = Obj(Snapshot, TEXT("cell"));
    DrawEnvironment(G, D, L, false);
    const float Ground = SelfHeight();
    // Only the tiles on screen are drawn: a large cell has tens of thousands more.
    const int32 FirstX = FMath::Max(0, FMath::FloorToInt((MapRect.Left - MapOrigin.X) / TileSize) - 1);
    const int32 FirstY = FMath::Max(0, FMath::FloorToInt((MapRect.Top - MapOrigin.Y) / TileSize) - 1);
    const int32 LastX = FMath::CeilToInt((MapRect.Right - MapOrigin.X) / TileSize) + 1;
    const int32 LastY = FMath::Min(TileRows.Num() - 1, FMath::CeilToInt((MapRect.Bottom - MapOrigin.Y) / TileSize) + 1);
    for (int Y = FirstY; Y <= LastY; ++Y)
        for (int X = FirstX; X <= FMath::Min(LastX, TileRows[Y].Len() - 1); ++X)
        {
            const TCHAR C = TileRows[Y][X];
            const TCHAR Visibility =
                VisibilityRows.IsValidIndex(Y) && VisibilityRows[Y].IsValidIndex(X) ? VisibilityRows[Y][X] : TEXT('2');
            if (Visibility == TEXT('0') || C == TEXT(' '))
                continue;
            const FVector2D P = MapOrigin + FVector2D(X, Y) * TileSize;
            const bool Known = Visibility == TEXT('1');
            const auto* Info = ratw::terrainInfo(char(C < 128 ? C : 0));
            FLinearColor Color = Info ? RGB(Info->fg) : Sage;
            if (Environment.Illumination < 1)
            {
                const float Darkness = 1.f - Environment.Illumination;
                Color = FMath::Lerp(Color, RGB(0x8195ad).CopyWithNewOpacity(Color.A), Darkness * .28f);
                Color = (Color * (1.f - Darkness * .27f)).CopyWithNewOpacity(Color.A);
            }
            if (Known)
                Color = Color.CopyWithNewOpacity(.22);
            // Height reads relative to the wolf: ground above it is lit and warm, ground below sinks into
            // shade, and slopes facing the north-west light are brighter than those turned away from it.
            const float Rise = HeightAt(X, Y) - Ground;
            if (!Known)
            {
                const float Facing = FMath::Clamp(
                    (HeightAt(X + 1, Y) + HeightAt(X, Y + 1) - HeightAt(X - 1, Y) - HeightAt(X, Y - 1)) * .5f, -2.f, 2.f);
                const FLinearColor Base = Info ? RGB(Info->bg) : RGB(0x283126);
                FLinearColor Floor = Rise >= 0 ? FMath::Lerp(Base, RGB(0x6b6a4a), FMath::Min(Rise * .12f, .4f))
                                               : FMath::Lerp(Base, RGB(0x0b1216), FMath::Min(-Rise * .14f, .5f));
                Floor = (Floor * (1.f + Facing * .2f)).CopyWithNewOpacity(.27f + FMath::Min(FMath::Abs(Rise) * .03f, .12f));
                Box(G, D, L, P, FVector2D(TileSize - 1, TileSize - 1), Floor);
            }
            const TCHAR Shape = !Info ? C : (bPlainGlyphs ? TCHAR(Info->ascii) : TCHAR(Info->glyph));
            // Block and shade characters fill the whole tile, so stone walls and cliffs read as one mass
            // instead of a row of narrow bars.
            const float Fill = bPlainGlyphs ? 0.f
                               : Shape == 0x2588 ? 1.f : Shape == 0x2593 ? .75f : Shape == 0x2592 ? .5f
                               : Shape == 0x2591 ? .28f : 0.f;
            const double Lift = FMath::Clamp(double(Rise), -2., 2.);
            if (Fill > 0)
                Box(G, D, L + 1, P - FVector2D(0, Lift), FVector2D(TileSize, TileSize),
                    Color.CopyWithNewOpacity(Color.A * Fill));
            else
            {
                const FString Glyph = FString::Chr(Shape);
                const bool Small = Shape == TEXT('.') || Shape == 0x00b7 || Shape == 0x2219 || Shape == TEXT(',');
                const int FontSize = Small ? 12 : 15;
                const FVector2D Extent = Measure(Glyph, FontSize, true);
                Text(G, D, L + 1, P + FVector2D((TileSize - Extent.X) * .5, (TileSize - Extent.Y) * .5 - Lift), Glyph,
                     FontSize, Color, true);
            }
        }
    // Where the ground changes height, the edge is drawn by how it can be crossed: a faint contour for a
    // half step, a warm line where a slope or stairs make a full step walkable, and a heavy rim with a
    // cast shadow for a ledge or cliff that cannot be walked.
    auto Seen = [&](int32 X, int32 Y) {
        const auto* Info = TileRows.IsValidIndex(Y) && TileRows[Y].IsValidIndex(X) && TileRows[Y][X] < 128
                               ? ratw::terrainInfo(char(TileRows[Y][X]))
                               : nullptr;
        return Info && Info->kind != ratw::Terrain::Wall && VisibilityRows.IsValidIndex(Y) && VisibilityRows[Y].IsValidIndex(X) &&
               VisibilityRows[Y][X] != TEXT('0');
    };
    for (int32 Y = FirstY; Y <= LastY; ++Y)
        for (int32 X = FirstX; X <= FMath::Min(LastX, TileRows[Y].Len() - 1); ++X)
            for (const FIntPoint Step : {FIntPoint(1, 0), FIntPoint(0, 1)})
            {
                const int32 Nx = X + Step.X, Ny = Y + Step.Y;
                if (!Seen(X, Y) || !Seen(Nx, Ny))
                    continue;
                const TCHAR A = TileRows[Y][X], B = TileRows[Ny][Nx];
                const float Ha = HeightAt(X, Y), Hb = HeightAt(Nx, Ny), Drop = FMath::Abs(Ha - Hb);
                const auto* InfoA = ratw::terrainInfo(char(A));
                const auto* InfoB = ratw::terrainInfo(char(B));
                const bool Cliff = InfoA->kind == ratw::Terrain::Cliff || InfoB->kind == ratw::Terrain::Cliff;
                if (Drop < .01f && !Cliff)
                    continue;
                const bool Remembered = VisibilityRows[Y][X] == TEXT('1') || VisibilityRows[Ny][Nx] == TEXT('1');
                const float Fade = Remembered ? .45f : 1.f;
                const bool Ramp = InfoA->ramp || InfoB->ramp;
                const FVector2D Corner = MapOrigin + FVector2D(Nx, Ny) * TileSize;
                const FVector2D End = Corner + (Step.X ? FVector2D(0, TileSize) : FVector2D(TileSize, 0));
                if (Drop <= .5f && !Cliff)
                    Lines(G, D, L + 1, {Corner, End}, RGB(0xc9bf9a, .16f * Fade), 1.f);
                else if (Drop <= 1.01f && Ramp && !Cliff)
                    Lines(G, D, L + 1, {Corner, End}, RGB(0xd8b877, .34f * Fade), 1.3f);
                else
                {
                    // The shadow falls onto the lower side; a level cliff edge shades its open side.
                    const bool LowAfter = Hb < Ha || (Drop < .01f && InfoA->kind == ratw::Terrain::Cliff);
                    const double Band = FMath::Min(6., TileSize * .28);
                    const FVector2D Shadow = LowAfter ? Corner : Corner - FVector2D(Step) * Band;
                    Box(G, D, L, Shadow, Step.X ? FVector2D(Band, TileSize) : FVector2D(TileSize, Band),
                        RGB(0x04070a, .4f * Fade));
                    Lines(G, D, L + 1, {Corner, End}, RGB(0xe9d2a0, .55f * Fade), 2.2f);
                }
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
    if (const auto Resource = VisibleResource())
    {
        const FVector2D P = MapOrigin + FVector2D(Num(Resource, TEXT("x")), Num(Resource, TEXT("y"))) * TileSize;
        Text(G, D, L + 3, P - FVector2D(8, 12), TEXT("\""), 19,
             WholeCount(Resource, TEXT("remaining")) > 0 ? Sage : Muted.CopyWithNewOpacity(.5), true);
        Hits.Add({FSlateRect(P.X - 14, P.Y - 14, P.X + 14, P.Y + 14), TEXT("target"), TEXT("herb_patch")});
    }
    if (const auto* SelfView = EntityViews.Find(SelfId))
        DrawScent(G, D, L + 2, MapOrigin + SelfView->Position * TileSize);
    for (const auto& Pair : EntityViews)
    {
        const auto& E = Pair.Value;
        const FVector2D P = MapOrigin + E.Position * TileSize;
        const auto Color = E.bSelf ? Amber : (E.Kind == TEXT("npc") ? Sage : Blue);
        if (E.bSelf)
        {
            Frame(G, D, L + 3, P - FVector2D(17, 17), FVector2D(34, 34), Amber.CopyWithNewOpacity(.22), true);
            Box(G, D, L + 2, P - FVector2D(9, 10), FVector2D(18, 21), Ink, true);
        }
        const int WolfFont = FMath::Clamp(FMath::RoundToInt(TileSize * .55), 10, 13);
        const FVector2D Size = Measure(TEXT("W"), WolfFont, true);
        Text(G, D, L + 4, P - Size * .5, TEXT("W"), WolfFont, Color, true, false, true);
        const FVector2D Marker =
            P + FVector2D(FMath::Cos(E.Facing), FMath::Sin(E.Facing)) * FMath::Min(13., TileSize * .55) -
            FVector2D(5, 7);
        FSlateDrawElement::MakeText(D, L + 4,
                                    G.ToPaintGeometry(FVector2D(10, 14), FSlateLayoutTransform(Marker),
                                                      FSlateRenderTransform(FQuat2D(E.Facing)), FVector2D(.5, .5)),
                                    FString(TEXT(">")), Font(10, true), ESlateDrawEffect::NoPixelSnapping, Color);
        if (E.bSelf && bFacingPreview && CanFaceAt(HoverPoint))
        {
            const FVector2D PreviewMarker =
                P + FVector2D(FMath::Cos(PreviewFacing), FMath::Sin(PreviewFacing)) * FMath::Min(13., TileSize * .55) -
                FVector2D(5, 7);
            FSlateDrawElement::MakeText(
                D, L + 5,
                G.ToPaintGeometry(FVector2D(10, 14), FSlateLayoutTransform(PreviewMarker),
                                  FSlateRenderTransform(FQuat2D(PreviewFacing)), FVector2D(.5, .5)),
                FString(TEXT(">")), Font(10, true), ESlateDrawEffect::NoPixelSnapping, Color.CopyWithNewOpacity(.32));
        }
        Hits.Add({FSlateRect(P.X - 14, P.Y - 14, P.X + 14, P.Y + 14), TEXT("target"), E.Id});
        if (E.bTyping || Clock - E.SpokenAt < 4)
        {
            const double Alpha = E.bTyping ? 1 : FMath::Clamp((4 - (Clock - E.SpokenAt)) / 1.2, 0., 1.);
            const FVector2D B = P + FVector2D(-16, -39);
            Box(G, D, L + 5, B, FVector2D(32, 20), Panel.CopyWithNewOpacity(Alpha), true);
            Frame(G, D, L + 6, B, FVector2D(32, 20), SpeakingColor(E.Color).CopyWithNewOpacity(Alpha * .6), true);
            Text(G, D, L + 7, B + FVector2D(6, -1), E.bTyping ? TEXT("...") : TEXT("''"), 13,
                 SpeakingColor(E.Color).CopyWithNewOpacity(Alpha), true, false, true);
        }
        if (FVector2D::Distance(P, HoverPoint) < 20)
        {
            const FString Label = E.bSelf ? E.Name + TEXT(" · you") : E.Name;
            const FVector2D Ext = Measure(Label, 11);
            Box(G, D, L + 8, P + FVector2D(-Ext.X * .5 - 6, 21), Ext + FVector2D(12, 7), Panel, true);
            Text(G, D, L + 9, P + FVector2D(-Ext.X * .5, 23), Label, 11, Color, false, false, true);
        }
    }
    DrawEnvironment(G, D, L + 10, true);
    Text(G, D, L + 12, FVector2D(606 + StoryExtra, 218), TEXT("N ^"), 10, Muted, true);
    Text(G, D, L + 12, FVector2D(680 + StoryExtra, 218), WindLabel(), 9, Muted);
    Text(G, D, L + 12, FVector2D(606 + StoryExtra, 236), ElevationLabel(), 8, Muted, true);
    Text(G, D, L + 12, FVector2D(1225, 218), TEXT("W YOU"), 8, Amber, true);
    Text(G, D, L + 12, FVector2D(1305, 218), TEXT("W PLAYER"), 8, Blue, true);
    Text(G, D, L + 12, FVector2D(1410, 218), TEXT("W RESIDENT"), 8, Sage, true);
    const auto Travel = Obj(Snapshot, TEXT("travel"));
    if (Bool(Travel, TEXT("active")) || Bool(Travel, TEXT("paused")))
    {
        const FVector2D P(606 + StoryExtra, 738);
        Box(G, D, L + 12, P, FVector2D(914 - StoryExtra, 37), Panel);
        Text(G, D, L + 13, P + FVector2D(10, 12),
             (TEXT("TRAVEL · ") + Str(Travel, TEXT("status"), TEXT("Following your route")))
                 .Left(StoryExtra > 150 ? 55 : 89),
             10, Bool(Travel, TEXT("paused")) ? Amber : Sage);
        const FVector2D Cancel(1420, 742);
        Box(G, D, L + 13, Cancel, FVector2D(92, 28), Raised);
        Text(G, D, L + 14, Cancel + FVector2D(9, 8), TEXT("STOP · ESC"), 9, Paper, true);
        Hits.Add({FSlateRect(Cancel.X, Cancel.Y, Cancel.X + 92, Cancel.Y + 28), TEXT("cancel_travel"), TEXT("")});
    }
    if (bFacingPreview && CanFaceAt(HoverPoint))
        Text(G, D, L + 12, FVector2D(606 + StoryExtra, 782), TEXT("ALT · CLICK TO TURN"), 9, Amber, true);
    else
        Text(G, D, L + 12, FVector2D(606 + StoryExtra, 782), PostureLabel(Obj(Snapshot, TEXT("self"))), 9, Sage);
    Text(G, D, L + 12, FVector2D(1415, 782), TEXT("LOCAL  /  Z ") + FString::FromInt(Num(Cell, TEXT("z"))), 9, Muted,
         true);
}

void SRatwGame::DrawPace(const FGeometry& G, FSlateWindowElementList& D, int32 L) const
{
    const auto Self = Obj(Snapshot, TEXT("self"));
    const double Left = 602 + StoryExtra, Width = 924 - StoryExtra, PaceWidth = Width * .51;
    const int32 Pace = DisplayPace(), Effective = (int32)BoundedNum(Self, TEXT("effectivePace"), 0, 10);
    const bool Exhausted = Bool(Self, TEXT("exhausted"));
    const double Stamina = BoundedNum(Self, TEXT("stamina"), 0, 100, 100);
    const double Rate = BoundedNum(Self, TEXT("staminaRate"), -100, 100);
    const FLinearColor PaceColor = Exhausted ? RGB(0xe1aba2) : (Pace >= 9 ? Amber : Sage);
    Text(G, D, L, FVector2D(Left, 827), FString::Printf(TEXT("PACE · %s %d/10"), *PaceLabel(Pace), Pace), 10, PaceColor,
         true);
    const FString Limiter = RequestedPace >= 0 && Clock - LastPaceRequest <= 1.5 ? TEXT("REQUESTING…")
                            : Exhausted                                          ? TEXT("EXHAUSTED · walking")
                            : Effective < Pace                                   ? TEXT("POSTURE-LIMITED")
                                                                                 : TEXT("wheel / PgUp PgDn");
    Text(G, D, L, FVector2D(Left + PaceWidth - 151, 829), Limiter, 8, Exhausted ? PaceColor : Muted);
    const double Step = PaceWidth / 11.;
    for (int32 I = 0; I <= 10; ++I)
    {
        const FVector2D P(Left + I * Step, 851), S(Step - 3, 10);
        Box(G, D, L, P, S, I <= Pace ? (I >= 9 ? Amber : Sage).CopyWithNewOpacity(I == Pace ? 1 : .45) : Raised);
        if (I == Pace)
            Frame(G, D, L + 1, P - FVector2D(0, 2), S + FVector2D(0, 4), PaceColor);
        Hits.Add({FSlateRect(P.X, 846, P.X + S.X, 879), TEXT("pace"), FString::FromInt(I)});
    }
    Text(G, D, L, FVector2D(Left, 867), TEXT("WALK"), 8, Muted, true);
    Text(G, D, L, FVector2D(Left + Step * 3, 867), TEXT("TROT"), 8, Muted, true);
    Text(G, D, L, FVector2D(Left + Step * 6, 867), TEXT("RUN"), 8, Muted, true);
    Text(G, D, L, FVector2D(Left + Step * 9, 867), TEXT("SPRINT"), 8, Amber, true);
    const double StaminaX = Left + PaceWidth + 23, StaminaWidth = Width - PaceWidth - 23;
    const FLinearColor EnergyColor = Exhausted ? RGB(0xe1aba2) : (Rate < -.01 ? Amber : Sage);
    Text(G, D, L, FVector2D(StaminaX, 827), FString::Printf(TEXT("STAMINA  %.0f%%"), Stamina), 10, EnergyColor, true);
    Text(G, D, L, FVector2D(StaminaX + StaminaWidth - 121, 829),
         FString::Printf(TEXT("DEX %.0f · TOP %.1f t/s"),
                         EnvironmentNumber(Self, TEXT("effectiveDexterity"), 0, 100,
                                           EnvironmentNumber(Self, TEXT("dexterity"), 0, 100, 0)),
                         BoundedNum(Self, TEXT("topSpeed"), 0, 100)),
         8, Muted);
    Box(G, D, L, FVector2D(StaminaX, 851), FVector2D(StaminaWidth, 10), Raised);
    Box(G, D, L + 1, FVector2D(StaminaX, 851), FVector2D(StaminaWidth * Stamina / 100., 10), EnergyColor);
    const FString RateText =
        Rate < -.01  ? FString::Printf(TEXT("DRAINING %.1f/s · ease pace for distance"), -Rate)
        : Rate > .01 ? (Stamina >= 99.95 ? TEXT("FULL · recovery is always active")
                                         : FString::Printf(TEXT("RECOVERING +%.1f/s · ongoing recovery"), Rate))
                     : TEXT("STEADY · sustainable travel");
    Text(G, D, L + 1, FVector2D(StaminaX, 867), RateText, 8, EnergyColor);
}

void SRatwGame::DrawWorld(const FGeometry& G, FSlateWindowElementList& D, int32 L) const
{
    auto Tab = [&](double X, const FString& Label, const FString& Action, bool Active) {
        const FVector2D P(X, 216), S(130, 30);
        Box(G, D, L + 1, P, S, Active ? Raised : Panel);
        Frame(G, D, L + 2, P, S, Active ? Sage : Line);
        Text(G, D, L + 2, P + FVector2D(12, 8), Label, 9, Active ? Sage : Muted, true);
        Hits.Add({FSlateRect(X, 216, X + S.X, 246), Action, TEXT("")});
    };
    Tab(1250, TEXT("NEARBY"), TEXT("nearby"), !bTravelAtlas);
    Tab(1390, TEXT("KNOWN ROUTES"), TEXT("atlas"), bTravelAtlas);
    if (bTravelAtlas)
    {
        DrawTravelAtlas(G, D, L);
        return;
    }
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
                    const auto* Info = Ch < 128 ? ratw::terrainInfo(char(Ch)) : nullptr;
                    const TCHAR Shape = !Info ? Ch : (bPlainGlyphs ? TCHAR(Info->ascii) : TCHAR(Info->glyph));
                    Text(G, D, L + 3, P + FVector2D(10 + TX * 11, 34 + TY * 14), FString::Chr(Shape), 9,
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

void SRatwGame::DrawTravelAtlas(const FGeometry& G, FSlateWindowElementList& D, int32 L) const
{
    Text(G, D, L + 1, FVector2D(613 + StoryExtra, 225), TEXT("YOUR TRAVEL ATLAS"), 10, Sage, true);
    Text(G, D, L + 1, FVector2D(613 + StoryExtra, 251),
         TEXT("Choose a place you have visited. Travel happens on foot, cell by cell."), 12, Muted);
    TArray<TSharedPtr<FJsonObject>> Cells;
    TSet<FString> Included;
    // This second projection contains only remembered geometry. Never fall back to live neighborhood data.
    for (const auto& Value : Arr(Snapshot, TEXT("travelMap")))
    {
        if (!Value || Value->Type != EJson::Object)
            continue;
        const auto C = Value->AsObject();
        const FString Id = Str(C, TEXT("id"));
        if (Str(C, TEXT("knowledge")) != TEXT("visited") || Id.IsEmpty() || Included.Contains(Id))
            continue;
        if (!FMath::IsFinite(Num(C, TEXT("x"))) || !FMath::IsFinite(Num(C, TEXT("y"))) ||
            !FMath::IsFinite(Num(C, TEXT("z"))))
            continue;
        Cells.Add(C);
        Included.Add(Id);
        if (Cells.Num() >= 256)
            break;
    }
    Cells.Sort([](const TSharedPtr<FJsonObject>& A, const TSharedPtr<FJsonObject>& B) {
        return Str(A, TEXT("name")).Compare(Str(B, TEXT("name"))) < 0;
    });
    const double Left = 613 + StoryExtra, ListX = 1302, MapWidth = ListX - Left - 25;
    const FVector2D MapTop(Left, 330), MapSize(MapWidth, 355);
    Box(G, D, L + 1, MapTop, MapSize, RGB(0x121b1c));
    Frame(G, D, L + 2, MapTop, MapSize, Line);
    if (Cells.IsEmpty())
    {
        Paragraph(
            G, D, L + 2, MapTop + FVector2D(22, 35),
            TEXT("No visited places have arrived yet. Places seen only in the distance cannot be travel destinations."),
            MapWidth - 44, 13, Muted);
        return;
    }
    double MinX = 1.e9, MinY = 1.e9, MaxX = -1.e9, MaxY = -1.e9;
    for (const auto& C : Cells)
    {
        const double X = BoundedNum(C, TEXT("x"), -1.e6, 1.e6), Y = BoundedNum(C, TEXT("y"), -1.e6, 1.e6);
        MinX = FMath::Min(MinX, X);
        MinY = FMath::Min(MinY, Y);
        MaxX = FMath::Max(MaxX, X + BoundedNum(C, TEXT("width"), 1, 256, 32));
        MaxY = FMath::Max(MaxY, Y + BoundedNum(C, TEXT("height"), 1, 256, 24));
    }
    const double Scale = FMath::Min((MapWidth - 44) / FMath::Max(1., MaxX - MinX), 311. / FMath::Max(1., MaxY - MinY));
    const FVector2D Offset = MapTop + (MapSize - FVector2D(MaxX - MinX, MaxY - MinY) * Scale) * .5;
    TMap<FString, FVector2D> Centers;
    const auto Travel = Obj(Snapshot, TEXT("travel"));
    const FString Destination = Str(Travel, TEXT("destination"));
    FString DestinationName = TEXT("Known destination");
    struct FAtlasLabel
    {
        FString Text;
        FVector2D Position;
        FSlateRect Bounds;
        FLinearColor Color;
        int32 Places = 1;
    };
    TArray<FAtlasLabel> Labels;
    for (const auto& C : Cells)
    {
        const FString Id = Str(C, TEXT("id"));
        const double W = BoundedNum(C, TEXT("width"), 1, 256, 32), H = BoundedNum(C, TEXT("height"), 1, 256, 24);
        const FVector2D Center = Offset + FVector2D(BoundedNum(C, TEXT("x"), -1.e6, 1.e6) - MinX + W * .5,
                                                    BoundedNum(C, TEXT("y"), -1.e6, 1.e6) - MinY + H * .5) *
                                              Scale;
        const FVector2D Size(FMath::Max(8., W * Scale - 4), FMath::Max(8., H * Scale - 4));
        const FVector2D P = Center - Size * .5;
        const bool Current = Id == CellId, Selected = Id == Destination;
        const FLinearColor Color = Current ? Amber : (Selected ? Sage : Muted.CopyWithNewOpacity(.55));
        Box(G, D, L + 2, P, Size, Current ? RGB(0x2e3325) : RGB(0x1c2927));
        Frame(G, D, L + 3, P, Size, Color);
        if (Size.X > 70 && Size.Y > 35)
        {
            const FString Name = Str(C, TEXT("name")).Left(FMath::Max(6, (int32)(Size.X / 7) - 2));
            const FVector2D LabelPosition = P + FVector2D(7, 7), Extent = Measure(Name, 9);
            const FSlateRect Bounds(LabelPosition.X, LabelPosition.Y, LabelPosition.X + Extent.X, LabelPosition.Y + 15);
            FAtlasLabel* Overlap = Labels.FindByPredicate([&](const FAtlasLabel& Label) {
                return Bounds.Left < Label.Bounds.Right && Bounds.Right > Label.Bounds.Left &&
                       Bounds.Top < Label.Bounds.Bottom && Bounds.Bottom > Label.Bounds.Top;
            });
            if (Overlap)
            {
                Overlap->Text = FString::Printf(TEXT("%d places · use list"), ++Overlap->Places);
                Overlap->Color = Muted;
                Overlap->Bounds.Right =
                    FMath::Max(Overlap->Bounds.Right, Overlap->Position.X + Measure(Overlap->Text, 9).X);
            }
            else
                Labels.Add({Name, LabelPosition, Bounds, Color, 1});
        }
        if (Current)
            Text(G, D, L + 5, Center - FVector2D(8, 5), TEXT("W>"), 12, Amber, true);
        if (Selected)
            DestinationName = Str(C, TEXT("name"), DestinationName);
        Centers.Add(Id, Center);
        if (!Current)
            Hits.Add({FSlateRect(P.X, P.Y, P.X + Size.X, P.Y + Size.Y), TEXT("travel"), Id});
    }
    // Off-map interiors may share a world origin with another cell. Their individual names stay in the list;
    // colliding labels become one hint, drawn after all rectangles so an overlapping cell cannot obscure it.
    for (const auto& Label : Labels)
        Text(G, D, L + 4, Label.Position, Label.Text, 9, Label.Color);
    FVector2D Previous;
    bool HasPrevious = false;
    for (const auto& Value : Arr(Travel, TEXT("route")))
    {
        const auto* Center = Value && Value->Type == EJson::String ? Centers.Find(Value->AsString()) : nullptr;
        if (!Center)
        {
            HasPrevious = false;
            continue;
        }
        if (HasPrevious)
            Lines(G, D, L + 4, {Previous, *Center}, Sage.CopyWithNewOpacity(.65), 2);
        Previous = *Center;
        HasPrevious = true;
    }
    constexpr int32 PerPage = 8;
    const int32 LastPage = (Cells.Num() - 1) / PerPage, Page = FMath::Clamp(TravelPage, 0, LastPage);
    for (int32 I = 0; I < PerPage && Page * PerPage + I < Cells.Num(); ++I)
    {
        const auto C = Cells[Page * PerPage + I];
        const FString Id = Str(C, TEXT("id"));
        const bool Current = Id == CellId, Selected = Id == Destination;
        const FVector2D P(ListX, 331 + I * 43);
        Box(G, D, L + 2, P, FVector2D(216, 39), Selected ? Raised : Panel);
        Text(G, D, L + 3, P + FVector2D(9, 5), Str(C, TEXT("name")).Left(26), 10, Current ? Amber : Paper);
        Text(G, D, L + 3, P + FVector2D(9, 23),
             FString::Printf(TEXT("%s · Z %.0f"), Current ? TEXT("YOU ARE HERE") : TEXT("VISITED · TRAVEL >"),
                             BoundedNum(C, TEXT("z"), -1.e6, 1.e6)),
             8, Current ? Amber : Muted, true);
        if (!Current)
            Hits.Add({FSlateRect(P.X, P.Y, P.X + 216, P.Y + 39), TEXT("travel"), Id});
    }
    Text(G, D, L + 2, FVector2D(Left, 697), TEXT("DIM = remembered geography · no live remote activity"), 9, Muted);
    Text(G, D, L + 2, FVector2D(ListX + 55, 695), FString::Printf(TEXT("%d / %d"), Page + 1, LastPage + 1), 9, Muted,
         true);
    for (int32 Direction : {-1, 1})
    {
        const double X = ListX + (Direction < 0 ? 0 : 171);
        Text(G, D, L + 2, FVector2D(X + 9, 695), Direction < 0 ? TEXT("<") : TEXT(">"), 11, Sage, true);
        if ((Direction < 0 && Page > 0) || (Direction > 0 && Page < LastPage))
            Hits.Add({FSlateRect(X, 687, X + 40, 717), TEXT("travel_page"), FString::FromInt(Direction)});
    }
    const bool Active = Bool(Travel, TEXT("active")), Paused = Bool(Travel, TEXT("paused"));
    if (Active || Paused)
    {
        Text(G, D, L + 2, FVector2D(Left, 734), (TEXT("TO ") + DestinationName).Left(StoryExtra > 150 ? 55 : 85), 12,
             Paused ? Amber : Sage, false, true);
        Text(G, D, L + 2, FVector2D(Left, 759),
             Str(Travel, TEXT("status"), TEXT("Following route")).Left(StoryExtra > 150 ? 63 : 93), 11, Muted);
        Box(G, D, L + 2, FVector2D(1390, 738), FVector2D(128, 35), Raised);
        Text(G, D, L + 3, FVector2D(1401, 749), TEXT("STOP · ESC"), 10, Paper, true);
        Hits.Add({FSlateRect(1390, 738, 1518, 773), TEXT("cancel_travel"), TEXT("")});
    }
    else
        Text(G, D, L + 2, FVector2D(Left, 745), TEXT("No teleporting. Closed doors require an explicit open action."),
             11, Muted);
    Text(G, D, L + 2, FVector2D(Left, 785), TEXT("WASD or a local click takes over · choose a comfortable pace below"),
         9, Muted);
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
        Text(G, D, L + 4, FVector2D(325, 271),
             FString::Printf(TEXT("AGE %d  ·  NORMAL  ·  A STORY STILL UNFOLDING"),
                             WholeCount(Self, TEXT("age"), 18, 10000)),
             10, Muted, true);
        Text(G, D, L + 4, FVector2D(325, 293),
             FString::Printf(TEXT("STRENGTH %.0f   DEXTERITY %.0f (%.1f effective)   WISDOM %.0f"),
                             EnvironmentNumber(Self, TEXT("strength"), 0, 100, 50),
                             EnvironmentNumber(Self, TEXT("dexterity"), 0, 100, 50),
                             EnvironmentNumber(Self, TEXT("effectiveDexterity"), 0, 100,
                                               EnvironmentNumber(Self, TEXT("dexterity"), 0, 100, 50)),
                             EnvironmentNumber(Self, TEXT("wisdom"), 0, 100, 50)),
             10, Sage, true);
        Box(G, D, L + 3, FVector2D(324, 318), FVector2D(509, 355), Ink);
        Frame(G, D, L + 4, FVector2D(324, 318), FVector2D(509, 355), Line);
        // The real portrait widget is overlaid here, independent from the W map token.
        Text(G, D, L + 5, FVector2D(342, 643),
             Num(Self, TEXT("shoulderHeightCm")) > 0
                 ? FString::Printf(TEXT("%s STATURE · %.0f CM AT SHOULDER · SAVED PROFILE"),
                       *Str(Obj(Self, TEXT("appearance")), TEXT("stature"), TEXT("average")).ToUpper(),
                       Num(Self, TEXT("shoulderHeightCm")))
                 : FString(TEXT("STATIC PROFILE · YOUR SAVED APPEARANCE")),
             9, Muted, true);
        Text(G, D, L + 5, FVector2D(866, 328), TEXT("PRESENT STATE"), 10, Amber, true);
        Text(G, D, L + 5, FVector2D(866, 357), PostureLabel(Self), 18, Paper);
        Paragraph(G, D, L + 5, FVector2D(866, 397), Str(Self, TEXT("state"), TEXT("Set your current state with /me.")),
                  365, 14, Muted);
        Paragraph(G, D, L + 5, FVector2D(866, 444), TEXT("/lay then move to sneak. /stand to walk normally."), 365, 11,
                  Sage, 1.45);
        Text(G, D, L + 5, FVector2D(866, 478), TEXT("ROLEPLAY PROGRESSION"), 10, Amber, true);
        Text(G, D, L + 5, FVector2D(866, 508),
             FString::Printf(TEXT("Level %d"), (int)Num(Self, TEXT("socialLevel"), 1)), 24, Paper);
        Text(G, D, L + 5, FVector2D(866, 550),
             FString::Printf(TEXT("%d social experience"), (int)Num(Self, TEXT("socialXp"))), 13, Sage);
        Box(G, D, L + 4, FVector2D(866, 582), FVector2D(354, 4), Line);
        Box(G, D, L + 5, FVector2D(866, 582),
            FVector2D(FMath::Clamp(Num(Self, TEXT("socialXp")) / 100., 0., 1.) * 354, 4), Sage);
        Text(G, D, L + 5, FVector2D(866, 613),
             FString::Printf(TEXT("Sneak %d / 100"), FMath::Clamp((int)Num(Self, TEXT("sneakSkill")), 0, 100)), 12,
             Sage);
        Text(G, D, L + 5, FVector2D(1043, 613),
             FString::Printf(TEXT("Hearing %d / 100"), FMath::Clamp((int)Num(Self, TEXT("hearingSkill")), 0, 100)), 12,
             Sage);
        Text(G, D, L + 5, FVector2D(866, 637),
             FString::Printf(TEXT("Scent %d / 100"), FMath::Clamp((int)Num(Self, TEXT("scentSkill")), 0, 100)), 12,
             Scent);
        Text(G, D, L + 5, FVector2D(1043, 637),
             FString::Printf(TEXT("Nose %d%%"),
                             FMath::RoundToInt(FMath::Clamp(Num(Self, TEXT("noseHealth"), 1.), 0., 1.) * 100)),
             12, Muted);
        Text(G, D, L + 5, FVector2D(866, 663), TEXT("SKILLS · TRAINING NOT IMPLEMENTED"), 9, Muted, true);
        Text(G, D, L + 5, FVector2D(326, 706), TEXT("DESCRIPTION"), 10, Amber, true);
        Paragraph(G, D, L + 5, FVector2D(326, 736),
                  Str(Self, TEXT("description"),
                      TEXT("Your appearance belongs here. Map tokens remain simple, leaving actions and expression to "
                           "the imagination.")),
                  690, 14, Paper);
    }
    else if (Modal == TEXT("inventory"))
    {
        Text(G, D, L + 4, FVector2D(324, 185), TEXT("BELONGINGS / EQUIPMENT"), 10, Amber, true);
        Text(G, D, L + 4, FVector2D(324, 221), TEXT("What you carry"), 35, Paper, false, true);
        Text(G, D, L + 4, FVector2D(325, 271), TEXT("Each object has a place in the story."), 14, Muted);
        Text(G, D, L + 4, FVector2D(856, 272),
             TEXT("PURSE · ") + CountText(Self, TEXT("cash")) + TEXT(" silver pennies"), 13, Amber);
        const auto Items = Arr(Snapshot, TEXT("inventory"));
        if (Items.IsEmpty())
            Paragraph(G, D, L + 5, FVector2D(326, 337),
                      TEXT("Your pack is empty. Objects you acquire will appear here, each with its own icon and "
                           "description."),
                      700, 16, Muted);
        for (int I = 0; I < Items.Num() && I < 6; ++I)
        {
            const auto Item = Items[I] && Items[I]->Type == EJson::Object ? Items[I]->AsObject() : nullptr;
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
            else if (Str(Item, TEXT("id")) == TEXT("herbs"))
            {
                Lines(G, D, L + 6, {Icon + FVector2D(28, 59), Icon + FVector2D(26, 7)}, Sage, 2);
                for (int Leaf = 0; Leaf < 3; ++Leaf)
                {
                    const double Y = 16 + Leaf * 13;
                    Lines(G, D, L + 6,
                          {Icon + FVector2D(27, Y + 8), Icon + FVector2D(8, Y - 3), Icon + FVector2D(17, Y + 10),
                           Icon + FVector2D(27, Y + 8), Icon + FVector2D(47, Y - 4), Icon + FVector2D(38, Y + 11)},
                          Sage, 1.6);
                }
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
            Text(G, D, L + 6, P + FVector2D(114, 50),
                 (Bool(Item, TEXT("equipped")) ? FString(TEXT("EQUIPPED")) : FString(TEXT("CARRIED"))) + TEXT(" · × ") +
                     FString::FromInt(WholeCount(Item, TEXT("quantity"), 1)),
                 9, Bool(Item, TEXT("equipped")) ? Sage : Muted, true);
            Paragraph(G, D, L + 6, P + FVector2D(114, 72), Str(Item, TEXT("description")), 316, 12, Muted, 1.4);
        }
        if (InventoryQuantity(TEXT("meal")) > 0)
            Button(FVector2D(326, 765), FVector2D(160, 39), TEXT("EAT ONE MEAL"), TEXT("eat"));
        if (CanGather())
            Button(FVector2D(501, 765), FVector2D(165, 39), TEXT("GATHER HERBS"), TEXT("gather"));
        else if (const auto Resource = VisibleResource())
            Text(G, D, L + 5, FVector2D(505, 779),
                 WholeCount(Resource, TEXT("remaining")) > 0 ? TEXT("Approach the herb patch to gather.")
                                                             : TEXT("The visible herb patch is depleted."),
                 11, Muted);
        const FString MerchantId = Str(Obj(Snapshot, TEXT("merchant")), TEXT("id"));
        if (!MerchantId.IsEmpty())
            Button(FVector2D(949, 765), FVector2D(285, 39),
                   MerchantId == TEXT("npc_keeper") ? TEXT("TRADE WITH THE KEEPER") : TEXT("TRADE WITH THE SHOPKEEPER"),
                   TEXT("trade_open"), MerchantId);
        Text(G, D, L + 5, FVector2D(326, 823), TEXT("Equipment appears on your sheet. Your map presence remains W>."),
             12, Muted);
    }
    else if (Modal == TEXT("trade"))
    {
        const auto Merchant = Obj(Snapshot, TEXT("merchant"));
        const bool Available = !Str(Merchant, TEXT("id")).IsEmpty();
        Text(G, D, L + 4, FVector2D(324, 185), TEXT("LOCAL TRADE / REAL GOODS & REAL PURSES"), 10, Amber, true);
        Text(G, D, L + 4, FVector2D(324, 221),
             Available ? Str(Merchant, TEXT("name"), TEXT("The keeper")).Left(38) : TEXT("The counter is unattended"),
             31, Paper, false, true);
        if (!Available)
        {
            Paragraph(G, D, L + 5, FVector2D(326, 320),
                      TEXT("The keeper is no longer awake, visible, and within reach. Return to them to see current "
                           "stock and offers. Old quotes are not retained."),
                      860, 17, Muted, 1.7);
        }
        else
        {
            Text(G, D, L + 4, FVector2D(326, 273), TEXT("One item per exchange. The authority rechecks every offer."),
                 13, Muted);
            Text(G, D, L + 5, FVector2D(326, 307), TEXT("YOUR PURSE · ") + CountText(Self, TEXT("cash")) + TEXT(" p"),
                 12, Amber, true);
            Text(G, D, L + 5, FVector2D(804, 307),
                 TEXT("KEEPER'S PURSE · ") + CountText(Merchant, TEXT("cash")) + TEXT(" p"), 12, Sage, true);
            const FString Goods[] = {TEXT("herbs"), TEXT("meal")};
            for (int I = 0; I < 2; ++I)
            {
                const auto Item = TradeItem(Goods[I]);
                const FVector2D P(326, 347 + I * 204);
                Box(G, D, L + 4, P, FVector2D(908, 186), Ink);
                Frame(G, D, L + 5, P, FVector2D(908, 186), Line);
                Text(G, D, L + 5, P + FVector2D(20, 16), I == 0 ? TEXT("Cooking herbs") : TEXT("Prepared meal"), 21,
                     Paper, false, true);
                Text(G, D, L + 5, P + FVector2D(21, 53),
                     TEXT("KEEPER STOCK ") + CountText(Item, TEXT("stock")) + TEXT(" · YOU CARRY ") +
                         CountText(Item, TEXT("owned")),
                     10, Muted, true);
                for (bool Buy : {true, false})
                {
                    const FVector2D B = P + FVector2D(Buy ? 21 : 465, 83);
                    const bool Enabled = CanTradeItem(Goods[I], Buy);
                    const FString Label = FString(Buy ? TEXT("BUY 1 · ") : TEXT("SELL 1 · ")) +
                                          CountText(Item, Buy ? TEXT("buyPrice") : TEXT("sellPrice")) + TEXT(" p");
                    if (Enabled)
                        Button(B, FVector2D(214, 39), Label, Buy ? TEXT("trade_buy") : TEXT("trade_sell"), Goods[I]);
                    else
                    {
                        Box(G, D, L + 5, B, FVector2D(214, 39), Panel);
                        Text(G, D, L + 6, B + FVector2D(12, 11), Label, 12, Muted);
                    }
                    Paragraph(G, D, L + 5, B + FVector2D(0, 51),
                              Enabled ? (Buy ? TEXT("Your purse pays for one item from the keeper's stock.")
                                             : TEXT("The keeper pays for one item from your pack."))
                                      : Str(Item, Buy ? TEXT("buyReason") : TEXT("sellReason"),
                                            TEXT("This offer is currently unavailable."))
                                            .Left(102),
                              403, 12, Enabled ? Muted : Amber, 1.4);
                }
            }
            Text(G, D, L + 5, FVector2D(326, 786), TEXT("p = silver penny · stock, demand, and cash are finite"), 12,
                 Muted);
            Text(G, D, L + 5, FVector2D(326, 811), TEXT("Prices can change as residents gather, cook, buy, and eat."),
                 12, Muted);
        }
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
               bReducedMotion ? TEXT("Reduced motion: On · static weather") : TEXT("Reduced motion: Off"),
               TEXT("motion"));
        Button(FVector2D(886, 535), FVector2D(170, 45), bFlatWorld ? TEXT("World: Always flat") : TEXT("World: Automatic"),
               TEXT("projection"));
        Button(FVector2D(1063, 535), FVector2D(170, 45), bPlainGlyphs ? TEXT("Map: Plain ASCII") : TEXT("Map: Unicode"),
               TEXT("glyphs"));
        const FString SplitName =
            StoryExtra < 0 ? TEXT("Compact narrative")
                           : (StoryExtra == 0 ? TEXT("Balanced")
                                              : (StoryExtra == 150 ? TEXT("Wide narrative") : TEXT("Text-first")));
        Button(FVector2D(886, 596), FVector2D(347, 45), TEXT("Pane balance: ") + SplitName, TEXT("split"));
        if (Bool(Snapshot, TEXT("devTools")))
        {
            Text(G, D, L + 5, FVector2D(326, 675), TEXT("WORLD CLOCK · DEVELOPMENT ONLY"), 9, Muted, true);
            const FString Times[] = {TEXT("dawn"), TEXT("day"), TEXT("dusk"), TEXT("night")};
            for (int I = 0; I < 4; ++I)
                Button(FVector2D(326 + I * 122, 692), FVector2D(112, 34), Times[I], TEXT("time"), Times[I],
                       Environment.Phase == Times[I]);
            Text(G, D, L + 5, FVector2D(326, 738), TEXT("ROOM LIGHTING · DEVELOPMENT ONLY"), 9, Muted, true);
            const FString Lighting[] = {TEXT("warm"), TEXT("unlit"), TEXT("daylit"), TEXT("cool")};
            for (int I = 0; I < 4; ++I)
                Button(FVector2D(326 + I * 122, 755), FVector2D(112, 34), Lighting[I], TEXT("lighting"), Lighting[I]);
            Text(G, D, L + 5, FVector2D(886, 663), TEXT("DEVELOPMENT WEATHER"), 9, Muted, true);
            const FString Weathers[] = {TEXT("clear"), TEXT("overcast"), TEXT("rain"), TEXT("storm"),
                                        TEXT("fog"),   TEXT("snow"),     TEXT("sandstorm")};
            const FString Short[] = {TEXT("clear"), TEXT("cloud"), TEXT("rain"), TEXT("storm"),
                                     TEXT("fog"),   TEXT("snow"),  TEXT("sand")};
            for (int I = 0; I < 7; ++I)
                Button(FVector2D(886 + I * 50, 686), FVector2D(47, 39), Short[I], TEXT("weather"), Weathers[I]);
            Text(G, D, L + 5, FVector2D(886, 737), TEXT("WIND FLOW · DEVELOPMENT ONLY"), 9, Muted, true);
            const FString Winds[] = {TEXT("east"), TEXT("west"), TEXT("north"), TEXT("calm"), TEXT("live")};
            for (int I = 0; I < 5; ++I)
                Button(FVector2D(886 + I * 70, 757), FVector2D(66, 34), Winds[I], TEXT("wind"), Winds[I]);
            Button(FVector2D(886, 800), FVector2D(105, 32), TEXT("Day +1"), TEXT("calendar"), TEXT("day"));
            Button(FVector2D(1005, 800), FVector2D(105, 32), TEXT("Year +1"), TEXT("calendar"), TEXT("year"));
            Button(FVector2D(1124, 800), FVector2D(110, 32), TEXT("Seasonal"), TEXT("weather"), TEXT("seasonal"));
        }
        Text(G, D, L + 5, FVector2D(326, 798), TEXT("ENTER  write / send     SHIFT + ENTER  newline"), 10, Muted, true);
        Text(G, D, L + 5, FVector2D(326, 814), TEXT("ESC  preserve draft"), 10, Muted, true);
        Text(G, D, L + 5, FVector2D(326, 835),
             TEXT("ALT + mouse previews facing; click to turn. /lay + move sneaks; /stand rises."), 12, Muted);
    }
    else if (Modal == TEXT("leave_character"))
    {
        Text(G, D, L + 4, FVector2D(324, 185), TEXT("RETURN TO YOUR CHARACTERS"), 10, Amber, true);
        Text(G, D, L + 4, FVector2D(324, 249), TEXT("Leave this character?"), 33, Paper);
        Paragraph(G, D, L + 5, FVector2D(326, 322),
                  TEXT("Your character remains saved. Returning to selection ends this play session. Unsent drafts and this session's local transcript are not kept when switching characters."),
                  876, 19, Muted, 1.7);
    }
    else
    {
        Text(G, D, L + 4, FVector2D(324, 185), TEXT("A CLOSER LOOK"), 10, Amber, true);
        if (PortraitAppearance())
        {
            Box(G, D, L + 3, FVector2D(324, 285), FVector2D(449, 355), Ink);
            Frame(G, D, L + 4, FVector2D(324, 285), FVector2D(449, 355), Line);
            Text(G, D, L + 5, FVector2D(326, 670),
                 Str(InspectedCharacter, TEXT("lifeStage"), TEXT("adult")).ToUpper() +
                     (Num(InspectedCharacter, TEXT("shoulderHeightCm")) > 0
                          ? FString::Printf(TEXT(" · %.0f CM AT SHOULDER"), Num(InspectedCharacter, TEXT("shoulderHeightCm")))
                          : FString(TEXT(" · STATIC CHARACTER PROFILE"))),
                 10, Sage, true);
            Paragraph(G, D, L + 5, FVector2D(805, 254), InspectedText, 426, 16, Paper, 1.6);
        }
        else
            Paragraph(G, D, L + 5, FVector2D(325, 254), InspectedText, 876, 19, Paper, 1.7);
        Text(G, D, L + 5, FVector2D(326, 787),
             TEXT("Only information your character is allowed to perceive appears here."), 12, Muted);
    }
}
