#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TransitKioskSession.h"
#include "TransitKioskActor.generated.h"

class UStaticMeshComponent;
class UWidgetComponent;
class UTransitKioskWidget;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USoundBase;

UENUM(BlueprintType)
enum class EKioskSound : uint8 { Start, Click, Accept, Reject, Help, PaymentShowUp, PickUp };

USTRUCT(BlueprintType)
struct TRANSIT_API FTransitKioskAudio
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") TObjectPtr<USoundBase> Start;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") TObjectPtr<USoundBase> Click;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") TObjectPtr<USoundBase> Accept;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") TObjectPtr<USoundBase> Reject;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") TObjectPtr<USoundBase> Help;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") TObjectPtr<USoundBase> PaymentShowUp;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") TObjectPtr<USoundBase> PickUp;
};

UCLASS()
class TRANSIT_API ATransitKioskActor : public AActor
{
	GENERATED_BODY()
	
public:	
	ATransitKioskActor();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Kiosk")
    FTransitKioskConfig DemoConfig;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Kiosk", meta=(ClampMin="50", ClampMax="1000", Units="cm"))
    float InteractionDistance = 250.f;
    bool CanInteract(const APlayerController* Player) const;
    UStaticMeshComponent* FindPart(FName Name) const;
    UTransitKioskWidget* PrepareTouchscreen(APlayerController* Player);
    UWidgetComponent* GetTouchscreen() const { return Touchscreen; }
    void ShowIdleScreen(int32 BalanceCents);
    void UpdatePresentation();
    bool HasUncollectedTicket() const;
    bool HasPhysicalItems() const;
    bool HasPendingCollection() const;
    bool CollectTicket();
    bool CollectCard();
    bool IsInteractionHighlighted(FName Part) const;
    bool IsPaymentScreenLit() const { return bPaymentLightOn; }
    void PressHelpButton();
    bool IsHelpRingLit() const { return bHelpRingLit; }
    float GetHelpDepression() const { return HelpDepression; }
    int32 GetDisplayedTicketCount() const;
    void PlayKioskSound(EKioskSound Cue);
    USoundBase* GetKioskSound(EKioskSound Cue) const;
    int32 GetSoundEventCount(EKioskSound Cue) const { return SoundEventCounts.FindRef(Cue); }
    UPROPERTY(EditAnywhere, Category="Kiosk|Audio") FTransitKioskAudio Audio;
    UPROPERTY(EditAnywhere, Category="Kiosk|Audio",meta=(ClampMin="0",ClampMax="1")) float AudioVolume=.75f;
    UPROPERTY(EditAnywhere, Category="Kiosk|Audio") bool bAudioEnabled=true;
    void GetInteractionView(float EyeHeight, FVector& Location, FRotator& Rotation) const;
    UPROPERTY(EditAnywhere, Category="Kiosk|Camera", meta=(ClampMin="50", ClampMax="100", Units="cm"))
    float StandingDistance = 75.f;
    UPROPERTY(EditAnywhere, Category="Kiosk|Camera", meta=(ClampMin="65", ClampMax="90"))
    float InteractionFOV = 80.f;
    UPROPERTY(EditAnywhere, Category="Kiosk|Camera", meta=(ClampMin="0.1", ClampMax="1"))
    float CameraBlendSeconds = .33f;
    UPROPERTY(EditDefaultsOnly, Category="Kiosk|UI")
    TObjectPtr<UMaterialInterface> TouchscreenMaterial;
    UPROPERTY(EditDefaultsOnly, Category="Kiosk|UI")
    TObjectPtr<UMaterialInterface> InteractionGlowMaterial;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(EditDefaultsOnly, Category="Kiosk|UI")
    TSubclassOf<UTransitKioskWidget> TouchscreenClass;
    UPROPERTY(Transient, VisibleInstanceOnly, Category="Kiosk|UI")
    TObjectPtr<UWidgetComponent> Touchscreen;
    UPROPERTY(Transient) TObjectPtr<UTransitKioskWidget> TouchscreenWidget;
private:
    void PrepareInteractionHighlights();
    void SetInteractionHighlight(FName Part, bool bVisible, bool bPickup = false);
    void UpdateHelpFeedback();
    void UpdateTicketStack(int32 Count);
    TMap<FName,TArray<TWeakObjectPtr<UStaticMeshComponent>>> InteractionOutlines;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ActionGlow;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> PickupGlow;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> HelpGlow;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> OriginalHelpBezel;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> ExtraTickets;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> OriginalPaymentScreen;
    FColor OriginalPaymentText = FColor::White;
    bool bPaymentLightOn = false;
    bool bHelpRingLit = false;
    FTransform HelpButtonRest;
    double HelpPressedAt = -1000;
    float HelpDepression = 0;
    FTimerHandle HelpFeedbackTimer;
    TMap<EKioskSound,int32> SoundEventCounts;
};
