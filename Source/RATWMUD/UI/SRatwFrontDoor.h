#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Widgets/SCompoundWidget.h"

class SBox;
class SEditableTextBox;

/** Account lobby only. Credentials are never written to disk or client logs. */
class SRatwFrontDoor : public SCompoundWidget
{
  public:
    SLATE_BEGIN_ARGS(SRatwFrontDoor) {}
    SLATE_ARGUMENT(TFunction<void(const FString&)>, OnCommand)
    SLATE_END_ARGS()
    void Construct(const FArguments&);
    void ReceiveEvent(const TSharedPtr<FJsonObject>&);
    void SetPresentationPage(const FString&);
    void SetCharacterDraft(const FString& Name, int32 Age, TSharedPtr<FJsonObject> Appearance);
    FString GetPresentationPage() const
    {
        return Page;
    }
    virtual bool SupportsKeyboardFocus() const override
    {
        return true;
    }
    virtual void Tick(const FGeometry&, double, float) override;
    virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent&) override;

  private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FRatwUIFrontDoorTest;
#endif
    TFunction<void(const FString&)> Command;
    TSharedPtr<SBox> Body;
    TSharedPtr<SEditableTextBox> Username, Password;
    TArray<TSharedPtr<FJsonObject>> Characters;
    TMap<FString, TArray<TSharedPtr<FString>>> Choices;
    TSharedPtr<FJsonObject> DraftAppearance;
    FString Stage = TEXT("login"), Page = TEXT("login"), Message, SelectedId, DraftName;
    FString CreationRequestId, CreationFingerprint;
    int32 DraftAge = 18;
    bool bBusy = false, bError = false;
    double SentAt = 0;
    void Show(const FString&);
    void StartCreation();
    void Send(const TSharedRef<FJsonObject>&);
    void SubmitAuth();
    void Review();
    void CreateCharacter();
    void EnterCharacter();
    void SetMessage(const FString&, bool Error = false);
    TSharedPtr<FJsonObject> SelectedCharacter() const;
    TSharedRef<SWidget> LoginPage();
    TSharedRef<SWidget> RosterPage();
    TSharedRef<SWidget> CreatorPage(bool ReviewOnly);
    TSharedRef<SWidget> Choice(const FString& Field, const FString& Label);
    TSharedRef<SWidget> Palette(const FString& Field, const FString& Label);
    TSharedRef<SWidget> Amount(const FString& Field, const FString& Label);
    TSharedRef<SWidget> Button(const FString&, TFunction<void()>, bool Primary = false);
};
