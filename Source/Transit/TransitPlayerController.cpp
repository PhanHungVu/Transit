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
#include "TransitKioskText.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Tests/TransitKioskSmokeTest.h"
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "InputKeyEventArgs.h"
#include "TimerManager.h"
#include "GameFramework/PlayerInput.h"

ATransitPlayerController::ATransitPlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = ATransitCameraManager::StaticClass();
    KioskPointer = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("KioskPointer"));
    KioskPointer->SetupAttachment(GetRootComponent());
    KioskPointer->InteractionSource = EWidgetInteractionSource::Custom;
    KioskPointer->InteractionDistance = 250;
    KioskPointer->SetAutoActivate(false);
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
    if (KioskWidget && (!ActiveKiosk.IsValid() || !GetPawn())) CloseKioskInternal(true);
    if (KioskWidget)
    {
        UpdateKioskCamera();
        UpdateKioskInteraction();
    }
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
    KioskWidget = NearbyKiosk->PrepareTouchscreen(this);
    if (!KioskWidget) return;
    ActiveKiosk = NearbyKiosk;
    if (DemoCardBalanceCents == INDEX_NONE) DemoCardBalanceCents = ActiveKiosk->DemoConfig.InitialCardBalanceCents;
    if (KioskWidget->GetSession() && (KioskWidget->GetSession()->HasPendingCollection() || KioskWidget->GetSession()->bCardPresent))
        KioskWidget->ResumeKiosk();
    else
        KioskWidget->InitializeKiosk(ActiveKiosk->DemoConfig, DemoCardBalanceCents);
    KioskWidget->Refresh();
    FVector Eye;
    FRotator OldView;
    GetPlayerViewPoint(Eye,OldView);
    const auto NormalView = ReturningCamera && PreviousViewTarget.IsValid() ? PreviousViewTarget : TWeakObjectPtr<AActor>(GetViewTarget());
    FinishCameraReturn();
    PreviousViewTarget = NormalView;
    KioskUserPawn = GetPawn();
    if (KioskUserPawn.IsValid())
    {
        bSavedPawnHidden = KioskUserPawn->IsHidden();
        KioskUserPawn->SetActorHiddenInGame(true);
    }
    KioskEyeHeight = FMath::Max(60.f, static_cast<float>(ActiveKiosk->GetActorTransform().InverseTransformPosition(Eye).Z));
    FVector CameraPosition;
    FRotator CameraRotation;
    ActiveKiosk->GetInteractionView(KioskEyeHeight,CameraPosition,CameraRotation);
    const auto LocalView = ActiveKiosk->GetActorTransform().InverseTransformRotation(CameraRotation.Quaternion()).Rotator();
    KioskYaw = FMath::FindDeltaAngleDegrees(180.f,LocalView.Yaw);
    KioskPitch = LocalView.Pitch;
    KioskCamera = GetWorld()->SpawnActor<ACameraActor>(CameraPosition,CameraRotation);
    if (KioskCamera)
    {
        KioskCamera->GetCameraComponent()->SetFieldOfView(ActiveKiosk->InteractionFOV);
        KioskCamera->GetCameraComponent()->SetConstraintAspectRatio(false);
        SetViewTargetWithBlend(KioskCamera,ActiveKiosk->CameraBlendSeconds,VTBlend_Cubic,0,true);
    }
    bSavedCursor = bShowMouseCursor; bSavedClickEvents = bEnableClickEvents; bSavedHoverEvents = bEnableMouseOverEvents;
    SetIgnoreMoveInput(true); SetIgnoreLookInput(true);
    if (auto* KioskUser = Cast<ACharacter>(GetPawn())) { KioskUser->StopJumping(); KioskUser->GetCharacterMovement()->StopMovementImmediately(); }
    bShowMouseCursor = false; bEnableClickEvents = false; bEnableMouseOverEvents = false;
    FInputModeGameOnly Mode;
    Mode.SetConsumeCaptureMouseDown(false);
    SetInputMode(Mode);
    KioskPointer->Activate();
    if (PlayerInput) PlayerInput->FlushPressedKeys();
    KioskAim = CreateWidget<UTransitKioskAimWidget>(this);
    if (KioskAim) KioskAim->AddToPlayerScreen(20);
    ActiveKiosk->PlayKioskSound(EKioskSound::Start);
    UpdateKioskPrompt();
}

void ATransitPlayerController::CloseKiosk()
{
    CloseKioskInternal(false);
}

void ATransitPlayerController::CloseKioskInternal(bool bForce)
{
    if (!KioskWidget) return;
    const auto* Session = KioskWidget->GetSession();
    if (!bForce && Session)
    {
        if (Session->Popup == EKioskPopup::TopUpCancelled) return;
        if (Session->HasPendingCollection())
        {
            if (ActiveKiosk.IsValid()) ActiveKiosk->PlayKioskSound(EKioskSound::Reject);
            KioskWidget->ShowCollectionWarning();
            return;
        }
        if (Session->bCardPresent)
        {
            KioskWidget->RequireCardCollection();
            return;
        }
    }
    KioskPointer->ReleasePointerKey(EKeys::LeftMouseButton);
    KioskPointer->SetCustomHitResult(FHitResult());
    KioskPointer->Deactivate();
    if (KioskAim) { KioskAim->RemoveFromParent(); KioskAim = nullptr; }
    KioskWidget->ShutdownKiosk();
    if (ActiveKiosk.IsValid()) ActiveKiosk->ShowIdleScreen(DemoCardBalanceCents);
    const float BlendSeconds = ActiveKiosk.IsValid() ? ActiveKiosk->CameraBlendSeconds : .33f;
    KioskWidget = nullptr; ActiveKiosk.Reset();
    AActor* Target = PreviousViewTarget.IsValid() ? PreviousViewTarget.Get() : GetPawn();
    if (Target) SetViewTargetWithBlend(Target,BlendSeconds,VTBlend_Cubic,0,true);
    if (KioskUserPawn.IsValid()) KioskUserPawn->SetActorHiddenInGame(bSavedPawnHidden);
    KioskUserPawn.Reset();
    ReturningCamera = KioskCamera;
    KioskCamera = nullptr;
    GetWorld()->GetTimerManager().SetTimer(CameraReturnTimer,this,&ATransitPlayerController::FinishCameraReturn,BlendSeconds+.05f,false);
    SetIgnoreMoveInput(false); SetIgnoreLookInput(false);
    bShowMouseCursor = bSavedCursor; bEnableClickEvents = bSavedClickEvents; bEnableMouseOverEvents = bSavedHoverEvents;
    SetInputMode(FInputModeGameOnly());
    if (PlayerInput) PlayerInput->FlushPressedKeys();
    UpdateKioskPrompt();
}

void ATransitPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CloseKioskInternal(true);
    FinishCameraReturn();
    if (InteractionPrompt) { InteractionPrompt->RemoveFromParent(); InteractionPrompt = nullptr; }
    Super::EndPlay(EndPlayReason);
}

void ATransitPlayerController::FinishCameraReturn()
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(CameraReturnTimer);
    if (ReturningCamera) { ReturningCamera->Destroy(); ReturningCamera = nullptr; }
    PreviousViewTarget.Reset();
}

void ATransitPlayerController::UpdateKioskCamera()
{
    if (!KioskCamera || !ActiveKiosk.IsValid()) return;
    FVector Position;
    FRotator Ignored;
    ActiveKiosk->GetInteractionView(KioskEyeHeight,Position,Ignored);
    const auto Rotation = ActiveKiosk->GetActorTransform().TransformRotation(FRotator(KioskPitch,180+KioskYaw,0).Quaternion());
    KioskCamera->SetActorLocationAndRotation(Position,Rotation);
}

void ATransitPlayerController::UpdateKioskInteraction()
{
    bPointerOnScreen = false;
    HardwareAction = NAME_None;
    const auto* Session = KioskWidget ? KioskWidget->GetSession() : nullptr;
    if (!Session || !ActiveKiosk.IsValid())
    {
        KioskPointer->SetCustomHitResult(FHitResult());
        return;
    }
    if (Session->Popup == EKioskPopup::TopUpCancelled)
    {
        KioskPointer->SetCustomHitResult(FHitResult());
        if (KioskAim) KioskAim->SetAimHint(FString(), false);
        return;
    }
    const bool bCanTakeUnpaidCard = Session->CanCancelUnpaidTopUp();
    FHitResult Hit;
    FVector Eye;
    FRotator View;
    GetPlayerViewPoint(Eye,View);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(KioskPointerTrace),false,GetPawn());
    GetWorld()->LineTraceSingleByChannel(Hit,Eye,Eye+View.Vector()*250,ECC_Visibility,Query);
    if (Hit.GetActor() == ActiveKiosk.Get())
    {
        bPointerOnScreen = Hit.GetComponent() == ActiveKiosk->GetTouchscreen();
        if (!bPointerOnScreen && Hit.GetComponent())
        {
            const FString Name = Hit.GetComponent()->GetFName().GetPlainNameString();
            if ((Name==TEXT("CollectionItem") || Name.StartsWith(TEXT("Collection")) || Name==TEXT("TicketCollection")) && ActiveKiosk->HasUncollectedTicket()) HardwareAction=TEXT("CollectTicket");
            else if (Name == TEXT("TransitCardProp"))
            {
                HardwareAction = Session->bCardAwaitingCollection || bCanTakeUnpaidCard ? TEXT("CollectCard") : TEXT("ReadCard");
            }
            else if (Name.StartsWith(TEXT("Payment"))) HardwareAction = TEXT("Pay");
            else if (Name.StartsWith(TEXT("Help"))) HardwareAction = TEXT("Help");
            else if (Name.StartsWith(TEXT("TransitCardReader")) || Name.StartsWith(TEXT("CardReader")) || Name.StartsWith(TEXT("CardHolder"))) HardwareAction = Session->bCardAwaitingCollection ? TEXT("CollectCard") : TEXT("ReadCard");
        }
    }
    const bool bCollecting = Session->HasPendingCollection();
    KioskPointer->SetCustomHitResult(bPointerOnScreen && !bCollecting ? Hit : FHitResult());
    if (KioskAim)
    {
        const bool bReady = HardwareAction == TEXT("CollectTicket") ? Session->bTicketAwaitingCollection :
            HardwareAction == TEXT("CollectCard") ? Session->bCardAwaitingCollection || bCanTakeUnpaidCard :
            bCollecting ? false :
            HardwareAction == TEXT("Help") ? Session->Popup != EKioskPopup::Assistance :
            bPointerOnScreen ? KioskPointer->IsOverInteractableWidget() :
            HardwareAction == TEXT("Pay") ? Session->Popup == EKioskPopup::Payment && Session->CanPay() :
            HardwareAction == TEXT("ReadCard") && Session->Page == EKioskPage::CardPlacement && Session->Popup == EKioskPopup::None;
        FString Hint = TEXT("Mouse: look   |   Click: interact   |   Esc: exit   |   DEMONSTRATION ONLY");
        if (bCollecting) Hint = KioskText(Session->Language,Session->bCardAwaitingCollection ? TEXT("CollectCardHint") : TEXT("CollectHint"));
        else if (bCanTakeUnpaidCard && HardwareAction == TEXT("CollectCard"))
            Hint = KioskText(Session->Language, TEXT("CancelTopUpHint"));
        else if (bReady && !HardwareAction.IsNone())
        {
            const FString ActionHint = HardwareAction == TEXT("CollectTicket") ? TEXT("collect ticket") :
                HardwareAction == TEXT("Pay") ? TEXT("tap payment card") :
                HardwareAction == TEXT("ReadCard") ? TEXT("place transit card") : TEXT("request staff assistance");
            Hint = TEXT("Click: ")+ActionHint+TEXT("   |   Esc: exit   |   DEMONSTRATION ONLY");
        }
        KioskAim->SetAimHint(Hint,bReady);
    }
}

bool ATransitPlayerController::InputKey(const FInputKeyEventArgs& Args)
{
    if (!KioskWidget) return Super::InputKey(Args);
    if (Args.Key == EKeys::MouseX || Args.Key == EKeys::MouseY)
    {
        if (Args.Key == EKeys::MouseX) KioskYaw = FMath::Clamp(KioskYaw+static_cast<float>(Args.AmountDepressed)*KioskLookSensitivity,-45.f,45.f);
        else KioskPitch = FMath::Clamp(KioskPitch+static_cast<float>(Args.AmountDepressed)*KioskLookSensitivity,-75.f,60.f);
        UpdateKioskCamera();
        return true;
    }
    if (Args.Key == EKeys::LeftMouseButton)
    {
        if (Args.Event == IE_Pressed)
        {
            UpdateKioskInteraction();
            KioskPointer->TickComponent(0,LEVELTICK_All,nullptr);
            if (HardwareAction == TEXT("CollectTicket")) ActiveKiosk->CollectTicket();
            else if (HardwareAction == TEXT("CollectCard")) ActiveKiosk->CollectCard();
            else if (KioskWidget->GetSession()->HasPendingCollection())
            {
                if (!HardwareAction.IsNone()) ActiveKiosk->PlayKioskSound(EKioskSound::Click);
                if (bPointerOnScreen || !HardwareAction.IsNone()) ActiveKiosk->PlayKioskSound(EKioskSound::Reject);
                KioskWidget->ShowCollectionWarning();
            }
            else if (bPointerOnScreen) KioskPointer->PressPointerKey(EKeys::LeftMouseButton);
            else if (!HardwareAction.IsNone()) KioskWidget->HandleAction(HardwareAction);
        }
        else if (Args.Event == IE_Released) KioskPointer->ReleasePointerKey(EKeys::LeftMouseButton);
        return true;
    }
    if (Args.Event == IE_Pressed)
    {
        if (Args.Key == EKeys::Escape) { KioskWidget->HandleAction(TEXT("Exit")); return true; }
        if (Args.Key == EKeys::C) { KioskWidget->HandleAction(TEXT("ReadCard")); return true; }
        if (Args.Key == EKeys::P) { KioskWidget->HandleAction(TEXT("Pay")); return true; }
        if (Args.Key == EKeys::F1) { KioskWidget->HandleAction(TEXT("Help")); return true; }
    }
    // Walking, jumping and unrelated actions cannot leak into gameplay while using the kiosk.
    return true;
}
