#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "TransitPlayerController.h"
#include "TransitKioskActor.h"
#include "TransitKioskText.h"
#include "TransitKioskWidget.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "InputKeyEventArgs.h"
#include "Containers/Ticker.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"

/** Exercises real camera rays and Slate pointer events, rather than broadcasting button delegates. */
struct FTransitKioskPOVCheck
{
    struct FStep { TFunction<void()> Run; float Delay; };
    TWeakObjectPtr<ATransitPlayerController> Player;
    TWeakObjectPtr<ATransitKioskActor> Kiosk;
    TArray<FStep> Steps;
    int32 Index = 0, Failures = 0;
    double NextTime = 0;
    FString Report;
    FVector SavedPawn, BeforeMove, HelpRestFace;
    float NaturalEye = 0;
    bool SavedCursor = false;

    void Check(bool Pass, const FString& Name)
    {
        Report += (Pass ? TEXT("PASS ") : TEXT("FAIL "))+Name+TEXT("\n");
        if (!Pass) ++Failures;
        if (!Pass && Name == TEXT("E opens the existing kiosk")) Index = Steps.Num();
        UE_LOG(LogTemp,Display,TEXT("KIOSK_POV %s %s"),Pass ? TEXT("PASS") : TEXT("FAIL"),*Name);
    }
    void Add(TFunction<void()> Run, float Delay = .18f) { Steps.Add({MoveTemp(Run),Delay}); }
    void Wait(float Seconds) { Add([]{},Seconds); }
    void Assert(const FString& Name, TFunction<bool()> Predicate)
    { Add([this,Name,Predicate=MoveTemp(Predicate)]{ Check(Predicate(),Name); }); }
    UTransitKioskSession* Session() const { return Player->GetKioskWidget() ? Player->GetKioskWidget()->GetSession() : nullptr; }
    void Key(FKey Value, EInputEvent Event, double Amount = 1)
    {
        FInputKeyEventArgs Args;
        Args.Key = Value; Args.Event = Event; Args.AmountDepressed = Amount;
        Player->InputKey(Args);
    }
    void Aim(const FVector& Target)
    {
        auto* Camera = Player->KioskCamera.Get();
        if (!Camera) { Check(false,TEXT("Aim has active camera")); return; }
        const auto Goal = (Target-Camera->GetActorLocation()).Rotation();
        const auto Current = Camera->GetActorRotation();
        Key(EKeys::MouseX,IE_Axis,FMath::FindDeltaAngleDegrees(Current.Yaw,Goal.Yaw)/Player->KioskLookSensitivity);
        Key(EKeys::MouseY,IE_Axis,FMath::FindDeltaAngleDegrees(Current.Pitch,Goal.Pitch)/Player->KioskLookSensitivity);
    }
    FVector HardwarePoint(FName Name) const
    {
        auto* Mesh = Kiosk->FindPart(Name);
        if (!Mesh) return Kiosk->GetActorLocation();
        if (Name == TEXT("HelpButton")) return Mesh->GetComponentLocation()+Kiosk->GetActorForwardVector()*1.2;
        FVector Low,High; Mesh->GetLocalBounds(Low,High);
        return Mesh->GetComponentTransform().TransformPosition(FVector(High.X,0,0));
    }
    void AimButton(FName Name)
    {
        auto* W = Player->GetKioskWidget();
        auto* B = W ? Cast<UButton>(W->GetWidgetFromName(Name)) : nullptr;
        if (!B || !Kiosk->GetTouchscreen()) { Check(false,TEXT("Button exists ")+Name.ToString()); return; }
        const auto Geometry = B->GetCachedGeometry();
        const FVector2D Pixel = W->GetCachedGeometry().AbsoluteToLocal(Geometry.LocalToAbsolute(Geometry.GetLocalSize()*.5));
        Check(Geometry.GetLocalSize().X > 1 && Geometry.GetLocalSize().Y > 1,TEXT("Visible layout ")+Name.ToString());
        UE_LOG(LogTemp,Display,TEXT("KIOSK_POV_AIM %s pixel=%s root=%s"),*Name.ToString(),*Pixel.ToString(),*W->GetCachedGeometry().GetLocalSize().ToString());
        Aim(Kiosk->GetTouchscreen()->GetComponentTransform().TransformPosition(FVector(0,480-Pixel.X,350-Pixel.Y)));
    }
    void Click(FName Name)
    {
        auto BeforeClick=MakeShared<int32>(0);
        Add([this,Name]{ AimButton(Name); });
        Add([this,Name,BeforeClick]
        {
            *BeforeClick=Kiosk->GetSoundEventCount(EKioskSound::Click);
            Player->UpdateKioskInteraction();
            Player->KioskPointer->TickComponent(0,LEVELTICK_All,nullptr);
            Check(Player->KioskPointer->GetHoveredWidgetComponent() == Kiosk->GetTouchscreen() &&
                  Player->KioskPointer->IsOverInteractableWidget(),TEXT("Real screen ray ")+Name.ToString());
            Key(EKeys::LeftMouseButton,IE_Pressed);
        },.06f);
        Add([this,Name,BeforeClick]
        {
            Key(EKeys::LeftMouseButton,IE_Released,0);
            Check(Kiosk->GetSoundEventCount(EKioskSound::Click)==*BeforeClick+1,TEXT("One click SFX ")+Name.ToString());
        });
    }
    void Hardware(FName Name)
    {
        Add([this,Name]{ Aim(HardwarePoint(Name)); });
        Add([this,Name]
        {
            Player->UpdateKioskInteraction();
            Check(!Player->HardwareAction.IsNone(),TEXT("Physical trace ")+Name.ToString());
            Key(EKeys::LeftMouseButton,IE_Pressed);
        },.06f);
        Add([this]{ Key(EKeys::LeftMouseButton,IE_Released,0); });
    }
    void Picture(const FString& Name)
    {
        // A newly visible mesh material may queue a shader permutation after initial startup.
        // Complete that work, then allow a frame before requesting the visual evidence.
        Add([]{ if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation(); },.15f);
        Add([this,Name]
        {
            if (!FParse::Param(FCommandLine::Get(),TEXT("nullrhi")))
            {
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/Name,true,false);
                if (Name == TEXT("KioskPOV_Standing.png"))
                {
                    auto* Surface = Kiosk->GetTouchscreen();
                    auto* Target = Surface->GetRenderTarget();
                    auto* Material = Surface->GetMaterialInstance();
                    UTexture* Bound = nullptr;
                    if (Material) Material->GetTextureParameterValue(FMaterialParameterInfo(TEXT("SlateUI")),Bound);
                    Check(Kiosk->TouchscreenMaterial && Material && Material->GetMaterial()==Kiosk->TouchscreenMaterial->GetMaterial(),
                        TEXT("Exposure-compensated world-screen material active"));
                    UE_LOG(LogTemp,Display,TEXT("KIOSK_SURFACE target=%s material=%s base=%s bound=%s"),
                        *GetPathNameSafe(Target),*GetPathNameSafe(Material),Material ? *GetPathNameSafe(Material->GetMaterial()) : TEXT("none"),*GetPathNameSafe(Bound));
                    TArray<FColor> Pixels;
                    if (Target && Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels))
                    {
                        TArray64<uint8> PNG;
                        FImageUtils::PNGCompressImageArray(Target->SizeX,Target->SizeY,Pixels,PNG);
                        FFileHelper::SaveArrayToFile(PNG,*(FPaths::ProjectSavedDir()/TEXT("KioskPOVScreenRender.png")));
                        TSet<uint32> Colors;
                        for (int32 I=0; I<Pixels.Num(); I+=17) Colors.Add(Pixels[I].DWColor());
                        Check(Colors.Num()>64,TEXT("World render target contains the real UI artwork"));
                    }
                    else Check(false,TEXT("World screen render target is readable"));
                }
            }
        },.4f);
    }
    void Open()
    {
        Add([this]
        {
            FVector Eye; FRotator View; Player->GetPlayerViewPoint(Eye,View);
            NaturalEye = Kiosk->GetActorTransform().InverseTransformPosition(Eye).Z;
            Report+=FString::Printf(TEXT("NATURAL eye=%.2f cm\n"),NaturalEye);
            SavedCursor = Player->bShowMouseCursor;
            Key(EKeys::E,IE_Pressed);
        },.06f);
        Add([this]{ Key(EKeys::E,IE_Released,0); },.5f);
        Assert(TEXT("E opens the existing kiosk"),[this]{return Player->IsKioskOpen();});
        Assert(TEXT("Kiosk startup sound was requested"),[this]{return Kiosk->GetSoundEventCount(EKioskSound::Start)>0;});
    }
    void Exit()
    {
        Add([this]{ Key(EKeys::Escape,IE_Pressed); },.5f);
        Assert(TEXT("Escape restores movement, look, cursor and camera"),[this]
        { return !Player->IsKioskOpen() && !Player->IsMoveInputIgnored() && !Player->IsLookInputIgnored() &&
                 Player->bShowMouseCursor == SavedCursor && Player->GetViewTarget() == Player->GetPawn() && !Player->GetPawn()->IsHidden(); });
    }
    void StartLanguage(FName Language)
    {
        Click(TEXT("BTN_Start")); Click(Language);
        Assert(TEXT("Language selection reaches options"),[this]{return Session() && Session()->Page==EKioskPage::MainMenu;});
    }
    void PayAndWait()
    {
        Hardware(TEXT("PaymentReader"));
        Assert(TEXT("Terminal starts existing payment simulation"),[this]{return Session() && Session()->Popup==EKioskPopup::Processing;});
        Wait(2.4f);
        Assert(TEXT("World timer confirms payment"),[this]{return Session() && Session()->Popup==EKioskPopup::Success;});
    }
    void CancelTopUpByPickingUpCard(const FString& Stage, bool bTakeScreenshot = false)
    {
        Add([this] { Aim(HardwarePoint(TEXT("TransitCardProp"))); });
        Add([this, Stage]
        {
            Player->UpdateKioskInteraction();
            Check(Player->HardwareAction == TEXT("CollectCard"), Stage + TEXT(": the physical card offers pickup"));
            const int32 Rejects = Kiosk->GetSoundEventCount(EKioskSound::Reject);
            const int32 Pickups = Kiosk->GetSoundEventCount(EKioskSound::PickUp);
            const int32 Balance = Session()->CardBalanceCents;
            Key(EKeys::LeftMouseButton, IE_Pressed);
            Check(Session()->Popup == EKioskPopup::TopUpCancelled && !Session()->bCardPresent &&
                  !Session()->HasPendingCollection() && !Kiosk->FindPart(TEXT("TransitCardProp"))->IsVisible(),
                  Stage + TEXT(": pickup cancels and removes the card immediately"));
            Check(Session()->CardBalanceCents == Balance && Session()->GetTotalCents() == 0 &&
                  Session()->ReceiptId.IsEmpty() && !Kiosk->IsPaymentScreenLit(),
                  Stage + TEXT(": the bill is cleared without a charge"));
            Check(Kiosk->GetSoundEventCount(EKioskSound::Reject) == Rejects + 1 &&
                  Kiosk->GetSoundEventCount(EKioskSound::PickUp) == Pickups + 1,
                  Stage + TEXT(": one Reject and one PickUp sound"));
        }, .06f);
        Add([this] { Key(EKeys::LeftMouseButton, IE_Released, 0); });
        Assert(Stage + TEXT(": cancellation notice replaces the active screen"), [this]
        {
            const auto* Widget = Player->GetKioskWidget();
            const auto* Title = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("LOC_CollectTitle")));
            return Title && Title->GetText().ToString() == KioskText(Session()->Language, TEXT("TopUpCancelledTitle")) &&
                   Widget->GetWidgetFromName(TEXT("CollectionWarning"))->GetVisibility() == ESlateVisibility::Visible &&
                   Widget->GetWidgetFromName(TEXT("PopupLayer"))->GetVisibility() == ESlateVisibility::Collapsed;
        });
        Add([this]
        {
            Key(EKeys::LeftMouseButton, IE_Pressed);
            Key(EKeys::LeftMouseButton, IE_Released, 0);
            Key(EKeys::P, IE_Pressed);
            Key(EKeys::C, IE_Pressed);
            Key(EKeys::Escape, IE_Pressed);
        });
        if (bTakeScreenshot)
        {
            Add([this] { Aim(Kiosk->FindPart(TEXT("ScreenDisplay"))->GetComponentLocation()); });
            Picture(TEXT("KioskTopUpCancelled.png"));
        }
        Wait(3.f);
        Assert(Stage + TEXT(": the notice stays open and blocks payment, reinsertion and exit"), [this]
        {
            return Player->IsKioskOpen() && Session()->Popup == EKioskPopup::TopUpCancelled &&
                   !Session()->bCardPresent && Session()->CardBalanceCents == 340;
        });
        Wait(2.f);
        Assert(Stage + TEXT(": Welcome returns within the kiosk with the original balance"), [this]
        {
            return Player->IsKioskOpen() && Session()->Page == EKioskPage::Welcome &&
                   Session()->Popup == EKioskPopup::None && Session()->CardBalanceCents == 340 &&
                   Player->GetKioskWidget()->GetWidgetFromName(TEXT("CollectionWarning"))->GetVisibility() == ESlateVisibility::Collapsed;
        });
    }
    void Build()
    {
        Add([this]
        {
            auto* PC = Player.Get();
            PC->OpenKiosk(); Check(!PC->IsKioskOpen(),TEXT("Distant interaction rejected"));
            for (TActorIterator<ATransitKioskActor> It(PC->GetWorld()); It; ++It) { Kiosk=*It; break; }
            Check(Kiosk.IsValid() && PC->GetPawn(),TEXT("Original kiosk and character exist"));
            if (!Kiosk.IsValid() || !PC->GetPawn()) { Index=Steps.Num(); return; }
            SavedPawn = Kiosk->GetActorTransform().TransformPosition(FVector(180,0,100));
            PC->GetPawn()->SetActorLocation(SavedPawn,false,nullptr,ETeleportType::TeleportPhysics);
            PC->SetControlRotation((Kiosk->FindPart(TEXT("ScreenDisplay"))->GetComponentLocation()-SavedPawn).Rotation());
            if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
        },.7f);
        Open();
        Assert(TEXT("True world-space screen and hidden desktop cursor"),[this]
        { return Player->GetKioskWidget() && !Player->GetKioskWidget()->IsInViewport() && !Player->bShowMouseCursor &&
                 Kiosk->GetTouchscreen() && Kiosk->GetTouchscreen()->GetWidgetSpace()==EWidgetSpace::World &&
                 Kiosk->GetTouchscreen()->GetWidget()==Player->GetKioskWidget(); });
        Assert(TEXT("Natural approaching eye height retained"),[this]
        { return Player->KioskCamera && FMath::IsNearlyEqual(static_cast<float>(Kiosk->GetActorTransform().InverseTransformPosition(Player->KioskCamera->GetActorLocation()).Z),NaturalEye,.1f); });
        Assert(TEXT("Movement/body locked safely; kiosk FOV is 80 degrees"),[this]
        { return Player->IsMoveInputIgnored() && Player->IsLookInputIgnored() && Player->GetPawn()->IsHidden() &&
                 FMath::IsNearlyEqual(Player->KioskCamera->GetCameraComponent()->FieldOfView,80.f); });
        Add([this]
        {
            const auto* Surface=Kiosk->GetTouchscreen();
            const auto* Display=Kiosk->FindPart(TEXT("ScreenDisplay"));
            const auto LocalRotation=Kiosk->GetActorTransform().InverseTransformRotation(Display->GetComponentQuat()).Rotator();
            Check(FMath::IsNearlyEqual(LocalRotation.Pitch,12.f,.01f),TEXT("Whole screen assembly tilted 12 degrees"));
            Check(Surface->GetComponentQuat().Equals(Display->GetComponentQuat(),.0001),TEXT("Widget follows tilted display rotation"));
            Check(Surface->GetDrawSize()==FVector2D(960,700),TEXT("Original touchscreen aspect and resolution retained"));
            Check(Kiosk->FindPart(TEXT("TransitCardReader"))->GetRelativeLocation().Z>=80 && Kiosk->FindPart(TEXT("TicketCollection"))->GetRelativeLocation().Z>=85,
                TEXT("Reader and collection opening raised to accessible band"));
            const auto* Payment=Kiosk->FindPart(TEXT("PaymentReader"));
            const auto* Body=Kiosk->FindPart(TEXT("MachineBase"));
            Check(FMath::Abs(Payment->GetRelativeLocation().Y)+Payment->GetRelativeScale3D().Y*50 <= Body->GetRelativeScale3D().Y*50,
                TEXT("Payment unit fits inside continuous cabinet width"));
        });
        Picture(TEXT("KioskPOV_Standing.png"));
        Add([this]{ Key(EKeys::MouseX,IE_Axis,10000); });
        Assert(TEXT("Free look clamps right at 45 degrees"),[this]{return FMath::IsNearlyEqual(Player->KioskYaw,45.f);});
        Add([this]{ Key(EKeys::MouseX,IE_Axis,-20000); });
        Assert(TEXT("Free look clamps left at 45 degrees"),[this]{return FMath::IsNearlyEqual(Player->KioskYaw,-45.f);});
        Click(TEXT("BTN_Start"));
        Assert(TEXT("World pointer activates Welcome"),[this]{return Session() && Session()->Page==EKioskPage::Language;});
        if (FParse::Param(FCommandLine::Get(),TEXT("KioskPOVProbe"))) { Exit(); return; }

        Click(TEXT("BTN_LanguageEnglish"));
        Click(TEXT("BTN_Purchase")); Click(TEXT("BTN_Destination")); Click(TEXT("BTN_Station6"));
        Click(TEXT("BTN_Confirm")); Click(TEXT("BTN_RoundTrip"));
        Assert(TEXT("Quantity opens before card payment"),[this]{return Session()->Popup==EKioskPopup::Quantity && Session()->TicketQuantity==1 && !Kiosk->IsPaymentScreenLit();});
        Assert(TEXT("Minus disabled at the lower bound"),[this]{return !Player->GetKioskWidget()->GetWidgetFromName(TEXT("BTN_QuantityMinus"))->GetIsEnabled();});
        Click(TEXT("BTN_QuantityPlus")); Click(TEXT("BTN_QuantityPlus"));
        Assert(TEXT("Three passengers multiply round-trip fare to SGD 18.00"),[this]{return Session()->TicketQuantity==3 && Session()->GetTotalCents()==1800;});
        Click(TEXT("BTN_QuantityMinus"));
        Assert(TEXT("Minus updates count and price"),[this]{return Session()->TicketQuantity==2 && Session()->GetTotalCents()==1200;});
        Click(TEXT("BTN_QuantityPlus"));
        for(const FString Language : {TEXT("English"),TEXT("Chinese"),TEXT("Malay"),TEXT("Tamil")})
        {
            Add([this,Language]{Session()->Language=Language;Player->GetKioskWidget()->Refresh();});
            Picture(TEXT("KioskFeedback_Quantity_")+Language+TEXT(".png"));
        }
        Add([this]{Session()->Language=TEXT("English");Player->GetKioskWidget()->Refresh();HelpRestFace=Kiosk->FindPart(TEXT("HelpButton"))->GetRelativeLocation();Aim(HardwarePoint(TEXT("HelpButton")));});
        Assert(TEXT("Help face has no phone or bell logo"),[this]{return !Kiosk->FindPart(TEXT("HelpIcon0")) && !Kiosk->FindPart(TEXT("HelpEarpiece")) && !Kiosk->FindPart(TEXT("HelpMouthpiece"));});
        Picture(TEXT("KioskFeedback_HelpRest.png"));
        Hardware(TEXT("HelpButton"));
        Assert(TEXT("Help press lights green ring and physically depresses face"),[this]
        {
            const bool Passed=Session()->Popup==EKioskPopup::Assistance && Kiosk->IsHelpRingLit() && Kiosk->FindPart(TEXT("HelpButton"))->GetRelativeLocation().X<HelpRestFace.X-.5;
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("KioskFeedback_HelpPressed.png"),true,false);
            return Passed;
        });
        Wait(.45f);
        Assert(TEXT("Help face springs back while green ring acknowledges assistance"),[this]{return Kiosk->IsHelpRingLit() && Kiosk->FindPart(TEXT("HelpButton"))->GetRelativeLocation().Equals(HelpRestFace,.01);});
        Click(TEXT("BTN_StaffClose")); Wait(.85f);
        Assert(TEXT("Help ring clears after assistance and preserves quantity"),[this]{return !Kiosk->IsHelpRingLit() && Session()->Popup==EKioskPopup::Quantity && Session()->TicketQuantity==3;});
        Click(TEXT("BTN_QuantityConfirm"));
        Assert(TEXT("Card payment keeps three tickets and SGD 18.00 total"),[this]{return Session()->Popup==EKioskPopup::Payment && Session()->TicketQuantity==3 && Session()->GetTotalCents()==1800;});
        Add([this]
        {
            const int32 Before=Kiosk->GetSoundEventCount(EKioskSound::PaymentShowUp);
            Player->GetKioskWidget()->Refresh(); Player->GetKioskWidget()->Refresh();
            Check(Before>0 && Kiosk->GetSoundEventCount(EKioskSound::PaymentShowUp)==Before,TEXT("Payment popup sound fires on appearance, not Refresh"));
        });
        Assert(TEXT("Payment display and terminal outline light only when ready"),[this]{return Kiosk->IsPaymentScreenLit() && Kiosk->IsInteractionHighlighted(TEXT("PaymentReader")) && !Kiosk->IsInteractionHighlighted(TEXT("TransitCardReader"));});
        Add([this]{Aim(HardwarePoint(TEXT("PaymentReader")));});
        Picture(TEXT("KioskInteraction_PaymentReady.png"));
        PayAndWait();
        Assert(TEXT("Ticket and gold outline appear with immediate reminder"),[this]{return Session()->bTicketAwaitingCollection && Kiosk->FindPart(TEXT("CollectionItem"))->IsVisible() && Kiosk->IsInteractionHighlighted(TEXT("TicketCollection")) && Player->GetKioskWidget()->GetWidgetFromName(TEXT("CollectionWarning"))->GetVisibility()==ESlateVisibility::Visible;});
        Assert(TEXT("Three tickets are physically dispensed as a stack"),[this]{return Kiosk->GetDisplayedTicketCount()==3 && !Kiosk->IsInteractionHighlighted(TEXT("CollectionItem"));});
        Assert(TEXT("Payment light turns off after payment"),[this]{return !Kiosk->IsPaymentScreenLit() && !Kiosk->IsInteractionHighlighted(TEXT("PaymentReader"));});
        Add([this]{Player->GetKioskWidget()->HandleAction(TEXT("Home"));Key(EKeys::Escape,IE_Pressed);Key(EKeys::P,IE_Pressed);Key(EKeys::C,IE_Pressed);Key(EKeys::F1,IE_Pressed);});
        Assert(TEXT("All controls including Help and Escape locked during ticket pickup"),[this]{return Player->IsKioskOpen() && Session()->bTicketAwaitingCollection && Session()->Popup==EKioskPopup::Success && Session()->CardBalanceCents==340;});
        Hardware(TEXT("HelpButton"));
        Assert(TEXT("Physical Help cannot bypass the pickup reminder"),[this]{return Session()->Popup==EKioskPopup::Success && Session()->bTicketAwaitingCollection;});
        for(const FString Language : {TEXT("English"),TEXT("Chinese"),TEXT("Malay"),TEXT("Tamil")})
        {
            // Presentation fixtures do not enable a user language-change action while locked.
            Add([this,Language]{Session()->Language=Language;Player->GetKioskWidget()->Refresh();Aim(Kiosk->FindPart(TEXT("ScreenDisplay"))->GetComponentLocation());});
            Picture(TEXT("KioskInteraction_Ticket_")+Language+TEXT(".png"));
        }
        Add([this]{Aim(HardwarePoint(TEXT("CollectionItem")));});
        Picture(TEXT("KioskInteraction_TicketOutline.png"));
        Add([this]{Session()->Language=TEXT("English");Player->GetKioskWidget()->Refresh();Player->CloseKioskInternal(true);},.5f);
        Open();
        Assert(TEXT("Forced teardown/reopen preserves ticket and outline"),[this]{return Session()->bTicketAwaitingCollection && Kiosk->IsInteractionHighlighted(TEXT("TicketCollection"));});
        Hardware(TEXT("CollectionItem"));
        Assert(TEXT("Only physical ticket click removes ticket, outline and reminder"),[this]{return !Session()->HasPendingCollection() && !Kiosk->FindPart(TEXT("CollectionItem"))->IsVisible() && !Kiosk->IsInteractionHighlighted(TEXT("TicketCollection")) && Kiosk->FindPart(TEXT("CollectionItem"))->GetCollisionEnabled()==ECollisionEnabled::NoCollision && Player->GetKioskWidget()->GetWidgetFromName(TEXT("CollectionWarning"))->GetVisibility()==ESlateVisibility::Collapsed;});
        Wait(.4f);
        Assert(TEXT("Collected ticket does not respawn on Success"),[this]{return !Kiosk->FindPart(TEXT("CollectionItem"))->IsVisible() && Kiosk->GetDisplayedTicketCount()==0;});
        Hardware(TEXT("HelpButton"));
        Assert(TEXT("Help restored after ticket pickup"),[this]{return Session()->Popup==EKioskPopup::Assistance;});
        Click(TEXT("BTN_StaffClose")); Click(TEXT("BTN_SuccessHome"));
        StartLanguage(TEXT("BTN_LanguageEnglish")); Click(TEXT("BTN_Purchase")); Click(TEXT("BTN_DayPass"));
        Click(TEXT("BTN_QuantityPlus")); Click(TEXT("BTN_QuantityConfirm")); PayAndWait();
        Assert(TEXT("Two day passes cost SGD 20.00 and dispense two tickets"),[this]{return Session()->GetTotalCents()==2000 && Kiosk->GetDisplayedTicketCount()==2;});
        Hardware(TEXT("TicketCollection")); Exit(); Open();
        StartLanguage(TEXT("BTN_LanguageChinese")); Click(TEXT("BTN_Purchase"));
        Assert(TEXT("Next customer can buy day passes after exit/reopen"),[this]{return Player->GetKioskWidget()->GetWidgetFromName(TEXT("BTN_DayPass"))->GetIsEnabled();});
        Click(TEXT("BTN_DayPass"));
        Assert(TEXT("Next customer quantity resets to one"),[this]{return Session()->Popup==EKioskPopup::Quantity && Session()->TicketQuantity==1;});
        Click(TEXT("BTN_QuantityConfirm")); PayAndWait(); Hardware(TEXT("TicketCollection")); Click(TEXT("BTN_SuccessHome"));
        StartLanguage(TEXT("BTN_LanguageChinese")); Click(TEXT("BTN_TopUp"));
        Assert(TEXT("Transit reader outlined when card placement is required"),[this]{return Kiosk->IsInteractionHighlighted(TEXT("TransitCardReader")) && !Kiosk->IsPaymentScreenLit();});
        Add([this]{Aim(HardwarePoint(TEXT("TransitCardReader")));});
        Picture(TEXT("KioskInteraction_PlaceCard.png"));
        Hardware(TEXT("TransitCardReader"));
        Assert(TEXT("Card stays physically in holder throughout top-up selection"),[this]{return Session()->bCardPresent && !Session()->bCardAwaitingCollection && Kiosk->FindPart(TEXT("TransitCardProp"))->IsVisible() && !Kiosk->IsInteractionHighlighted(TEXT("TransitCardReader"));});
        Click(TEXT("BTN_Back"));
        Assert(TEXT("Leaving amount selection immediately requires unpaid card pickup"),[this]{return Session()->bCardAwaitingCollection && Kiosk->IsInteractionHighlighted(TEXT("TransitCardReader")) && Session()->CardBalanceCents==340;});
        Add([this]{Key(EKeys::Escape,IE_Pressed);});
        Assert(TEXT("Escape cannot abandon unpaid card"),[this]{return Player->IsKioskOpen() && Session()->bCardPresent;});
        Hardware(TEXT("TransitCardProp"));
        Assert(TEXT("Unpaid physical card pickup restores placement page"),[this]{return !Session()->bCardPresent && Session()->Page==EKioskPage::CardPlacement && Session()->CardBalanceCents==340;});
        Exit(); Open();
        StartLanguage(TEXT("BTN_LanguageEnglish")); Click(TEXT("BTN_TopUp")); Hardware(TEXT("TransitCardReader"));
        CancelTopUpByPickingUpCard(TEXT("Before amount selection"), true);
        StartLanguage(TEXT("BTN_LanguageTamil")); Click(TEXT("BTN_TopUp")); Hardware(TEXT("TransitCardReader"));
        Click(TEXT("BTN_TopUp0"));
        CancelTopUpByPickingUpCard(TEXT("After amount selection, before payment"));
        StartLanguage(TEXT("BTN_LanguageTamil")); Click(TEXT("BTN_TopUp")); Hardware(TEXT("TransitCardReader"));
        Click(TEXT("BTN_TopUp5"));
        Assert(TEXT("Top-up payment terminal lights up"),[this]{return Kiosk->IsPaymentScreenLit();});
        Hardware(TEXT("PaymentReader"));
        Hardware(TEXT("TransitCardProp"));
        Assert(TEXT("Card click during payment processing keeps the card and the payment in progress"), [this]
        {
            return Session()->Popup == EKioskPopup::Processing && Session()->bCardPresent &&
                   Kiosk->FindPart(TEXT("TransitCardProp"))->IsVisible();
        });
        Hardware(TEXT("HelpButton")); Wait(2.4f);
        Assert(TEXT("Approved top-up interrupts Help with immediate physical card reminder"),[this]{return Session()->Popup==EKioskPopup::Success && Session()->CardBalanceCents==10340 && Session()->bCardPresent && Session()->bCardAwaitingCollection && Kiosk->IsInteractionHighlighted(TEXT("TransitCardReader")) && !Kiosk->IsPaymentScreenLit();});
        for(const FString Language : {TEXT("English"),TEXT("Chinese"),TEXT("Malay"),TEXT("Tamil")})
        {
            Add([this,Language]{Session()->Language=Language;Player->GetKioskWidget()->Refresh();Aim(Kiosk->FindPart(TEXT("ScreenDisplay"))->GetComponentLocation());});
            Picture(TEXT("KioskInteraction_Card_")+Language+TEXT(".png"));
        }
        Add([this]{Aim(HardwarePoint(TEXT("TransitCardProp")));});
        Picture(TEXT("KioskInteraction_CardOutline.png"));
        Add([this]{Player->GetKioskWidget()->HandleAction(TEXT("Home"));Key(EKeys::F1,IE_Pressed);Key(EKeys::Escape,IE_Pressed);});
        Assert(TEXT("Card reminder blocks Return, Help and Escape"),[this]{return Player->IsKioskOpen() && Session()->Popup==EKioskPopup::Success && Session()->bCardPresent;});
        Add([this]{Player->CloseKioskInternal(true);},.5f);
        Open();
        Assert(TEXT("Forced teardown/reopen preserves card and approved balance"),[this]{return Session()->bCardAwaitingCollection && Session()->bCardPresent && Session()->CardBalanceCents==10340 && Kiosk->IsInteractionHighlighted(TEXT("TransitCardReader"));});
        Hardware(TEXT("TransitCardReader"));
        Assert(TEXT("Highlighted holder click collects card and clears collision, outline and reminder"),[this]{return !Session()->HasPendingCollection() && !Session()->bCardPresent && !Kiosk->FindPart(TEXT("TransitCardProp"))->IsVisible() && !Kiosk->IsInteractionHighlighted(TEXT("TransitCardReader")) && Kiosk->FindPart(TEXT("TransitCardProp"))->GetCollisionEnabled()==ECollisionEnabled::NoCollision && Player->GetKioskWidget()->GetWidgetFromName(TEXT("CollectionWarning"))->GetVisibility()==ESlateVisibility::Collapsed;});
        Picture(TEXT("KioskPOV_TamilSuccess.png")); Click(TEXT("BTN_SuccessHome"));
        Assert(TEXT("Completion clears physical props after actual pickups"),[this]{return !Kiosk->FindPart(TEXT("TransitCardProp"))->IsVisible() && !Kiosk->FindPart(TEXT("CollectionItem"))->IsVisible();});
        StartLanguage(TEXT("BTN_LanguageEnglish")); Click(TEXT("BTN_TopUp")); Hardware(TEXT("TransitCardReader"));
        Click(TEXT("BTN_TopUp0")); Hardware(TEXT("PaymentReader"));
        Add([this]{Key(EKeys::Escape,IE_Pressed);});
        Wait(2.4f);
        Assert(TEXT("Cancel during processing keeps card and prevents late credit"),[this]{return Player->IsKioskOpen() && Session()->bCardAwaitingCollection && Session()->CardBalanceCents==10340;});
        Hardware(TEXT("TransitCardProp")); Exit(); Open();
        Assert(TEXT("Cancelled balance persists after physical pickup and exit"),[this]{return Session()->CardBalanceCents==10340;});
        StartLanguage(TEXT("BTN_LanguageMalay")); Click(TEXT("BTN_Purchase")); Click(TEXT("BTN_Destination"));
        Click(TEXT("BTN_Station0")); Click(TEXT("BTN_Confirm")); Click(TEXT("BTN_OneWay")); Click(TEXT("BTN_QuantityConfirm")); PayAndWait();
        Assert(TEXT("Malay one-way ticket retains SGD 2.00 fare"),[this]{return Session() && Session()->GetTotalCents()==200;});
        Hardware(TEXT("CollectionItem")); Click(TEXT("BTN_SuccessHome"));

        const float Eyes[] = {110,135,165,180,105};
        const TCHAR* Names[] = {TEXT("Child120"),TEXT("Short145"),TEXT("Adult175"),TEXT("Tall190"),TEXT("Seated")};
        for (int32 I=0; I<5; ++I)
        {
            const float Height=Eyes[I]; const FString Name=Names[I];
            Add([this,Height,Name]
            {
                Player->KioskEyeHeight=Height;
                FVector Position; FRotator Rotation; Kiosk->GetInteractionView(Height,Position,Rotation);
                const auto Local = Kiosk->GetActorTransform().InverseTransformRotation(Rotation.Quaternion()).Rotator();
                Player->KioskYaw=FMath::FindDeltaAngleDegrees(180.f,Local.Yaw); Player->KioskPitch=Local.Pitch;
                Player->UpdateKioskCamera();
                Report+=FString::Printf(TEXT("PROFILE %s eye=%.0f cm\n"),*Name,Height);
            },.35f);
            Assert(TEXT("Eye height preserved: ")+Name,[this,Height]{return FMath::IsNearlyEqual(static_cast<float>(Kiosk->GetActorTransform().InverseTransformPosition(Player->KioskCamera->GetActorLocation()).Z),Height,.1f);});
            Assert(TEXT("Camera clear of geometry: ")+Name,[this]
            {
                FCollisionQueryParams Q(SCENE_QUERY_STAT(KioskCameraClearance),false,Player->GetPawn());
                return !Player->GetWorld()->OverlapBlockingTestByChannel(Player->KioskCamera->GetActorLocation(),FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(8),Q);
            });
            Picture(TEXT("KioskPOV_")+Name+TEXT(".png"));
            Click(TEXT("BTN_Start"));
            Assert(TEXT("Touchscreen works at ")+Name,[this]{return Session() && Session()->Page==EKioskPage::Language;});
            Click(TEXT("BTN_Back"));
            Hardware(TEXT("HelpButton")); Click(TEXT("BTN_StaffClose"));
            for (FName Part : {TEXT("PaymentReader"),TEXT("TransitCardReader"),TEXT("TicketCollection"),TEXT("IdentificationPanel")})
            {
                Add([this,Part]{Aim(HardwarePoint(Part));});
                Assert(TEXT("Visible/reachable ray ")+Part.ToString()+TEXT(" at ")+Name,[this,Part]
                {
                    FVector Eye; FRotator View; Player->GetPlayerViewPoint(Eye,View);
                    FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(KioskHeight),false,Player->GetPawn());
                    Player->GetWorld()->LineTraceSingleByChannel(Hit,Eye,Eye+View.Vector()*250,ECC_Visibility,Q);
                    const FString HitName = Hit.GetComponent() ? Hit.GetComponent()->GetFName().GetPlainNameString() : TEXT("");
                    const bool bPart = Part==TEXT("PaymentReader") ? HitName.StartsWith(TEXT("Payment")) :
                        Part==TEXT("TransitCardReader") ? HitName.StartsWith(TEXT("TransitCardReader")) || HitName.StartsWith(TEXT("CardReader")) :
                        Part==TEXT("TicketCollection") ? HitName.StartsWith(TEXT("Collection")) || HitName.StartsWith(TEXT("TicketCollection")) :
                        HitName.StartsWith(Part.ToString());
                    UE_LOG(LogTemp,Display,TEXT("KIOSK_HEIGHT target=%s hit=%s"),*Part.ToString(),*HitName);
                    return Hit.GetActor()==Kiosk.Get() && bPart && FMath::Abs(Player->KioskYaw)<=45 && Player->KioskPitch>=-75 && Player->KioskPitch<=60;
                });
            }
        }
        Exit();
        Add([this]
        {
            Check(!Player->KioskPointer->IsActive() && !Player->KioskAim,TEXT("Pointer and HUD removed on exit"));
            Check(!Player->ReturningCamera,TEXT("Temporary camera removed after blend"));
            for (int32 I=0;I<7;++I)
            {
                const auto Cue=static_cast<EKioskSound>(I);
                Check(Kiosk->GetKioskSound(Cue) && Kiosk->GetSoundEventCount(Cue)>0,TEXT("Configured SFX triggered by real interaction: ")+StaticEnum<EKioskSound>()->GetNameStringByValue(I));
                Report+=FString::Printf(TEXT("SFX %s count=%d\n"),*StaticEnum<EKioskSound>()->GetNameStringByValue(I),Kiosk->GetSoundEventCount(Cue));
            }
        });
    }
    bool Tick(float)
    {
        if (!Player.IsValid()) return false;
        const double Now=Player->GetWorld()->GetTimeSeconds();
        if (Now<NextTime) return true;
        if (Index>=Steps.Num())
        {
            Report+=FString::Printf(TEXT("Failures: %d\n"),Failures);
            FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("KioskPOVVerification.txt")));
            FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("KioskGameplayVerification.txt")));
            FPlatformMisc::RequestExitWithStatus(false,Failures ? 1 : 0);
            return false;
        }
        auto& Step=Steps[Index++];
        Step.Run(); NextTime=Now+Step.Delay;
        return true;
    }
};

void StartTransitKioskPOVTest(ATransitPlayerController* Player)
{
    auto Test=MakeShared<FTransitKioskPOVCheck>();
    Test->Player=Player; Test->Build(); Test->NextTime=Player->GetWorld()->GetTimeSeconds()+.8;
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Test](float Delta){return Test->Tick(Delta);}));
}
#endif
