#include "TransitKioskWidget.h"
#include "TransitKioskText.h"
#include "TransitKioskActor.h"
#include "TransitPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/World.h"
#include "Engine/Font.h"
#include "TimerManager.h"
#include "Components/CanvasPanel.h"
#include "Styling/CoreStyle.h"

void UTransitKioskButtonBinding::Click()
{
    if (Owner)
        Owner->HandleAction(Action);
}
void UTransitKioskWidget::InitializeKiosk(const FTransitKioskConfig &Config, int32 Balance)
{
    if (auto* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PaymentTimer);
        World->GetTimerManager().ClearTimer(TopUpCancellationTimer);
    }
    Session = NewObject<UTransitKioskSession>(this);
    Session->Configure(Config, Balance);
    bShuttingDown = false;
    LastPresentedPopup=EKioskPopup::None;
}
void UTransitKioskWidget::PlayFeedback(EKioskSound Cue)
{
    if (PhysicalKiosk.IsValid()) PhysicalKiosk->PlayKioskSound(Cue);
}
void UTransitKioskWidget::SetPhysicalKiosk(ATransitKioskActor* Kiosk)
{
    PhysicalKiosk = Kiosk;
}
void UTransitKioskWidget::BindButton(FName Name, FName Action)
{
    if (auto *Button = Cast<UButton>(GetWidgetFromName(Name)))
    {
        auto *Binding = NewObject<UTransitKioskButtonBinding>(this);
        Binding->Button = Button;
        Binding->Owner = this;
        Binding->Action = Action;
        Button->OnClicked.AddDynamic(Binding, &UTransitKioskButtonBinding::Click);
        ButtonBindings.Add(Binding);
    }
    else
        UE_LOG(LogTemp, Error, TEXT("Missing kiosk button %s"), *Name.ToString());
}
void UTransitKioskWidget::UnbindButtons()
{
    for (UTransitKioskButtonBinding *Binding : ButtonBindings)
        if (Binding && Binding->Button)
            Binding->Button->OnClicked.RemoveDynamic(Binding, &UTransitKioskButtonBinding::Click);
    ButtonBindings.Reset();
}
void UTransitKioskWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(!bWorldSpace);
    if (!Session)
        InitializeKiosk(FTransitKioskConfig(), 340);
    UnbindButtons();
    const TPair<FName, FName> Buttons[] = {{TEXT("BTN_Start"), TEXT("Start")},
                                           {TEXT("BTN_LanguageEnglish"), TEXT("English")},
                                           {TEXT("BTN_LanguageChinese"), TEXT("Chinese")},
                                           {TEXT("BTN_LanguageMalay"), TEXT("Malay")},
                                           {TEXT("BTN_LanguageTamil"), TEXT("Tamil")},
                                           {TEXT("BTN_Back"), TEXT("Back")},
                                           {TEXT("BTN_Purchase"), TEXT("Purchase")},
                                           {TEXT("BTN_TopUp"), TEXT("TopUp")},
                                           {TEXT("BTN_Destination"), TEXT("Destination")},
                                           {TEXT("BTN_DayPass"), TEXT("DayPass")},
                                           {TEXT("BTN_Confirm"), TEXT("Confirm")},
                                           {TEXT("BTN_OneWay"), TEXT("OneWay")},
                                           {TEXT("BTN_RoundTrip"), TEXT("RoundTrip")},
                                           {TEXT("BTN_QuantityMinus"), TEXT("QuantityMinus")},
                                           {TEXT("BTN_QuantityPlus"), TEXT("QuantityPlus")},
                                           {TEXT("BTN_QuantityConfirm"), TEXT("QuantityConfirm")},
                                           {TEXT("BTN_QuantityCancel"), TEXT("Back")},
                                           {TEXT("BTN_TripCancel"), TEXT("Back")},
                                           {TEXT("BTN_PaymentCancel"), TEXT("CancelPayment")},
                                           {TEXT("BTN_SuccessHome"), TEXT("Home")},
                                           {TEXT("BTN_StaffClose"), TEXT("StaffClose")},
                                           {TEXT("BTN_HardwareHelp"), TEXT("Help")},
                                           {TEXT("BTN_HardwarePay"), TEXT("Pay")},
                                           {TEXT("BTN_HardwareCard"), TEXT("ReadCard")},
                                           {TEXT("BTN_ExitKiosk"), TEXT("Exit")}};
    for (const auto &Pair : Buttons)
        BindButton(Pair.Key, Pair.Value);
    for (int32 I = 0; I < 8; ++I)
        BindButton(*FString::Printf(TEXT("BTN_Station%d"), I), *FString::Printf(TEXT("Station%d"), I));
    for (int32 I = 0; I < 6; ++I)
        BindButton(*FString::Printf(TEXT("BTN_TopUp%d"), I), *FString::Printf(TEXT("Amount%d"), I));
    WidgetTree->ForEachWidget(
        [this](UWidget *Item)
        {
            if (auto *Text = Cast<UTextBlock>(Item))
                OriginalFonts.Add(Item->GetFName(), Text->GetFont());
        });
    Refresh();
}
void UTransitKioskWidget::SetText(FName Name, const FString &Value)
{
    if (auto *Text = Cast<UTextBlock>(GetWidgetFromName(Name)))
        Text->SetText(FText::FromString(Value));
}
void UTransitKioskWidget::Refresh()
{
    if (!Session)
        return;
    WidgetTree->ForEachWidget(
        [this](UWidget *Item)
        {
            if (auto *Text = Cast<UTextBlock>(Item))
            {
                FString Name = Item->GetName();
                if (Name.StartsWith(TEXT("LOC_")))
                {
                    FString Key = Name.RightChop(4);
                    int32 Split;
                    if (Key.FindChar('_', Split))
                        Key = Key.Left(Split);
                    Text->SetText(FText::FromString(KioskText(Session->Language, *Key)));
                }
                if (const auto *Font = OriginalFonts.Find(Item->GetFName()))
                {
                    auto Display = *Font;
                    if (Session->Language == TEXT("Tamil"))
                        Display.Size = FMath::Max(12.f, Display.Size * .85f);
                    Text->SetFont(Display);
                }
            }
        });
    CastChecked<UWidgetSwitcher>(GetWidgetFromName(TEXT("WS_Pages")))
        ->SetActiveWidgetIndex(static_cast<int32>(Session->Page));
    static const FName Titles[] = {
        TEXT(""),      TEXT(""),           TEXT("Options"), TEXT("TicketType"), TEXT("DestinationTitle"),
        TEXT("TopUp"), TEXT("AmountTitle")};
    SetText(TEXT("TXT_Title"), Session->Page == EKioskPage::Language
                                   ? TEXT("Language/语言/Bahasa/மொழி")
                                   : KioskText(Session->Language, Titles[static_cast<int32>(Session->Page)]));
    GetWidgetFromName(TEXT("Header"))
        ->SetVisibility(Session->Page == EKioskPage::Welcome ? ESlateVisibility::Collapsed
                                                             : ESlateVisibility::SelfHitTestInvisible);
    const bool bCollecting = Session->HasPendingCollection();
    const bool bCancelled = Session->Popup == EKioskPopup::TopUpCancelled;
    const bool bNotice = bCollecting || bCancelled;
    const bool bModal = Session->Popup != EKioskPopup::None;
    GetWidgetFromName(TEXT("PopupLayer"))
        ->SetVisibility(bModal && !bNotice ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    GetWidgetFromName(TEXT("WS_Pages"))->SetIsEnabled(!bModal && !bNotice);
    GetWidgetFromName(TEXT("BTN_Back"))->SetIsEnabled(!bModal && !bNotice);
    if (bModal && !bNotice)
        CastChecked<UWidgetSwitcher>(GetWidgetFromName(TEXT("WS_Popups")))
            ->SetActiveWidgetIndex(static_cast<int32>(Session->Popup) - 1);
    CastChecked<UButton>(GetWidgetFromName(TEXT("BTN_DayPass")))->SetIsEnabled(true);
    SetText(TEXT("TXT_DayValidity"),KioskText(Session->Language,TEXT("DayValidity")));
    SetText(TEXT("TXT_TicketQuantity"),FString::FromInt(Session->TicketQuantity));
    SetText(TEXT("TXT_QuantitySummary"),KioskText(Session->Language,TEXT("People"))+TEXT(": ")+FString::FromInt(Session->TicketQuantity)+TEXT("   •   ")+KioskText(Session->Language,TEXT("UnitFare"))+TEXT(": ")+Session->Money(Session->GetUnitPriceCents())+TEXT("\n")+KioskText(Session->Language,TEXT("Total"))+TEXT(": ")+Session->Money(Session->GetTotalCents()));
    if (auto* Minus=GetWidgetFromName(TEXT("BTN_QuantityMinus"))) Minus->SetIsEnabled(Session->TicketQuantity>1);
    if (auto* Plus=GetWidgetFromName(TEXT("BTN_QuantityPlus"))) Plus->SetIsEnabled(Session->TicketQuantity<Session->Config.MaxTicketsPerPurchase);
    SetText(TEXT("TXT_Balance"),
            KioskText(Session->Language, TEXT("Balance")) + TEXT(" ") + Session->Money(Session->CardBalanceCents));
    for (int32 I = 0; I < 6; ++I)
        SetText(*FString::Printf(TEXT("TXT_TopUp%d"), I), Session->Money(Session->Config.TopUpAmountsCents[I]));
    const bool bSelected = Session->Config.Stations.IsValidIndex(Session->Destination);
    GetWidgetFromName(TEXT("MapSelection"))
        ->SetVisibility(bSelected ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    GetWidgetFromName(TEXT("StationLabel"))
        ->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    CastChecked<UButton>(GetWidgetFromName(TEXT("BTN_Confirm")))->SetIsEnabled(Session->CanConfirmDestination());
    if (bSelected)
    {
        const FString Station = Session->StationName();
        SetText(TEXT("TXT_StationName"), Station);
        SetText(TEXT("TXT_SelectedFare"), KioskText(Session->Language, TEXT("To")) + TEXT(" ") + Station +
                                              TEXT("   •   ") + KioskText(Session->Language, TEXT("Fare")) + TEXT(" ") +
                                              Session->Money(Session->GetOneWayCents()));
        auto *LabelSlot = CastChecked<UCanvasPanelSlot>(GetWidgetFromName(TEXT("StationLabel"))->Slot);
        const FVector2D P = Session->Config.Stations[Session->Destination].MapPosition * FVector2D(768, 512);
        LabelSlot->SetPosition(FVector2D(FMath::Clamp(P.X - 90., 0., 588.), FMath::Max(0., P.Y - 65)));
    }
    for (int32 I = 0; I < 8; ++I)
    {
        auto *Button = CastChecked<UButton>(GetWidgetFromName(*FString::Printf(TEXT("BTN_Station%d"), I)));
        auto Style = Button->GetStyle();
        const bool bActive = I == Session->Destination;
        for (auto *Brush : {&Style.Normal, &Style.Hovered, &Style.Pressed})
        {
            Brush->TintColor = FLinearColor::White;
            Brush->OutlineSettings.Color = FSlateColor(bActive ? FLinearColor(1.f, .4f, 0) : FLinearColor::Black);
            Brush->OutlineSettings.Width = bActive ? 7.f : 4.f;
        }
        Style.Hovered.TintColor = FLinearColor(1.f, .9f, .25f);
        Button->SetStyle(Style);
        const FString Code = Session->Config.Stations[I].Code;
        Button->SetToolTipText(FText::FromString(
            Session->Language == TEXT("Chinese") ? Code + TEXT(" ") + KioskText(Session->Language, TEXT("Station"))
                                                 : KioskText(Session->Language, TEXT("Station")) + TEXT(" ") + Code));
    }
    SetText(TEXT("TXT_OneWayPrice"), Session->Money(Session->GetOneWayCents()));
    SetText(TEXT("TXT_RoundTripPrice"), Session->Money((Session->GetOneWayCents() * 3 + 1) / 2));
    SetText(TEXT("TXT_PaymentDescription"), Session->ProductName()+(Session->Product==EKioskProduct::TopUp ? TEXT("") : FString::Printf(TEXT(" × %d"),Session->TicketQuantity)));
    SetText(TEXT("TXT_PaymentTotal"), Session->Money(Session->GetTotalCents()));
    SetText(TEXT("TXT_Receipt"),
            Session->ReceiptId + TEXT("\n") + Session->ProductName() + (Session->Product==EKioskProduct::TopUp ? TEXT("") : FString::Printf(TEXT(" × %d"),Session->TicketQuantity)) + TEXT(" • ") +
                Session->Money(Session->GetTotalCents()) +
                ((Session->Product == EKioskProduct::OneWay || Session->Product == EKioskProduct::RoundTrip)
                     ? TEXT("\n") + Session->StationName()
                     : TEXT("")) +
                (Session->Product == EKioskProduct::TopUp ? TEXT("\n") + KioskText(Session->Language, TEXT("Balance")) +
                                                                TEXT(" ") + Session->Money(Session->CardBalanceCents)
                                                          : TEXT("")) +
                (Session->Product == EKioskProduct::DayPass ? TEXT("\n") + Session->ValidityText() : TEXT("")));
    GetWidgetFromName(TEXT("BTN_HardwarePay"))->SetIsEnabled(!bNotice && Session->Popup == EKioskPopup::Payment);
    GetWidgetFromName(TEXT("BTN_HardwareCard"))->SetIsEnabled(!bNotice && Session->Page == EKioskPage::CardPlacement && !bModal);
    GetWidgetFromName(TEXT("BTN_HardwareHelp"))->SetIsEnabled(!bNotice && Session->Popup != EKioskPopup::Assistance);
    GetWidgetFromName(TEXT("BTN_ExitKiosk"))->SetIsEnabled(!bNotice);
    SetText(TEXT("LOC_CollectTitle"),KioskText(Session->Language,Session->bCardAwaitingCollection ? TEXT("CollectCardTitle") : TEXT("CollectTitle")));
    SetText(TEXT("LOC_CollectInstruction"),KioskText(Session->Language,Session->bCardAwaitingCollection ? TEXT("CollectCardInstruction") : TEXT("CollectInstruction")));
    if (Session->bTicketAwaitingCollection && Session->TicketQuantity>1)
    {
        SetText(TEXT("LOC_CollectTitle"),KioskText(Session->Language,TEXT("CollectStackTitle")));
        SetText(TEXT("LOC_CollectInstruction"),KioskText(Session->Language,TEXT("CollectStackInstruction")).Replace(TEXT("{count}"),*FString::FromInt(Session->TicketQuantity)));
    }
    if (bCancelled)
    {
        SetText(TEXT("LOC_CollectTitle"), KioskText(Session->Language, TEXT("TopUpCancelledTitle")));
        SetText(TEXT("LOC_CollectInstruction"), KioskText(Session->Language, TEXT("TopUpCancelledMessage")));
        SetText(TEXT("LOC_CollectHelp"), KioskText(Session->Language, TEXT("TopUpCancelledReturn")));
    }
    if (auto* Warning = GetWidgetFromName(TEXT("CollectionWarning")))
        Warning->SetVisibility(bNotice ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (PhysicalKiosk.IsValid()) PhysicalKiosk->UpdatePresentation();
    const auto Presented=bNotice ? EKioskPopup::None : Session->Popup;
    if (!bShuttingDown && Presented==EKioskPopup::Payment && LastPresentedPopup!=EKioskPopup::Payment)
        PlayFeedback(EKioskSound::PaymentShowUp);
    LastPresentedPopup=Presented;
}
void UTransitKioskWidget::HandleAction(FName Action)
{
    if (!Session || bShuttingDown || Session->Popup == EKioskPopup::TopUpCancelled) return;
    PlayFeedback(EKioskSound::Click);
    if (Session->HasPendingCollection())
    {
        PlayFeedback(EKioskSound::Reject);
        ShowCollectionWarning();
        return;
    }
    if (Action==TEXT("Exit"))
    {
        if (auto* PC=Cast<ATransitPlayerController>(GetOwningPlayer())) PC->CloseKiosk();
        return;
    }
    const auto Before=Session->Popup;
    bool bAccepted=false;
    if (Action==TEXT("Help"))
    {
        bAccepted=Session->ShowAssistance();
        if (bAccepted)
        {
            if (PhysicalKiosk.IsValid()) PhysicalKiosk->PressHelpButton();
            PlayFeedback(EKioskSound::Help);
        }
    }
    else if (Action==TEXT("StaffClose")) bAccepted=Session->DismissAssistance();
    else if (Action==TEXT("CancelPayment")) bAccepted=Before==EKioskPopup::Payment && Session->Back();
    else if (Action==TEXT("Back")) bAccepted=Session->Back();
    else if (Action==TEXT("Home")) bAccepted=Session->CompleteAndReset();
    else if (Session->Popup==EKioskPopup::Assistance) bAccepted=false;
    else if (Action==TEXT("Start")) bAccepted=Session->Start();
    else if (Action==TEXT("English") || Action==TEXT("Chinese") || Action==TEXT("Malay") || Action==TEXT("Tamil"))
        bAccepted=Session->SelectLanguage(Action.ToString());
    else if (Action==TEXT("Purchase")) bAccepted=Session->BeginPurchase();
    else if (Action==TEXT("Destination")) bAccepted=Session->BeginDestination();
    else if (Action==TEXT("TopUp")) bAccepted=Session->BeginTopUp();
    else if (Action==TEXT("ReadCard")) bAccepted=Session->ReadCard();
    else if (Action==TEXT("DayPass")) bAccepted=Session->SelectProduct(EKioskProduct::DayPass);
    else if (Action==TEXT("Confirm")) bAccepted=Session->ConfirmDestination();
    else if (Action==TEXT("OneWay")) bAccepted=Session->SelectProduct(EKioskProduct::OneWay);
    else if (Action==TEXT("RoundTrip")) bAccepted=Session->SelectProduct(EKioskProduct::RoundTrip);
    else if (Action==TEXT("QuantityMinus")) bAccepted=Session->SetTicketQuantity(Session->TicketQuantity-1);
    else if (Action==TEXT("QuantityPlus")) bAccepted=Session->SetTicketQuantity(Session->TicketQuantity+1);
    else if (Action==TEXT("QuantityConfirm")) bAccepted=Session->ConfirmTicketQuantity();
    else if (Action.ToString().StartsWith(TEXT("Station")))
        bAccepted=Session->SelectDestination(FCString::Atoi(*Action.ToString().RightChop(7)));
    else if (Action.ToString().StartsWith(TEXT("Amount")))
    {
        const int32 I=FCString::Atoi(*Action.ToString().RightChop(6));
        if (Session->Config.TopUpAmountsCents.IsValidIndex(I)) bAccepted=Session->SelectTopUp(Session->Config.TopUpAmountsCents[I]);
    }
    else if (Action==TEXT("Pay"))
    {
        bAccepted=Session->StartPayment();
        if (bAccepted) GetWorld()->GetTimerManager().SetTimer(PaymentTimer,this,&UTransitKioskWidget::FinishPayment,Session->Config.ProcessingSeconds,false);
    }
    const bool bCancelled = Action == TEXT("CancelPayment") ||
        (Action == TEXT("Back") && (Before == EKioskPopup::Trip || Before == EKioskPopup::Quantity ||
         Before == EKioskPopup::Payment || Session->HasPendingCollection()));
    if (!bAccepted || bCancelled) PlayFeedback(EKioskSound::Reject);
    else if (Action==TEXT("Confirm") || Action==TEXT("QuantityConfirm") || Action==TEXT("StaffClose") || Action==TEXT("ReadCard"))
        PlayFeedback(EKioskSound::Accept);
    Refresh();
    if (!bWorldSpace) SetKeyboardFocus();
}

void UTransitKioskWidget::ShowCollectionWarning()
{
    Refresh();
}
void UTransitKioskWidget::RequireCardCollection()
{
    if (!Session || !Session->bCardPresent) return;
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(PaymentTimer);
    Session->RequestCardCollection();
    PlayFeedback(EKioskSound::Reject);
    Refresh();
}

void UTransitKioskWidget::ResumeKiosk()
{
    bShuttingDown = false;
    Refresh();
}

void UTransitKioskWidget::PhysicalItemCollected()
{
    if (Session && Session->Popup == EKioskPopup::TopUpCancelled)
    {
        if (auto* World = GetWorld())
        {
            auto& Timers = World->GetTimerManager();
            Timers.ClearTimer(PaymentTimer);
            Timers.SetTimer(TopUpCancellationTimer, this, &UTransitKioskWidget::FinishTopUpCancellation, 5.f, false);
        }
        PlayFeedback(EKioskSound::Reject);
    }
    Refresh();
}

void UTransitKioskWidget::FinishTopUpCancellation()
{
    if (bShuttingDown || !Session || Session->Popup != EKioskPopup::TopUpCancelled) return;
    Session->Reset();
    Refresh();
}

void UTransitKioskWidget::SetWorldSpaceMode()
{
    bWorldSpace = true;
    SetIsFocusable(false);
    for (FName Name : {TEXT("BTN_HardwareHelp"),TEXT("BTN_HardwarePay"),TEXT("BTN_HardwareCard"),TEXT("BTN_ExitKiosk"),TEXT("LOC_Demo")})
        if (auto* Item = GetWidgetFromName(Name)) Item->SetVisibility(ESlateVisibility::Collapsed);
    if (auto* Item = GetWidgetFromName(TEXT("Scale_Kiosk")))
        if (auto* ScreenSlot = Cast<UCanvasPanelSlot>(Item->Slot))
        {
            ScreenSlot->SetAnchors(FAnchors(0,0,1,1));
            ScreenSlot->SetOffsets(FMargin(0));
        }
}
void UTransitKioskWidget::FinishPayment()
{
    if (bShuttingDown || !Session || !Session->CompletePayment(FDateTime::Now()))
        return;
    if (auto *PC = Cast<ATransitPlayerController>(GetOwningPlayer()))
        PC->StoreDemoProgress(Session->CardBalanceCents);
    PlayFeedback(EKioskSound::Accept);
    Refresh();
}
void UTransitKioskWidget::PlaceProjectedControl(FName Name, const FVector2D &Position, const FVector2D &Size,
                                                float Scale)
{
    if (auto *Item = GetWidgetFromName(Name))
        if (auto *ProjectionSlot = Cast<UCanvasPanelSlot>(Item->Slot))
        {
            ProjectionSlot->SetAnchors(FAnchors(0, 0));
            ProjectionSlot->SetAlignment(FVector2D::ZeroVector);
            ProjectionSlot->SetPosition(Position / Scale);
            ProjectionSlot->SetSize(Size / Scale);
        }
}
FReply UTransitKioskWidget::NativeOnPreviewKeyDown(const FGeometry &Geometry, const FKeyEvent &Key)
{
    if (Key.GetKey() == EKeys::Escape)
    {
        if (Session && (Session->Popup == EKioskPopup::Assistance || Session->Popup == EKioskPopup::Trip ||
                        Session->Popup == EKioskPopup::Payment || Session->Popup == EKioskPopup::Quantity))
            HandleAction(TEXT("Back"));
        else
            HandleAction(TEXT("Exit"));
        return FReply::Handled();
    }
    if (Key.GetKey() == EKeys::P)
    {
        HandleAction(TEXT("Pay"));
        return FReply::Handled();
    }
    if (Key.GetKey() == EKeys::C)
    {
        HandleAction(TEXT("ReadCard"));
        return FReply::Handled();
    }
    if (Key.GetKey() == EKeys::F1)
    {
        HandleAction(TEXT("Help"));
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry, Key);
}
void UTransitKioskWidget::ShutdownKiosk()
{
    bShuttingDown = true;
    if (auto* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PaymentTimer);
        World->GetTimerManager().ClearTimer(TopUpCancellationTimer);
    }
    if (Session && Session->bCardPresent) Session->RequestCardCollection();
}
void UTransitKioskWidget::NativeDestruct()
{
    ShutdownKiosk();
    UnbindButtons();
    Super::NativeDestruct();
}

void UTransitKioskAimWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    SetVisibility(ESlateVisibility::HitTestInvisible);
    if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this);
    auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Canvas;
    auto MakeText = [&](int32 Size)
    {
        auto* Text = WidgetTree->ConstructWidget<UTextBlock>();
        Text->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),Size));
        Text->SetJustification(ETextJustify::Center);
        Text->SetShadowColorAndOpacity(FLinearColor::Black);
        Text->SetShadowOffset(FVector2D(1,1));
        return Text;
    };
    Reticle = MakeText(20);
    Reticle->SetText(FText::FromString(TEXT("+")));
    auto* AimSlot = Canvas->AddChildToCanvas(Reticle);
    AimSlot->SetAnchors(FAnchors(.5,.5));
    AimSlot->SetAlignment(FVector2D(.5,.5));
    AimSlot->SetSize(FVector2D(26,32));
    HintText = MakeText(16);
    // Reuse the screen's composite font so collection guidance also renders Tamil/Chinese.
    if (auto* HintFont = LoadObject<UFont>(nullptr,TEXT("/Game/Kiosk/Fonts/KioskMultilingual")))
        HintText->SetFont(FSlateFontInfo(HintFont,16,TEXT("Regular")));
    HintText->SetAutoWrapText(true);
    auto* HintSlot = Canvas->AddChildToCanvas(HintText);
    HintSlot->SetAnchors(FAnchors(.5,1));
    HintSlot->SetAlignment(FVector2D(.5,1));
    HintSlot->SetPosition(FVector2D(0,-20));
    HintSlot->SetSize(FVector2D(1050,64));
    SetAimHint(TEXT("Mouse: look   |   Click: interact   |   Esc: exit   |   DEMONSTRATION ONLY"),false);
}

void UTransitKioskAimWidget::SetAimHint(const FString& Hint, bool bReady)
{
    if (Reticle) Reticle->SetColorAndOpacity(bReady ? FLinearColor(.2f,1.f,.5f) : FLinearColor::White);
    if (HintText) HintText->SetText(FText::FromString(Hint));
}

void UTransitHeightPromptWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    SetVisibility(ESlateVisibility::HitTestInvisible);
    if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this);

    auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Canvas;
    auto* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("HeightPromptPanel"));
    Panel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(.018f, .032f, .06f, .94f), 8.f));
    Panel->SetPadding(FMargin(16, 10));
    PromptText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HeightPromptText"));
    PromptText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18));
    PromptText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    Panel->AddChild(PromptText);

    auto* PromptSlot = Canvas->AddChildToCanvas(Panel);
    PromptSlot->SetAnchors(FAnchors(1.f, 0.f));
    PromptSlot->SetAlignment(FVector2D(1.f, 0.f));
    PromptSlot->SetPosition(FVector2D(-20.f, 20.f));
    PromptSlot->SetAutoSize(true);
    UpdatePrompt(false);
}

void UTransitHeightPromptWidget::UpdatePrompt(bool bChildViewEnabled)
{
    if (!PromptText) return;
    if (bChildViewEnabled)
        PromptText->SetText(NSLOCTEXT("TransitHUD", "NormalHeightPrompt", "Press C to return to normal height"));
    else
        PromptText->SetText(FText::Format(NSLOCTEXT("TransitHUD", "ChildHeightPrompt", "Press C for child height ({0} cm)"),
            FText::AsNumber(FMath::RoundToInt(ATransitPlayerController::ChildEyeHeightCm))));
}
