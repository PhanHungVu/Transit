#include "TransitKioskActor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "TransitKioskWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
ATransitKioskActor::ATransitKioskActor()
{
	PrimaryActorTick.bCanEverTick = false;

    static ConstructorHelpers::FClassFinder<UTransitKioskWidget> UI(TEXT("/Game/Kiosk/UI/KioskUI"));
    TouchscreenClass = UI.Class;
    static ConstructorHelpers::FObjectFinder<USoundBase> Start(TEXT("/Game/Sound/Start.Start"));
    static ConstructorHelpers::FObjectFinder<USoundBase> Click(TEXT("/Game/Sound/Click.Click"));
    static ConstructorHelpers::FObjectFinder<USoundBase> Accept(TEXT("/Game/Sound/Accept_Sound.Accept_Sound"));
    static ConstructorHelpers::FObjectFinder<USoundBase> Reject(TEXT("/Game/Sound/Reject_Sound.Reject_Sound"));
    static ConstructorHelpers::FObjectFinder<USoundBase> Help(TEXT("/Game/Sound/Help.Help"));
    static ConstructorHelpers::FObjectFinder<USoundBase> Payment(TEXT("/Game/Sound/PaymentShowUp.PaymentShowUp"));
    static ConstructorHelpers::FObjectFinder<USoundBase> Pickup(TEXT("/Game/Sound/PickUp.PickUp"));
    Audio.Start=Start.Object; Audio.Click=Click.Object; Audio.Accept=Accept.Object; Audio.Reject=Reject.Object;
    Audio.Help=Help.Object; Audio.PaymentShowUp=Payment.Object; Audio.PickUp=Pickup.Object;
}

USoundBase* ATransitKioskActor::GetKioskSound(EKioskSound Cue) const
{
    switch(Cue)
    {
    case EKioskSound::Start: return Audio.Start;
    case EKioskSound::Click: return Audio.Click;
    case EKioskSound::Accept: return Audio.Accept;
    case EKioskSound::Reject: return Audio.Reject;
    case EKioskSound::Help: return Audio.Help;
    case EKioskSound::PaymentShowUp: return Audio.PaymentShowUp;
    case EKioskSound::PickUp: return Audio.PickUp;
    default: return nullptr;
    }
}
void ATransitKioskActor::PlayKioskSound(EKioskSound Cue)
{
    auto* Sound=GetKioskSound(Cue);
    if (!bAudioEnabled || AudioVolume<=0 || !Sound || !GetWorld() || !GetWorld()->IsGameWorld()) return;
    ++SoundEventCounts.FindOrAdd(Cue);
    // UI cues remain clear as the player looks around the nearby physical machine.
    const float Gain=FMath::Clamp(AudioVolume,0.f,1.f)*(Cue==EKioskSound::Click ? .5f : 1.f);
    UGameplayStatics::PlaySound2D(this,Sound,Gain,1.f,0.f,nullptr,this,true);
    UE_LOG(LogTemp,Verbose,TEXT("KIOSK_SFX %s %s"),*StaticEnum<EKioskSound>()->GetNameStringByValue(static_cast<int64>(Cue)),*Sound->GetPathName());
}

bool ATransitKioskActor::CanInteract(const APlayerController* Player) const
{
    if (!Player || !Player->GetPawn()) return false;
    FVector Center, Extent;
    GetActorBounds(false, Center, Extent);
    if (FVector::DistSquared(Player->GetPawn()->GetActorLocation(), Center) > FMath::Square(InteractionDistance)) return false;
    FVector Eye; FRotator View;
    Player->GetPlayerViewPoint(Eye, View);
    if (GetActorTransform().InverseTransformPosition(Eye).X < 25.f) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(KioskVisibility), false, Player->GetPawn());
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, Center, ECC_Visibility, Query) || Hit.GetActor() == this;
}

UStaticMeshComponent* ATransitKioskActor::FindPart(FName Name) const
{
    TArray<UStaticMeshComponent*> Meshes;
    GetComponents(Meshes);
    for (auto* Mesh : Meshes)
        if (Mesh->ComponentHasTag(Name) || Mesh->GetFName().GetPlainNameString() == Name.ToString()) return Mesh;
    return nullptr;
}

void ATransitKioskActor::BeginPlay()
{
    Super::BeginPlay();
    if (auto* CancelKey = FindPart(TEXT("PaymentKey_3_0")))
    {
        CancelKey->ComponentTags.AddUnique(TEXT("CancelPayment"));
        CancelKey->SetCollisionResponseToAllChannels(ECR_Ignore);
        CancelKey->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        CancelKey->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    }
    PrepareInteractionHighlights();
    PrepareTouchscreen(GetWorld()->GetFirstPlayerController());
    UpdatePresentation();
}

UTransitKioskWidget* ATransitKioskActor::PrepareTouchscreen(APlayerController* Player)
{
    auto* Display = FindPart(TEXT("ScreenDisplay"));
    if (!Display || !TouchscreenClass) return nullptr;
    if (!Touchscreen)
    {
        Touchscreen = NewObject<UWidgetComponent>(this, TEXT("KioskTouchscreen"));
        Touchscreen->SetupAttachment(Display);
        Touchscreen->SetWidgetSpace(EWidgetSpace::World);
        Touchscreen->SetDrawSize(FVector2D(960,700));
        Touchscreen->SetPivot(FVector2D(.5,.5));
        Touchscreen->SetTwoSided(false);
        Touchscreen->SetBlendMode(EWidgetBlendMode::Opaque);
        Touchscreen->SetWindowFocusable(false);
        Touchscreen->SetTickWhenOffscreen(true);
        Touchscreen->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Touchscreen->SetCollisionResponseToAllChannels(ECR_Ignore);
        Touchscreen->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
        AddInstanceComponent(Touchscreen);
        Touchscreen->RegisterComponent();
    }
    FVector Low, High;
    Display->GetLocalBounds(Low,High);
    const auto Transform = Display->GetComponentTransform();
    const FVector Position = Transform.TransformPosition(FVector(High.X,0,0)) + Display->GetForwardVector()*.15f;
    const FVector Scale = Display->GetComponentScale();
    Touchscreen->SetWorldTransform(FTransform(Display->GetComponentQuat(), Position,
        FVector(.05, (High.Y-Low.Y)*Scale.Y/960, (High.Z-Low.Z)*Scale.Z/700)));
    if (!TouchscreenWidget)
    {
        TouchscreenWidget = Player ? CreateWidget<UTransitKioskWidget>(Player,TouchscreenClass)
                                   : CreateWidget<UTransitKioskWidget>(GetWorld(),TouchscreenClass);
        if (!TouchscreenWidget) return nullptr;
        TouchscreenWidget->SetPhysicalKiosk(this);
        TouchscreenWidget->SetWorldSpaceMode();
        TouchscreenWidget->InitializeKiosk(DemoConfig,DemoConfig.InitialCardBalanceCents);
        Touchscreen->SetWidget(TouchscreenWidget);
        Touchscreen->SetMaterial(0,TouchscreenMaterial ? TouchscreenMaterial.Get() : Touchscreen->GetMaterial(0));
    }
    if (Player) TouchscreenWidget->SetOwningPlayer(Player);
    return TouchscreenWidget;
}

void ATransitKioskActor::ShowIdleScreen(int32 Balance)
{
    if (TouchscreenWidget && !HasPhysicalItems())
    {
        TouchscreenWidget->InitializeKiosk(DemoConfig,Balance);
        TouchscreenWidget->Refresh();
    }
    else if (TouchscreenWidget) TouchscreenWidget->Refresh();
    UpdatePresentation();
}

bool ATransitKioskActor::HasPhysicalItems() const
{
    const auto* S = TouchscreenWidget ? TouchscreenWidget->GetSession() : nullptr;
    return S && (S->bCardPresent || S->bTicketAwaitingCollection);
}
bool ATransitKioskActor::HasPendingCollection() const
{
    return TouchscreenWidget && TouchscreenWidget->GetSession() && TouchscreenWidget->GetSession()->HasPendingCollection();
}

bool ATransitKioskActor::HasUncollectedTicket() const
{
    return TouchscreenWidget && TouchscreenWidget->GetSession() && TouchscreenWidget->GetSession()->bTicketAwaitingCollection;
}

bool ATransitKioskActor::CollectTicket()
{
    if (!HasUncollectedTicket() || !TouchscreenWidget->GetSession()->CollectTicket()) return false;
    TouchscreenWidget->PhysicalItemCollected();
    PlayKioskSound(EKioskSound::PickUp);
    return true;
}

bool ATransitKioskActor::CollectCard()
{
    if (!TouchscreenWidget || !TouchscreenWidget->GetSession() || !TouchscreenWidget->GetSession()->CollectCard()) return false;
    TouchscreenWidget->PhysicalItemCollected();
    PlayKioskSound(EKioskSound::PickUp);
    return true;
}
void ATransitKioskActor::PrepareInteractionHighlights()
{
    if (!InteractionGlowMaterial) return;
    ActionGlow=UMaterialInstanceDynamic::Create(InteractionGlowMaterial,this);
    PickupGlow=UMaterialInstanceDynamic::Create(InteractionGlowMaterial,this);
    HelpGlow=UMaterialInstanceDynamic::Create(InteractionGlowMaterial,this);
    ActionGlow->SetVectorParameterValue(TEXT("GlowColor"),FLinearColor(.02f,.65f,1.f));
    PickupGlow->SetVectorParameterValue(TEXT("GlowColor"),FLinearColor(1.f,.65f,.015f));
    HelpGlow->SetVectorParameterValue(TEXT("GlowColor"),FLinearColor(.015f,1.f,.08f));
    if (auto* Bezel=FindPart(TEXT("HelpMount"))) OriginalHelpBezel=Bezel->GetMaterial(0);
    if (auto* Face=FindPart(TEXT("HelpButton"))) HelpButtonRest=Face->GetRelativeTransform();
    if (auto* Display=FindPart(TEXT("PaymentDisplay"))) OriginalPaymentScreen=Display->GetMaterial(0);
    TArray<UTextRenderComponent*> Labels;
    GetComponents(Labels);
    for (auto* Label : Labels) if (Label->GetFName().GetPlainNameString()==TEXT("PaymentDisplayLabel")) OriginalPaymentText=Label->TextRenderColor;

    for (FName Part : {TEXT("PaymentReader"),TEXT("TransitCardReader"),TEXT("TicketCollection")})
    {
        auto* Target=FindPart(Part);
        if (!Target || !Target->GetStaticMesh()) continue;
        FVector Low,High;
        Target->GetLocalBounds(Low,High);
        const auto Transform=Target->GetComponentTransform();
        const FVector Half=(High-Low)*.5*Transform.GetScale3D().GetAbs();
        const FVector Center=Transform.TransformPosition((Low+High)*.5);
        int32 Normal=Half.X<Half.Y ? 0 : 1;
        if (Half.Z<Half[Normal]) Normal=2;
        const int32 A=(Normal+1)%3, B=(Normal+2)%3;
        for (int32 Edge=0;Edge<4;++Edge)
        {
            const int32 Across=Edge<2 ? A : B, Along=Edge<2 ? B : A;
            FVector Offset=FVector::ZeroVector, Size(.3,.3,.3);
            Offset[Normal]=Half[Normal]+(Part==TEXT("TicketCollection") ? 7.2 : .12);
            Offset[Across]=(Edge%2 ? 1 : -1)*(Half[Across]+.08);
            Size[Along]=Half[Along]*2+.46;
            auto* Outline=NewObject<UStaticMeshComponent>(this,*FString::Printf(TEXT("Cue_%s_%d"),*Part.ToString(),Edge));
            Outline->SetupAttachment(Target);
            Outline->SetStaticMesh(Target->GetStaticMesh());
            Outline->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Outline->SetCastShadow(false);
            Outline->SetMaterial(0,ActionGlow);
            Outline->SetVisibility(false);
            AddInstanceComponent(Outline);
            Outline->RegisterComponent();
            Outline->SetWorldTransform(FTransform(Transform.GetRotation(),Center+Transform.GetRotation().RotateVector(Offset),Size/100.f));
            InteractionOutlines.FindOrAdd(Part).Add(Outline);
        }
    }
}
void ATransitKioskActor::SetInteractionHighlight(FName Part,bool bVisible,bool bPickup)
{
    if (const auto* Edges=InteractionOutlines.Find(Part))
        for (const auto& Edge : *Edges) if (Edge.IsValid())
        {
            Edge->SetVisibility(bVisible);
            Edge->SetHiddenInGame(!bVisible);
            auto* Material=bPickup ? PickupGlow.Get() : ActionGlow.Get();
            if (Edge->GetMaterial(0)!=Material) Edge->SetMaterial(0,Material);
        }
}
bool ATransitKioskActor::IsInteractionHighlighted(FName Part) const
{
    const auto* Edges=InteractionOutlines.Find(Part);
    if (!Edges || Edges->Num()!=4) return false;
    for (const auto& Edge : *Edges) if (!Edge.IsValid() || !Edge->IsVisible() || Edge->bHiddenInGame) return false;
    return true;
}
void ATransitKioskActor::UpdatePresentation()
{
    const auto* S = TouchscreenWidget ? TouchscreenWidget->GetSession() : nullptr;
    const bool bCardPresent = S && S->bCardPresent;
    const bool bCardPickup = S && S->bCardAwaitingCollection;
    const bool bShowOutput = HasUncollectedTicket();
    for (const auto& Part : {TPair<FName,bool>(TEXT("TransitCardProp"),bCardPresent),
                            TPair<FName,bool>(TEXT("CollectionItem"),bShowOutput)})
        if (auto* Mesh = FindPart(Part.Key))
        {
            Mesh->SetVisibility(Part.Value);
            Mesh->SetHiddenInGame(!Part.Value);
            if (Part.Key == TEXT("CollectionItem") || Part.Key == TEXT("TransitCardProp"))
            {
                Mesh->SetCollisionEnabled(Part.Value ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
                Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
                Mesh->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
            }
        }
    const bool bPay = S && !S->HasPendingCollection() && S->Popup==EKioskPopup::Payment && S->CanPay();
    if (ActionGlow && bPay!=bPaymentLightOn)
    {
        if (auto* Display=FindPart(TEXT("PaymentDisplay"))) Display->SetMaterial(0,bPay ? ActionGlow.Get() : OriginalPaymentScreen.Get());
        TArray<UTextRenderComponent*> Labels;
        GetComponents(Labels);
        for (auto* Label : Labels) if (Label->GetFName().GetPlainNameString()==TEXT("PaymentDisplayLabel"))
            Label->SetTextRenderColor(bPay ? FColor(5,16,30) : OriginalPaymentText);
    }
    bPaymentLightOn=bPay && ActionGlow!=nullptr;
    SetInteractionHighlight(TEXT("PaymentReader"),bPay);
    SetInteractionHighlight(TEXT("TransitCardReader"),bCardPickup || (S && !S->HasPendingCollection() && !bCardPresent && S->Page==EKioskPage::CardPlacement && S->Popup==EKioskPopup::None),bCardPickup);
    SetInteractionHighlight(TEXT("TicketCollection"),bShowOutput,true);
    UpdateTicketStack(bShowOutput && S ? S->TicketQuantity : 0);
    UpdateHelpFeedback();
}

void ATransitKioskActor::PressHelpButton()
{
    if (!GetWorld()) return;
    HelpPressedAt=GetWorld()->GetTimeSeconds();
    GetWorld()->GetTimerManager().SetTimer(HelpFeedbackTimer,this,&ATransitKioskActor::UpdateHelpFeedback,.016f,true);
    UpdateHelpFeedback();
}
void ATransitKioskActor::UpdateHelpFeedback()
{
    if (!GetWorld() || !HelpGlow) return;
    const double Elapsed=GetWorld()->GetTimeSeconds()-HelpPressedAt;
    const auto* S=TouchscreenWidget ? TouchscreenWidget->GetSession() : nullptr;
    const bool bLight=(S && S->Popup==EKioskPopup::Assistance) || Elapsed<.8;
    float Push=0;
    if (Elapsed>=0 && Elapsed<.10) Push=FMath::SmoothStep(0.f,1.f,static_cast<float>(Elapsed/.10));
    else if (Elapsed<.28) Push=1;
    else if (Elapsed<.48) Push=1-FMath::SmoothStep(0.f,1.f,static_cast<float>((Elapsed-.28)/.20));
    HelpDepression=Push*.65f;
    if (auto* Face=FindPart(TEXT("HelpButton")))
        Face->SetRelativeLocation(HelpButtonRest.GetLocation()-FVector(HelpDepression,0,0));
    if (HelpGlow && bLight!=bHelpRingLit)
        if (auto* Bezel=FindPart(TEXT("HelpMount"))) Bezel->SetMaterial(0,bLight ? HelpGlow.Get() : OriginalHelpBezel.Get());
    bHelpRingLit=bLight && HelpGlow!=nullptr;
    if (Elapsed>=.8) GetWorld()->GetTimerManager().ClearTimer(HelpFeedbackTimer);
}
void ATransitKioskActor::UpdateTicketStack(int32 Count)
{
    auto* Base=FindPart(TEXT("CollectionItem"));
    if (!Base) return;
    const int32 Extra=FMath::Max(0,Count-1);
    while (ExtraTickets.Num()<Extra)
    {
        auto* Ticket=NewObject<UStaticMeshComponent>(this,*FString::Printf(TEXT("PrintedTicket_%d"),ExtraTickets.Num()+1));
        Ticket->SetupAttachment(Base);
        Ticket->SetStaticMesh(Base->GetStaticMesh());
        Ticket->SetMaterial(0,Base->GetMaterial(0));
        Ticket->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Ticket->SetCastShadow(false);
        AddInstanceComponent(Ticket);
        Ticket->RegisterComponent();
        ExtraTickets.Add(Ticket);
    }
    const auto Transform=Base->GetComponentTransform();
    for (int32 I=0;I<ExtraTickets.Num();++I)
    {
        ExtraTickets[I]->SetWorldTransform(FTransform(Transform.GetRotation(),Transform.GetLocation()+Transform.GetRotation().RotateVector(FVector(0,(I+1)*.10,(I+1)*.14)),Transform.GetScale3D()));
        ExtraTickets[I]->SetVisibility(I<Extra);
        ExtraTickets[I]->SetHiddenInGame(I>=Extra);
    }
}
int32 ATransitKioskActor::GetDisplayedTicketCount() const
{
    const auto* Base=FindPart(TEXT("CollectionItem"));
    int32 Count=Base && Base->IsVisible() && !Base->bHiddenInGame ? 1 : 0;
    for (const auto& Ticket : ExtraTickets) if (Ticket && Ticket->IsVisible() && !Ticket->bHiddenInGame) ++Count;
    return Count;
}

void ATransitKioskActor::GetInteractionView(float EyeHeight, FVector& Location, FRotator& Rotation) const
{
    float Front = 22.5f;
    if (auto* Body = FindPart(TEXT("MachineBase")))
    {
        FVector Low,High;
        Body->GetLocalBounds(Low,High);
        Front = GetActorTransform().InverseTransformPosition(Body->GetComponentTransform().TransformPosition(FVector(High.X,0,0))).X;
    }
    Location = GetActorTransform().TransformPosition(FVector(Front+StandingDistance,12,EyeHeight));
    const auto* Display = FindPart(TEXT("ScreenDisplay"));
    const FVector Target = Display ? Display->GetComponentLocation() : GetActorTransform().TransformPosition(FVector(30,20,105));
    Rotation = (Target-Location).Rotation();
}

void ATransitKioskActor::EndPlay(const EEndPlayReason::Type Reason)
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(HelpFeedbackTimer);
    if (TouchscreenWidget) TouchscreenWidget->ShutdownKiosk();
    Super::EndPlay(Reason);
}

