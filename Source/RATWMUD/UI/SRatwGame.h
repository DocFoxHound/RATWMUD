#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Dom/JsonObject.h"
#include "UI/RatwMotionBuffer.h"
#include "UI/RatwWeatherArt.h"

class SMultiLineEditableTextBox;

/** The client is a projection of observer-filtered server data. No simulation lives here. */
class SRatwGame : public SCompoundWidget
{
  public:
    SLATE_BEGIN_ARGS(SRatwGame) {}
    SLATE_ARGUMENT(TFunction<void(const FString&)>, OnCommand)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    void ApplySnapshot(const TSharedPtr<FJsonObject>& InSnapshot);
    void ApplyMotion(const TSharedPtr<FJsonObject>& Frame);
    void ReceiveEvent(const TSharedPtr<FJsonObject>& Event);
    void SetPresentationPage(const FString& Page);
    virtual bool SupportsKeyboardFocus() const override
    {
        return true;
    }
    virtual void Tick(const FGeometry&, double, float) override;
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry&, const FSlateRect&, FSlateWindowElementList&, int32,
                          const FWidgetStyle&, bool) const override;
    virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent&) override;
    virtual FReply OnKeyUp(const FGeometry&, const FKeyEvent&) override;
    virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent&) override;
    virtual FReply OnMouseMove(const FGeometry&, const FPointerEvent&) override;
    virtual void OnMouseLeave(const FPointerEvent&) override;
    virtual FReply OnMouseWheel(const FGeometry&, const FPointerEvent&) override;
    virtual FReply OnFocusReceived(const FGeometry&, const FFocusEvent&) override;
    virtual void OnFocusLost(const FFocusEvent&) override;

  private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FRatwUIInputTest;
    friend class FRatwUIRevealTest;
    friend class FRatwUILayoutTest;
    friend class FRatwUIFacingTest;
    friend class FRatwUIScentTest;
    friend class FRatwUIPaceTravelTest;
    friend class FRatwUIWeatherTest;
    friend class FRatwUIAtmosphereTest;
    friend class FRatwUIElevationTest;
    friend class FRatwUILargeCellTest;
    friend class FRatwUICalendarEconomyTest;
    friend class FRatwUIPortraitTest;
    friend class FRatwUIMotionTest;
#endif
    struct FPost
    {
        FString Id, Speaker, Text, Channel, Kind;
        int32 Color = 0;
        int32 Revealed = 0;
        double PostedAt = 0;
        bool bSystem = false;
    };
    struct FHit
    {
        FSlateRect Rect;
        FString Action, Target;
    };
    struct FEntityView
    {
        FString Id, Name, Kind, State;
        TArray<FString> Actions;
        FVector2D Position = FVector2D::ZeroVector;
        FVector2D Target = FVector2D::ZeroVector;
        double Facing = 0, TargetFacing = 0;
        FRatwMotionBuffer Motion;
        int32 Color = 0;
        bool bSelf = false, bTyping = false, bSpeaking = false, bMoving = false;
        double SpokenAt = -100;
    };
    // Perception hints are deliberately not entities: no identity, position, or actions.
    struct FScentCue
    {
        int32 Sector = 0, Strength = 1;
        bool bWindborne = false;
    };
    // Presentation only: every multiplier and the illumination come from the server.
    struct FEnvironmentView
    {
        FString Weather = TEXT("clear"), Phase = TEXT("day");
        FString LightingTone = TEXT("neutral"), LightSource = TEXT("daylight");
        double Hour = 12, Daylight = 1, Illumination = 1;
        double ArtificialLight = 0, DaylightAccess = 1, GlowStrength = 0;
        double Sight = 1, Hearing = 1, Scent = 1, Movement = 1;
    } Environment;
    struct FCellAtmosphere
    {
        FSlateRect Bounds;
        FLinearColor GlowColor = FLinearColor::Transparent, WeatherColor = FLinearColor::Transparent;
        double Darkness = 0, GlowStrength = 0, WeatherStrength = 0;
        double Feather = 0, HaloRadius = 0;
    };
    struct FWeatherMark
    {
        FVector2D Position, End;
        float Alpha = 0, Size = 0;
        bool bSnow = false, bSplash = false;
    };
    // One tiled sheet of weather art drawn across the cell: Scroll is in texture pixels along the
    // sheet's own axes, Scale maps texture pixels to screen pixels, Angle rotates the sheet.
    struct FWeatherLayer
    {
        RatwWeatherArt::EArt Art = RatwWeatherArt::EArt::Mist;
        FVector2D Scroll = FVector2D::ZeroVector;
        double Scale = 1, Angle = 0;
        FLinearColor Tint = FLinearColor::Transparent;
    };

    TFunction<void(const FString&)> Command;
    TSharedPtr<FJsonObject> Snapshot;
    TSharedPtr<FJsonObject> InspectedCharacter;
    TSharedPtr<SMultiLineEditableTextBox> Composer;
    TArray<FPost> Posts;
    TMap<FString, FEntityView> EntityViews;
    TSet<FString> MotionVisibleIds;
    double MotionClock = 0, MotionOffset = 0, LatestMotionTime = -1;
    bool bMotionClockReady = false;
    int32 CellGeneration = -1;
    void ObserveMotionTime(double ServerTime);
    void ApplyPose(FEntityView& View, const TSharedPtr<FJsonObject>& Pose, double Time);
    TArray<FScentCue> ScentCues;
    TSet<FString> SeenPosts;
    TMap<FString, FString> PendingDrafts;
    FString FailedDraft;
    int64 NextRequestId = 0;
    TSet<FKey> HeldKeys;
    mutable TArray<FHit> Hits;
    mutable FSlateRect MapRect;
    mutable FVector2D MapOrigin;
    mutable double TileSize = 23;
    FVector2D CanvasOffset, HoverPoint, MapPan;
    double CanvasScale = 1, Clock = 0, LastTyping = -100, LastTypingSent = -100, LastMove = -100, RevealFraction = 0;
    int32 CellWidth = 32, CellHeight = 24, SelectedColor = 0, TranscriptScroll = 0;
    FString CellId, CellName = TEXT("Connecting…"), SceneDescription, SelfId, Channel = TEXT("ic"),
                    Volume = TEXT("speak"), Modal, ContextTarget, ContextName, ContextKind;
    FVector2D ContextPoint;
    TArray<FString> ContextActions;
    TArray<FString> TileRows, VisibilityRows;
    TArray<float> TileHeights; // Row-major, CellWidth x CellHeight; unknown tiles read as level ground.
    mutable RatwWeatherArt::FSheets WeatherSheets;
    bool bChat = false, bWorldMap = false, bReducedMotion = false, bFlatWorld = false, bTypingSent = false;
    bool bPlainGlyphs = false; // Draw each tile's plain-ASCII fallback instead of its Unicode glyph.
    bool bFacingPreview = false, bNavigationFocus = true, bMovementPending = false;
    bool bMovementHeard = false, bOutdoors = false, bWindVariable = false;
    double WindDirection = 0, WindStrength = 0;
    double PreviewFacing = 0;
    int32 RevealSpeed = 64;
    int32 RequestedPace = -1, TravelPage = 0;
    double LastPaceRequest = -100;
    bool bTravelAtlas = false;
    int32 StoryExtra = 0;
    FString InspectedText, Toast;
    double ToastUntil = 0;

    void Send(const TSharedRef<FJsonObject>&);
    void SendAction(const FString&, const FString& = TEXT(""));
    void SendMove();
    int32 DisplayPace() const;
    void RequestPace(int32);
    bool CanTravelTo(const FString&) const;
    void CancelTravel();
    bool CanFaceAt(const FVector2D&) const;
    void UpdateFacingPreview(const FVector2D&, bool);
    void SendFacing(const FVector2D&);
    void SetChat(bool);
    void SubmitPost();
    void SetTyping(bool);
    void ShowToast(const FString&);
    TSharedPtr<FJsonObject> PortraitAppearance() const;
    double PortraitAge() const;
    void LeaveCharacter();
    FReply ComposerKey(const FGeometry&, const FKeyEvent&);
    void ComposerChanged(const FText&);
    FVector2D ToCanvas(const FGeometry&, const FVector2D&) const;
    void Activate(const FHit&);
    void DrawLocal(const FGeometry&, FSlateWindowElementList&, int32) const;
    void DrawScent(const FGeometry&, FSlateWindowElementList&, int32, const FVector2D&) const;
    FString ScentLabel() const;
    FString WindLabel() const;
    FString EnvironmentLabel() const;
    FString EnvironmentEffectsLabel() const;
    FString CalendarLabel() const;
    FString MoonLabel() const;
    int32 InventoryQuantity(const FString&) const;
    TSharedPtr<FJsonObject> TradeItem(const FString&) const;
    bool CanTradeItem(const FString&, bool Buy) const;
    TSharedPtr<FJsonObject> VisibleResource() const;
    bool CanGather() const;
    void OpenTrade(const FString&);
    FSlateRect CellBounds() const;
    FSlateRect VisibleCellBounds() const;
    FCellAtmosphere CellAtmosphere() const;
    TArray<FWeatherMark> WeatherMarks() const;
    TArray<FWeatherLayer> WeatherLayers() const;
    double LightningFlash() const;
    void DrawWeatherLayer(const FGeometry&, FSlateWindowElementList&, int32, const FSlateRect&,
                          const FWeatherLayer&) const;
    float HeightAt(int32 X, int32 Y) const;
    float SelfHeight() const;
    FString ElevationLabel() const;
    void DrawEnvironment(const FGeometry&, FSlateWindowElementList&, int32, bool Foreground) const;
    void DrawWorld(const FGeometry&, FSlateWindowElementList&, int32) const;
    void DrawTravelAtlas(const FGeometry&, FSlateWindowElementList&, int32) const;
    void DrawPace(const FGeometry&, FSlateWindowElementList&, int32) const;
    void DrawModal(const FGeometry&, FSlateWindowElementList&, int32) const;
    static FLinearColor SpeakingColor(int32);
};
