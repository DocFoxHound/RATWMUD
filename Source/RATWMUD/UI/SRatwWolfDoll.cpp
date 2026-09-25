#include "UI/SRatwWolfDoll.h"
#include "Core/RatwAppearance.h"
#include "Runtime/RatwJson.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
struct FAtlas
{
    int32 Width = 0, Height = 0;
    TArray64<uint8> Pixels;
};

// Only five fixed, validated asset names are accepted, never a client file path.
TSharedPtr<FAtlas> AtlasFor(const FString& Species)
{
    static TMap<FString, TSharedPtr<FAtlas>> Cache;
    if (const auto* Existing = Cache.Find(Species)) return *Existing;
    auto Atlas = MakeShared<FAtlas>();
    TArray<uint8> Png;
    const FString Path = FPaths::ProjectDir() / TEXT("Data/Portraits") / (Species + TEXT(".png"));
    if (!FFileHelper::LoadFileToArray(Png, *Path) || Png.Num() > 16 * 1024 * 1024) return nullptr;
    auto& Module = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    const auto Decoder = Module.CreateImageWrapper(EImageFormat::PNG);
    if (!Decoder || !Decoder->SetCompressed(Png.GetData(), Png.Num())) return nullptr;
    // Unreal may expose PNG RGBA8 pixels in its native BGRA byte order.
    const ERGBFormat Format = Decoder->GetFormat();
    if ((Format != ERGBFormat::RGBA && Format != ERGBFormat::BGRA) || Decoder->GetBitDepth() != 8) return nullptr;
    Atlas->Width = Decoder->GetWidth();
    Atlas->Height = Decoder->GetHeight();
    if (Atlas->Width < 4 || Atlas->Height < 4 || Atlas->Width > 4096 || Atlas->Height > 4096 ||
        Atlas->Width % 2 || Atlas->Height % 2 || !Decoder->GetRaw(Atlas->Pixels) ||
        Atlas->Pixels.Num() != int64(Atlas->Width) * Atlas->Height * 4) return nullptr;
    if (Format == ERGBFormat::BGRA)
        for (int64 I = 0; I < Atlas->Pixels.Num(); I += 4) Swap(Atlas->Pixels[I], Atlas->Pixels[I + 2]);
    Cache.Add(Species, Atlas);
    return Atlas;
}

float Smooth(float A, float B, float V)
{
    const float T = FMath::Clamp((V - A) / (B - A), 0.f, 1.f);
    return T * T * (3.f - 2.f * T);
}
float Patch(float X, float Y, float Cx, float Cy, float Rx, float Ry)
{
    const float Dx = (X - Cx) / Rx, Dy = (Y - Cy) / Ry;
    return 1.f - Smooth(.64f, 1.f, Dx * Dx + Dy * Dy);
}
FVector Coat(int Index)
{
    const auto Color = FColor::FromHex(UTF8_TO_TCHAR(ratw::coatColorHex(Index)));
    return FVector(Color.R / 255., Color.G / 255., Color.B / 255.);
}
}

void SRatwWolfDoll::Construct(const FArguments& Args)
{
    AppearanceAttribute = Args._Appearance;
    AgeAttribute = Args._Age;
    SetCanTick(true);
    Refresh();
}
void SRatwWolfDoll::SetAppearance(TSharedPtr<FJsonObject> Value)
{
    AppearanceAttribute.Set(Value);
    Refresh();
}
void SRatwWolfDoll::SetAge(double Value)
{
    AgeAttribute.Set(Value);
    Refresh();
}
void SRatwWolfDoll::Tick(const FGeometry& Geometry, double Time, float Delta)
{
    SLeafWidget::Tick(Geometry, Time, Delta);
    if (Time >= NextRefresh)
    {
        NextRefresh = Time + .08;
        Refresh();
    }
}
FVector2D SRatwWolfDoll::ComputeDesiredSize(float) const { return FVector2D(480, 330); }

void SRatwWolfDoll::Refresh()
{
    ratw::Appearance Appearance;
    if (const auto Object = AppearanceAttribute.Get(); Object.IsValid())
        if (!ratwjson::ReadAppearance(Object, Appearance))
        {
            Texture.Reset();
            Brush.SetResourceObject(nullptr);
            LastKey.Empty();
            return;
        }
    const double RawAge = AgeAttribute.Get(18.);
    const int32 Age = FMath::IsFinite(RawAge) ? int32(FMath::Clamp(RawAge, 0., 10000.)) : 18;
    const int32 Frame = int32(ratw::lifeStage(Age));
    const FString Species = UTF8_TO_TCHAR(Appearance.species.c_str());
    const FString Key = FString::Printf(TEXT("%s:%d:%s:%s:%d:%d:%d:%.4f:%.4f"), *Species, Frame,
        UTF8_TO_TCHAR(Appearance.stature.c_str()), UTF8_TO_TCHAR(Appearance.pattern.c_str()),
        Appearance.baseColor, Appearance.gradientColor, Appearance.markingColor,
        Appearance.gradientAmount, Appearance.patternAmount);
    if (LastKey == Key) return;
    LastKey = Key;
    StatureScale = Appearance.stature == "short" ? .88f : Appearance.stature == "tall" ? 1.12f : 1.f;
    const auto Atlas = AtlasFor(Species);
    Texture.Reset();
    Brush.SetResourceObject(nullptr);
    if (!Atlas) return;
    const int32 W = Atlas->Width / 2, H = Atlas->Height / 2;
    const int32 Ox = (Frame % 2) * W, Oy = (Frame / 2) * H;
    TArray64<uint8> Pixels;
    Pixels.SetNumUninitialized(int64(W) * H * 4);
    const FVector Base = Coat(Appearance.baseColor), Gradient = Coat(Appearance.gradientColor);
    const FVector Marking = Coat(Appearance.markingColor), Highlight = Coat(0);
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X)
        {
            const uint8* Source = Atlas->Pixels.GetData() + (int64(Y + Oy) * Atlas->Width + X + Ox) * 4;
            uint8* Out = Pixels.GetData() + (int64(Y) * W + X) * 4;
            const float U = float(X) / W, V = float(Y) / H;
            const float L = (Source[0] * .2126f + Source[1] * .7152f + Source[2] * .0722f) / 255.f;
            FVector Color = FMath::Lerp(Base, Gradient, Smooth(.2f, .88f, V) * float(Appearance.gradientAmount));
            float Mask = 0;
            if (Appearance.pattern == "saddle")
                Mask = Patch(U, V, .49f, .44f, .27f, .14f);
            else if (Appearance.pattern == "mantle")
                Mask = FMath::Max(Patch(U, V, .48f, .42f, .34f, .18f), Patch(U, V, .27f, .36f, .11f, .2f));
            else if (Appearance.pattern == "piebald")
                Mask = FMath::Max(Patch(U, V, .4f, .53f, .11f, .16f),
                       FMath::Max(Patch(U, V, .66f, .57f, .1f, .19f), Patch(U, V, .22f, .3f, .065f, .12f)));
            Color = FMath::Lerp(Color, Marking, Mask * float(Appearance.patternAmount));
            // Keep painted fur clusters, dark outlines, eyes and highlights intact.
            if (L < .17f) Color = FVector(L * .7f, L * .75f, L * .78f);
            else
            {
                Color *= .30f + L * 1.02f;
                Color = FMath::Lerp(Color, Highlight, Smooth(.76f, 1.f, L) * .56f);
            }
            // Transient texture is BGRA; preserve every original alpha sample.
            Out[0] = uint8(FMath::Clamp(Color.Z * 255., 0., 255.));
            Out[1] = uint8(FMath::Clamp(Color.Y * 255., 0., 255.));
            Out[2] = uint8(FMath::Clamp(Color.X * 255., 0., 255.));
            Out[3] = Source[3];
        }
    Texture.Reset(UTexture2D::CreateTransient(W, H, PF_B8G8R8A8, NAME_None, Pixels));
    if (!Texture.IsValid()) return;
    Texture->Filter = TF_Nearest;
    Texture->SRGB = true;
    Texture->NeverStream = true;
    Texture->UpdateResource();
    Brush.SetResourceObject(Texture.Get());
    Brush.ImageSize = FVector2D(W, H);
    Brush.DrawAs = ESlateBrushDrawType::Image;
    Brush.Tiling = ESlateBrushTileType::NoTile;
    Invalidate(EInvalidateWidgetReason::Paint);
}

int32 SRatwWolfDoll::OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
                            FSlateWindowElementList& Elements, int32 Layer,
                            const FWidgetStyle& Style, bool) const
{
    const FVector2D Size = Geometry.GetLocalSize();
    if (!Texture.IsValid())
    {
        FSlateDrawElement::MakeText(Elements, Layer, Geometry.ToPaintGeometry(),
            TEXT("Portrait unavailable"), FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 12),
            ESlateDrawEffect::None, FLinearColor(.65f,.65f,.6f));
        return Layer;
    }
    // Reserve maximum stature space, so changing stature genuinely changes height.
    const float Scale = FMath::Min(Size.X / Brush.ImageSize.X, Size.Y / (Brush.ImageSize.Y * 1.12f));
    const FVector2D DrawSize(Brush.ImageSize.X * Scale, Brush.ImageSize.Y * Scale * StatureScale);
    const FVector2D Position((Size.X - DrawSize.X) * .5, Size.Y * .94 - DrawSize.Y * .9);
    FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(DrawSize, FSlateLayoutTransform(Position)),
        &Brush, ESlateDrawEffect::None, Style.GetColorAndOpacityTint());
    return Layer;
}
