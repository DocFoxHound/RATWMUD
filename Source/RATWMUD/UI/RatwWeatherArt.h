#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateImageBrush.h"
#include "Engine/Texture2D.h"
#include "UObject/StrongObjectPtr.h"

/**
 * Stylized weather art, painted in code rather than imported: every sheet is white with a
 * shaped alpha, so one texture serves any tint, and every sheet except the light pool tiles
 * seamlessly. Nothing here reads the world; the widget decides what to draw and where.
 */
namespace RatwWeatherArt
{
enum class EArt : uint8
{
    Mist,  // Soft rolling fog banks.
    Cloud, // Separated cloud masses: their shadows cross the ground on clear days.
    Rain,  // Short vertical streaks with a bright head and a fading tail.
    Snow,  // Soft flakes of several sizes.
    Dust,  // Wind-stretched streaks of blown sand, running along the texture's X axis.
    Pool,  // Clear centre fading to opaque edge: the night beyond a wolf's own sight. Does not tile.
    Count
};
constexpr int32 Size = 256;

/** Row-major BGRA8 pixels, Size x Size. Deterministic, so tests can inspect them. */
TArray64<uint8> Pixels(EArt Art);

/** Lazily built transient textures and brushes for one widget; release with the widget. */
class FSheets
{
  public:
    const FSlateBrush* Brush(EArt Art);

  private:
    TStrongObjectPtr<UTexture2D> Textures[int32(EArt::Count)];
    FSlateBrush Brushes[int32(EArt::Count)];
};
} // namespace RatwWeatherArt
