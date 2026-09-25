#pragma once

#include "CoreMinimal.h"

// A short history of authoritative poses, not a client-side simulation. Rendering
// between samples preserves velocity instead of repeatedly easing to a target.
struct FRatwMotionBuffer
{
    struct FPose
    {
        double Time = 0;
        FVector2D Position = FVector2D::ZeroVector;
        double Facing = 0;
    };
    TArray<FPose> Samples;

    bool Add(double Time, FVector2D Position, double Facing)
    {
        if (!FMath::IsFinite(Time) || !FMath::IsFinite(Position.X) ||
            !FMath::IsFinite(Position.Y) || !FMath::IsFinite(Facing)) return false;
        if (!Samples.IsEmpty() && Time <= Samples.Last().Time) return false;
        // A discontinuity must never animate through intervening walls/rooms.
        if (!Samples.IsEmpty() && (Time - Samples.Last().Time > .5 ||
            FVector2D::Distance(Position, Samples.Last().Position) > 8)) Samples.Empty();
        Samples.Add({Time, Position, Facing});
        if (Samples.Num() > 32) Samples.RemoveAt(0, Samples.Num() - 32);
        return true;
    }

    FPose At(double Time) const
    {
        if (Samples.IsEmpty()) return {};
        if (Time <= Samples[0].Time) return Samples[0];
        for (int32 I = 1; I < Samples.Num(); ++I)
            if (Time < Samples[I].Time)
            {
                const auto& A = Samples[I - 1];
                const auto& B = Samples[I];
                const double Alpha = (Time - A.Time) / (B.Time - A.Time);
                const double Arc = FMath::Atan2(FMath::Sin(B.Facing - A.Facing), FMath::Cos(B.Facing - A.Facing));
                return {Time, FMath::Lerp(A.Position, B.Position, Alpha), A.Facing + Arc * Alpha};
            }
        // No speculative extrapolation past an obstacle or out of visibility.
        return Samples.Last();
    }
};
