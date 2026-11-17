// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TransitPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UUserWidget;
class UTransitKioskWidget;
class ATransitKioskActor;
class ACameraActor;
class UWidgetInteractionComponent;
class UTransitKioskAimWidget;

/**
 *  Simple first person Player Controller
 *  Manages the input mapping context.
 *  Overrides the Player Camera Manager class.
 */
UCLASS(abstract, config="Game")
class TRANSIT_API ATransitPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:

	/** Constructor */
	ATransitPlayerController();

    UFUNCTION(BlueprintCallable, Category="Kiosk")
    void OpenKiosk();
    UFUNCTION(BlueprintCallable, Category="Kiosk")
    void CloseKiosk();
    bool IsKioskOpen() const { return KioskWidget != nullptr; }
    UTransitKioskWidget* GetKioskWidget() const { return KioskWidget; }
    void StoreDemoProgress(int32 BalanceCents)
    {
        DemoCardBalanceCents = BalanceCents;
    }
    virtual void Tick(float DeltaSeconds) override;
    virtual bool InputKey(const FInputKeyEventArgs& Args) override;
    UWidgetInteractionComponent* GetKioskPointer() const { return KioskPointer; }
    UPROPERTY(EditAnywhere, Category="Kiosk|Camera", meta=(ClampMin="0.01", ClampMax="1"))
    float KioskLookSensitivity = .12f;

protected:

    /** Hard asset references also keep the existing widgets/input in cooked builds. */
    UPROPERTY(EditDefaultsOnly, Category="Kiosk")
    TSubclassOf<UTransitKioskWidget> KioskWidgetClass;
    UPROPERTY(EditDefaultsOnly, Category="Kiosk")
    TSubclassOf<UUserWidget> InteractionPromptClass;
    UPROPERTY(EditDefaultsOnly, Category="Kiosk")
    TObjectPtr<UInputAction> InteractAction;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

private:
    friend struct FTransitKioskPOVCheck;
    void UpdateKioskPrompt();
    void CloseKioskInternal(bool bForce);
    void UpdateKioskCamera();
    void UpdateKioskInteraction();
    void FinishCameraReturn();
    UPROPERTY(Transient) TObjectPtr<ACameraActor> KioskCamera;
    UPROPERTY(Transient) TObjectPtr<ACameraActor> ReturningCamera;
    UPROPERTY(Transient) TObjectPtr<UWidgetInteractionComponent> KioskPointer;
    UPROPERTY(Transient) TObjectPtr<UTransitKioskAimWidget> KioskAim;
    TWeakObjectPtr<AActor> PreviousViewTarget;
    TWeakObjectPtr<APawn> KioskUserPawn;
    bool bSavedPawnHidden = false;
    UPROPERTY(Transient) TObjectPtr<UTransitKioskWidget> KioskWidget;
    UPROPERTY(Transient) TObjectPtr<UUserWidget> InteractionPrompt;
    TWeakObjectPtr<ATransitKioskActor> NearbyKiosk;
    TWeakObjectPtr<ATransitKioskActor> ActiveKiosk;
    float PromptUpdateSeconds = 0.f;
    int32 DemoCardBalanceCents = INDEX_NONE;
    bool bSavedCursor = false;
    bool bSavedClickEvents = false;
    bool bSavedHoverEvents = false;
    bool bPointerOnScreen = false;
    FName HardwareAction;
    float KioskYaw = 0, KioskPitch = 0, KioskEyeHeight = 165;
    FTimerHandle CameraReturnTimer;
};
