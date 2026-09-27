#include "UI/RatwWeatherArt.h"

namespace RatwWeatherArt
{
namespace
{
uint32 Hash(uint32 X, uint32 Y, uint32 Seed)
{
    uint32 H = X * 0x8da6b343u ^ Y * 0xd8163841u ^ Seed * 0xcb1ab31fu;
    H ^= H >> 13;
    H *= 0x5bd1e995u;
    return H ^ (H >> 15);
}
float Unit(uint32 X, uint32 Y, uint32 Seed)
{
    return (Hash(X, Y, Seed) & 0xffffff) / float(0xffffff);
}
float Smooth(float A, float B, float V)
{
    const float T = FMath::Clamp((V - A) / (B - A), 0.f, 1.f);
    return T * T * (3.f - 2.f * T);
}
// Value noise on a lattice that wraps every Px by Py cells, so the sheet tiles.
float Lattice(float U, float V, int32 Px, int32 Py, uint32 Seed)
{
    const int32 X0 = FMath::FloorToInt(U), Y0 = FMath::FloorToInt(V);
    const float Fx = Smooth(0, 1, U - X0), Fy = Smooth(0, 1, V - Y0);
    auto At = [&](int32 X, int32 Y) { return Unit(uint32((X % Px + Px) % Px), uint32((Y % Py + Py) % Py), Seed); };
    return FMath::Lerp(FMath::Lerp(At(X0, Y0), At(X0 + 1, Y0), Fx), FMath::Lerp(At(X0, Y0 + 1), At(X0 + 1, Y0 + 1), Fx),
                       Fy);
}
// Fractal noise over the whole sheet: X/Y in [0, Size), base periods in lattice cells.
float Fbm(int32 X, int32 Y, int32 Px, int32 Py, int32 Octaves, uint32 Seed)
{
    float Sum = 0, Weight = .5f, Total = 0;
    for (int32 O = 0; O < Octaves; ++O, Px *= 2, Py *= 2, Weight *= .5f)
    {
        Sum += Weight * Lattice(float(X) / Size * Px, float(Y) / Size * Py, Px, Py, Seed + O * 7919u);
        Total += Weight;
    }
    return Sum / Total;
}
int32 Wrap(int32 V)
{
    return (V % Size + Size) % Size;
}
} // namespace

TArray64<uint8> Pixels(EArt Art)
{
    TArray<float> Alpha;
    Alpha.SetNumZeroed(Size * Size);
    auto Put = [&](int32 X, int32 Y, float A) {
        float& Cell = Alpha[Wrap(Y) * Size + Wrap(X)];
        Cell = FMath::Max(Cell, A);
    };
    switch (Art)
    {
    case EArt::Mist:
        for (int32 Y = 0; Y < Size; ++Y)
            for (int32 X = 0; X < Size; ++X)
                Alpha[Y * Size + X] = Smooth(.34f, .82f, Fbm(X, Y, 4, 4, 4, 11)) * .92f;
        break;
    case EArt::Cloud:
        for (int32 Y = 0; Y < Size; ++Y)
            for (int32 X = 0; X < Size; ++X)
                Alpha[Y * Size + X] = Smooth(.5f, .66f, Fbm(X, Y, 3, 3, 5, 23));
        break;
    case EArt::Dust:
        // Long in X, short in Y: gusts of sand read as streaks along the wind.
        for (int32 Y = 0; Y < Size; ++Y)
            for (int32 X = 0; X < Size; ++X)
                Alpha[Y * Size + X] =
                    FMath::Clamp(Smooth(.42f, .84f, Fbm(X, Y, 2, 14, 4, 37)) * .8f +
                                     Smooth(.55f, .9f, Fbm(X, Y, 4, 48, 2, 41)) * .45f,
                                 0.f, 1.f);
        break;
    case EArt::Rain:
        for (uint32 I = 0; I < 150; ++I)
        {
            const int32 X = int32(Unit(I, 1, 53) * Size), Y0 = int32(Unit(I, 2, 53) * Size);
            const int32 Length = 9 + int32(Unit(I, 3, 53) * 22);
            const float Bright = .45f + .55f * Unit(I, 4, 53);
            // The head (bottom, the direction of fall) is brightest; the tail fades out above it.
            for (int32 T = 0; T <= Length; ++T)
            {
                const float A = Bright * FMath::Pow(float(T) / Length, 1.4f);
                Put(X, Y0 + T, A);
                Put(X + 1, Y0 + T, A * .22f);
            }
        }
        break;
    case EArt::Snow:
        for (uint32 I = 0; I < 95; ++I)
        {
            const float Cx = Unit(I, 1, 71) * Size, Cy = Unit(I, 2, 71) * Size;
            const float R = .9f + FMath::Square(Unit(I, 3, 71)) * 2.2f;
            for (int32 Dy = -3; Dy <= 3; ++Dy)
                for (int32 Dx = -3; Dx <= 3; ++Dx)
                {
                    const int32 X = FMath::FloorToInt(Cx) + Dx, Y = FMath::FloorToInt(Cy) + Dy;
                    const float D = FMath::Sqrt(FMath::Square(X + .5f - Cx) + FMath::Square(Y + .5f - Cy));
                    Put(X, Y, 1.f - Smooth(R * .4f, R, D));
                }
        }
        break;
    case EArt::Pool:
        for (int32 Y = 0; Y < Size; ++Y)
            for (int32 X = 0; X < Size; ++X)
            {
                const float D = FMath::Sqrt(FMath::Square(X + .5f - Size * .5f) + FMath::Square(Y + .5f - Size * .5f)) /
                                (Size * .5f);
                Alpha[Y * Size + X] = Smooth(.5f, 1.f, D);
            }
        break;
    default:
        break;
    }
    TArray64<uint8> Out;
    Out.SetNumUninitialized(int64(Size) * Size * 4);
    for (int32 I = 0; I < Size * Size; ++I)
    {
        Out[I * 4] = Out[I * 4 + 1] = Out[I * 4 + 2] = 255;
        Out[I * 4 + 3] = uint8(FMath::Clamp(FMath::RoundToInt(Alpha[I] * 255.f), 0, 255));
    }
    return Out;
}

const FSlateBrush* FSheets::Brush(EArt Art)
{
    const int32 Index = int32(Art);
    if (Index < 0 || Index >= int32(EArt::Count))
        return nullptr;
    if (!Textures[Index].IsValid())
    {
        UTexture2D* Texture = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8, NAME_None, Pixels(Art));
        if (!Texture)
            return nullptr;
        Texture->Filter = TF_Bilinear;
        Texture->SRGB = false;
        Texture->NeverStream = true;
        Texture->AddressX = Texture->AddressY = Art == EArt::Pool ? TA_Clamp : TA_Wrap;
        Texture->UpdateResource();
        Textures[Index].Reset(Texture);
        FSlateBrush& Brush = Brushes[Index];
        Brush.SetResourceObject(Texture);
        Brush.ImageSize = FVector2D(Size, Size);
        Brush.DrawAs = ESlateBrushDrawType::Image;
        Brush.Tiling = Art == EArt::Pool ? ESlateBrushTileType::NoTile : ESlateBrushTileType::Both;
    }
    return &Brushes[Index];
}
} // namespace RatwWeatherArt
