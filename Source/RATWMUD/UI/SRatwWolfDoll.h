#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Dom/JsonObject.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

/** Shared sheet/inspection/creator portrait. Never used for the map's W token. */
class SRatwWolfDoll : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SRatwWolfDoll) : _Age(18.) {}
    SLATE_ATTRIBUTE(TSharedPtr<FJsonObject>, Appearance)
    SLATE_ATTRIBUTE(double, Age)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);
    void SetAppearance(TSharedPtr<FJsonObject> Value);
    void SetAge(double Value);
    virtual void Tick(const FGeometry&, double, float) override;
    virtual FVector2D ComputeDesiredSize(float) const override;
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry&, const FSlateRect&, FSlateWindowElementList&,
                          int32, const FWidgetStyle&, bool) const override;
    bool HasPortrait() const { return Texture.IsValid(); }

private:
    TAttribute<TSharedPtr<FJsonObject>> AppearanceAttribute;
    TAttribute<double> AgeAttribute;
    TStrongObjectPtr<UTexture2D> Texture;
    FSlateBrush Brush;
    FString LastKey;
    double NextRefresh = 0;
    float StatureScale = 1;
    void Refresh();
};
