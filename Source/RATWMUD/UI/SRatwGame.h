#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Dom/JsonObject.h"

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
    virtual FReply OnMouseWheel(const FGeometry&, const FPointerEvent&) override;
    virtual void OnFocusLost(const FFocusEvent&) override;

  private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FRatwUIInputTest;
    friend class FRatwUIRevealTest;
    friend class FRatwUILayoutTest;
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
        double Facing = 0;
        int32 Color = 0;
        bool bSelf = false, bTyping = false, bSpeaking = false;
        double SpokenAt = -100;
    };

    TFunction<void(const FString&)> Command;
    TSharedPtr<FJsonObject> Snapshot;
    TSharedPtr<SMultiLineEditableTextBox> Composer;
    TArray<FPost> Posts;
    TMap<FString, FEntityView> EntityViews;
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
    bool bChat = false, bWorldMap = false, bReducedMotion = false, bFlatWorld = false, bTypingSent = false;
    int32 RevealSpeed = 64;
    int32 StoryExtra = 0;
    FString InspectedText, Toast;
    double ToastUntil = 0;

    void Send(const TSharedRef<FJsonObject>&);
    void SendAction(const FString&, const FString& = TEXT(""));
    void SendMove();
    void SetChat(bool);
    void SubmitPost();
    void SetTyping(bool);
    void ShowToast(const FString&);
    FReply ComposerKey(const FGeometry&, const FKeyEvent&);
    void ComposerChanged(const FText&);
    FVector2D ToCanvas(const FGeometry&, const FVector2D&) const;
    void Activate(const FHit&);
    void DrawLocal(const FGeometry&, FSlateWindowElementList&, int32) const;
    void DrawWorld(const FGeometry&, FSlateWindowElementList&, int32) const;
    void DrawModal(const FGeometry&, FSlateWindowElementList&, int32) const;
    static FLinearColor SpeakingColor(int32);
};
