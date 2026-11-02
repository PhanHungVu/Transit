// Copyright Epic Games, Inc. All Rights Reserved.


#include "TransitPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "TransitCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "Transit.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "TransitKioskActor.h"
#include "TransitKioskWidget.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Tests/TransitKioskSmokeTest.h"
#include "UObject/ConstructorHelpers.h"

ATransitPlayerController::ATransitPlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = ATransitCameraManager::StaticClass();
    static ConstructorHelpers::FClassFinder<UTransitKioskWidget> KioskClass(TEXT("/Game/Kiosk/UI/KioskUI"));
    static ConstructorHelpers::FClassFinder<UUserWidget> PromptClass(TEXT("/Game/Kiosk/UI/InteractPrompt"));
    static ConstructorHelpers::FObjectFinder<UInputAction> Action(TEXT("/Game/Kiosk/Input/IA_Interact"));
    KioskWidgetClass = KioskClass.Class;
    InteractionPromptClass = PromptClass.Class;
    InteractAction = Action.Object;
}

void ATransitPlayerController::BeginPlay()
{
	Super::BeginPlay();

#if WITH_DEV_AUTOMATION_TESTS
    StartTransitKioskSmokeTest(this);
#endif

	
	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogTransit, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void ATransitPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

    // IA_Interact is already mapped to E in IMC_Default; do not add another mapping.
    if (auto* Enhanced = Cast<UEnhancedInputComponent>(InputComponent))
    {
        if (InteractAction)
            Enhanced->BindAction(InteractAction, ETriggerEvent::Started, this, &ATransitPlayerController::OpenKiosk);
    }

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
	
}

bool ATransitPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void ATransitPlayerController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!IsLocalPlayerController()) return;
    if (KioskWidget && (!ActiveKiosk.IsValid() || !GetPawn())) CloseKiosk();
    PromptUpdateSeconds += DeltaSeconds;
    if (PromptUpdateSeconds >= 0.1f)
    {
        PromptUpdateSeconds = 0.f;
        UpdateKioskPrompt();
    }
}

void ATransitPlayerController::UpdateKioskPrompt()
{
    NearbyKiosk.Reset();
    if (!KioskWidget && GetPawn())
    {
        float BestDistance = TNumericLimits<float>::Max();
        for (TActorIterator<ATransitKioskActor> It(GetWorld()); It; ++It)
        {
            const float Distance = FVector::DistSquared(GetPawn()->GetActorLocation(), It->GetActorLocation());
            if (Distance < BestDistance && It->CanInteract(this)) { NearbyKiosk = *It; BestDistance = Distance; }
        }
    }
    if (NearbyKiosk.IsValid() && !InteractionPrompt)
    {
        if (InteractionPromptClass)
        {
            InteractionPrompt = CreateWidget<UUserWidget>(this, InteractionPromptClass);
            if (InteractionPrompt) { InteractionPrompt->AddToPlayerScreen(5); InteractionPrompt->SetVisibility(ESlateVisibility::HitTestInvisible); }
        }
    }
    if (InteractionPrompt) InteractionPrompt->SetVisibility(NearbyKiosk.IsValid() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void ATransitPlayerController::OpenKiosk()
{
    if (KioskWidget) return;
    UpdateKioskPrompt();
    if (!NearbyKiosk.IsValid() || !NearbyKiosk->CanInteract(this)) return;
    if (!KioskWidgetClass) { UE_LOG(LogTransit, Error, TEXT("KioskUI must derive from TransitKioskWidget. Run the TransitKiosk editor commandlet with -Apply.")); return; }
    KioskWidget = CreateWidget<UTransitKioskWidget>(this, KioskWidgetClass);
    if (!KioskWidget) return;
    ActiveKiosk = NearbyKiosk;
    if (DemoCardBalanceCents == INDEX_NONE) DemoCardBalanceCents = ActiveKiosk->DemoConfig.InitialCardBalanceCents;
    KioskWidget->InitializeKiosk(ActiveKiosk->DemoConfig, DemoCardBalanceCents, bDayPassPurchased);
    KioskWidget->AddToPlayerScreen(20);
    bSavedCursor = bShowMouseCursor; bSavedClickEvents = bEnableClickEvents; bSavedHoverEvents = bEnableMouseOverEvents;
    SetIgnoreMoveInput(true); SetIgnoreLookInput(true);
    if (auto* KioskUser = Cast<ACharacter>(GetPawn())) { KioskUser->StopJumping(); KioskUser->GetCharacterMovement()->StopMovementImmediately(); }
    bShowMouseCursor = true; bEnableClickEvents = true; bEnableMouseOverEvents = true;
    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(KioskWidget->TakeWidget());
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
    KioskWidget->SetKeyboardFocus();
    UpdateKioskPrompt();
}

void ATransitPlayerController::CloseKiosk()
{
    if (!KioskWidget) return;
    KioskWidget->ShutdownKiosk();
    KioskWidget->RemoveFromParent(); KioskWidget = nullptr; ActiveKiosk.Reset();
    SetIgnoreMoveInput(false); SetIgnoreLookInput(false);
    bShowMouseCursor = bSavedCursor; bEnableClickEvents = bSavedClickEvents; bEnableMouseOverEvents = bSavedHoverEvents;
    SetInputMode(FInputModeGameOnly());
    UpdateKioskPrompt();
}

void ATransitPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CloseKiosk();
    if (InteractionPrompt) { InteractionPrompt->RemoveFromParent(); InteractionPrompt = nullptr; }
    Super::EndPlay(EndPlayReason);
}
