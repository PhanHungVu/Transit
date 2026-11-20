#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TransitKioskSession.h"
#include "TransitKioskWidget.generated.h"
class UButton;
class UTextBlock;
class UTransitKioskWidget;
class ATransitKioskActor;
enum class EKioskSound : uint8;
UCLASS()
class UTransitKioskButtonBinding : public UObject
{
    GENERATED_BODY()
  public:
    UPROPERTY() TObjectPtr<UButton> Button;
    UPROPERTY() TObjectPtr<UTransitKioskWidget> Owner;
    FName Action;
    UFUNCTION() void Click();
};

/** Behavior for the existing KioskUI; its design remains editable in UMG. */
UCLASS()
class TRANSIT_API UTransitKioskWidget : public UUserWidget
{
    GENERATED_BODY()
  public:
    void InitializeKiosk(const FTransitKioskConfig &Config, int32 Balance);
    void ShutdownKiosk();
    void HandleAction(FName Action);
    void Refresh();
    void SetWorldSpaceMode();
    void ShowCollectionWarning();
    void ResumeKiosk();
    void PhysicalItemCollected();
    void RequireCardCollection();
    void SetPhysicalKiosk(ATransitKioskActor* Kiosk);
    UTransitKioskSession *GetSession() const
    {
        return Session;
    }
    void PlaceProjectedControl(FName Name, const FVector2D &PixelPosition, const FVector2D &PixelSize,
                               float ViewportScale);

  protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry &, const FKeyEvent &) override;

  private:
    void BindButton(FName Name, FName Action);
    void UnbindButtons();
    void SetText(FName Name, const FString &Value);
    void FinishPayment();
    void FinishTopUpCancellation();
    void PlayFeedback(EKioskSound Cue);
    UPROPERTY(Transient) TObjectPtr<UTransitKioskSession> Session;
    UPROPERTY(Transient) TArray<TObjectPtr<UTransitKioskButtonBinding>> ButtonBindings;
    TMap<FName, FSlateFontInfo> OriginalFonts;
    FTimerHandle PaymentTimer;
    FTimerHandle TopUpCancellationTimer;
    bool bShuttingDown = false;
    bool bWorldSpace = false;
    TWeakObjectPtr<ATransitKioskActor> PhysicalKiosk;
    EKioskPopup LastPresentedPopup=EKioskPopup::None;
};

/** A small noninteractive aim indicator; the kiosk itself is rendered in the world. */
UCLASS()
class TRANSIT_API UTransitKioskAimWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetAimHint(const FString& Hint, bool bReady);
protected:
    virtual void NativeOnInitialized() override;
private:
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Reticle;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> HintText;
};

UCLASS()
class TRANSIT_API UTransitHeightPromptWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void UpdatePrompt(bool bChildViewEnabled);
protected:
    virtual void NativeOnInitialized() override;
private:
    UPROPERTY(Transient) TObjectPtr<UTextBlock> PromptText;
};
