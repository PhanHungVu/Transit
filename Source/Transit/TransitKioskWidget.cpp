#include "TransitKioskWidget.h"
#include "TransitPlayerController.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Components/ComboBoxString.h"
#include "InputCoreTypes.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UTransitKioskButtonBinding::Click() { if (Owner) Owner->HandleAction(Action); }

void UTransitKioskWidget::InitializeKiosk(const FTransitKioskConfig& Config, int32 CardBalanceCents, bool bDayPassPurchased)
{
    Session = NewObject<UTransitKioskSession>(this);
    Session->Configure(Config, CardBalanceCents, bDayPassPurchased);
    bShuttingDown = false;
}

void UTransitKioskWidget::BindButton(FName WidgetName, FName Action)
{
    if (auto* Button = Cast<UButton>(GetWidgetFromName(WidgetName)))
    {
        auto* Binding = NewObject<UTransitKioskButtonBinding>(this);
        Binding->Button = Button; Binding->Owner = this; Binding->Action = Action;
        Button->OnClicked.AddDynamic(Binding, &UTransitKioskButtonBinding::Click);
        ButtonBindings.Add(Binding);
    }
    else UE_LOG(LogTemp, Error, TEXT("Missing kiosk button %s"), *WidgetName.ToString());
}

void UTransitKioskWidget::UnbindButtons()
{
    for (UTransitKioskButtonBinding* Binding : ButtonBindings)
        if (Binding && Binding->Button) Binding->Button->OnClicked.RemoveDynamic(Binding, &UTransitKioskButtonBinding::Click);
    ButtonBindings.Reset();
}

void UTransitKioskWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(true);
    if (!Session) InitializeKiosk(FTransitKioskConfig(), 750);
    UnbindButtons();
    const TPair<FName, FName> Buttons[] = {
        {TEXT("BTN_Start"), TEXT("Start")},
        {TEXT("BTN_LanguageEnglish"), TEXT("English")},
        {TEXT("BTN_LanguageEnglish_1"), TEXT("Chinese")},
        {TEXT("BTN_LanguageEnglish_2"), TEXT("Malay")},
        {TEXT("BTN_LanguageEnglish_3"), TEXT("Tamil")},
        {TEXT("BTN_Purchase"), TEXT("Purchase")}, {TEXT("BTN_TopUp"), TEXT("TopUp")},
        {TEXT("BTN_OneWay"), TEXT("OneWay")}, {TEXT("BTN_RoundTrip"), TEXT("RoundTrip")}, {TEXT("BTN_DayPass"), TEXT("DayPass")},
        {TEXT("BTN_RouteContinue"), TEXT("Review")}, {TEXT("BTN_ReviewProceed"), TEXT("Payment")},
        {TEXT("BTN_TopUp0"), TEXT("Amount0")}, {TEXT("BTN_TopUp1"), TEXT("Amount1")}, {TEXT("BTN_TopUp2"), TEXT("Amount2")},
        {TEXT("BTN_PayCard"), TEXT("Pay")}, {TEXT("BTN_SuccessHome"), TEXT("Home")},
        {TEXT("BTN_ExitKiosk"), TEXT("Exit")}, {TEXT("BTN_Help"), TEXT("Help")},
        {TEXT("BTN_StaffRequest"), TEXT("StaffRequest")}, {TEXT("BTN_StaffClose"), TEXT("StaffClose")}
    };
    for (const auto& Pair : Buttons) BindButton(Pair.Key, Pair.Value);
    for (FName Name : {FName(TEXT("BTN_LanguageBack")), FName(TEXT("BTN_MainMenuBack")), FName(TEXT("BTN_MainMenuBack_1")),
        FName(TEXT("BTN_RouteBack")), FName(TEXT("BTN_ReviewBack")), FName(TEXT("BTN_TopUpBack")), FName(TEXT("BTN_PaymentBack"))})
        BindButton(Name, TEXT("Back"));
    bRefreshing = true;
    for (FName Name : {FName(TEXT("CB_Origin")), FName(TEXT("CB_Destination"))})
        if (auto* Combo = Cast<UComboBoxString>(GetWidgetFromName(Name)))
        {
            Combo->ClearOptions();
            for (const auto& Station : Session->Config.Stations) Combo->AddOption(Station);
            Combo->OnSelectionChanged.AddUniqueDynamic(this, &UTransitKioskWidget::RouteChanged);
        }
    bRefreshing = false;
    ShowStaff(false);
    Refresh();
}

void UTransitKioskWidget::SetText(FName Name, const FString& Value)
{
    if (auto* Text = Cast<UTextBlock>(GetWidgetFromName(Name))) Text->SetText(FText::FromString(Value));
}

void UTransitKioskWidget::RouteChanged(FString Selected, ESelectInfo::Type SelectionType)
{
    if (bRefreshing || !Session || Session->Page != EKioskPage::Route) return;
    auto* Origin = Cast<UComboBoxString>(GetWidgetFromName(TEXT("CB_Origin")));
    auto* Destination = Cast<UComboBoxString>(GetWidgetFromName(TEXT("CB_Destination")));
    if (Origin && Destination) Session->SelectRoute(Origin->GetSelectedIndex(), Destination->GetSelectedIndex());
    Refresh();
}

void UTransitKioskWidget::Refresh()
{
    if (!Session) return;
    bRefreshing = true;
    if (auto* Pages = Cast<UWidgetSwitcher>(GetWidgetFromName(TEXT("WS_Pages")))) Pages->SetActiveWidgetIndex(static_cast<int32>(Session->Page));
    auto Station = [this](int32 Index) { return Session->Config.Stations.IsValidIndex(Index) ? Session->Config.Stations[Index] : TEXT("Select a station"); };
    const bool bTopUp = Session->Product == EKioskProduct::TopUp;
    const bool bDayPass = Session->Product == EKioskProduct::DayPass;
    const FString Route = bDayPass ? TEXT("All demonstration routes") : bTopUp ? TEXT("Demo card **** 0421") : Station(Session->Origin) + TEXT("  >  ") + Station(Session->Destination);
    const FString Total = Session->Money(Session->GetTotalCents());
    if (auto* DayPass = Cast<UButton>(GetWidgetFromName(TEXT("BTN_DayPass"))))
    {
        DayPass->SetIsEnabled(!Session->bDayPassPurchased);
        DayPass->SetToolTipText(FText::FromString(Session->bDayPassPurchased ? TEXT("You already purchased a day pass this play session.") : TEXT("Unlimited travel until midnight.")));
    }
    SetText(TEXT("TXT_DayPassDescription"), Session->bDayPassPurchased ? TEXT("Already purchased this play session") : TEXT("Unlimited travel until midnight"));
    SetText(TEXT("TXT_ReviewTitle"), bTopUp ? TEXT("Review Your Top-Up") : TEXT("Review Your Ticket"));
    SetText(TEXT("TXT_ReviewDescription"), Session->ValidityText());
    SetText(TEXT("TXT_ReviewTicketTypeValue"), Session->ProductName());
    SetText(TEXT("TXT_ReviewOriginLabel"), bTopUp ? TEXT("Card:") : TEXT("From:"));
    SetText(TEXT("TXT_ReviewOriginValue"), bTopUp ? TEXT("Demo card **** 0421") : bDayPass ? TEXT("Any station") : Station(Session->Origin));
    SetText(TEXT("TXT_ReviewDestinationLabel"), bTopUp ? TEXT("Balance after:") : TEXT("To:"));
    SetText(TEXT("TXT_ReviewDestinationValue"), bTopUp ? Session->Money(Session->CardBalanceCents + Session->TopUpCents) : bDayPass ? TEXT("Any station") : Station(Session->Destination));
    SetText(TEXT("TXT_ReviewFareValue"), Total);
    SetText(TEXT("TXT_TopUpBalance"), TEXT("Demo card **** 0421  |  Current balance: ") + Session->Money(Session->CardBalanceCents));
    for (int32 Index = 0; Index < 3; ++Index)
    {
        const bool bAvailable = Session->Config.TopUpAmountsCents.IsValidIndex(Index);
        SetText(*FString::Printf(TEXT("TXT_TopUp%d"), Index), bAvailable ? Session->Money(Session->Config.TopUpAmountsCents[Index]) : TEXT("Unavailable"));
        if (auto* Button = Cast<UButton>(GetWidgetFromName(*FString::Printf(TEXT("BTN_TopUp%d"), Index)))) Button->SetIsEnabled(bAvailable);
    }
    SetText(TEXT("TXT_PaymentSummary"), Session->ProductName() + TEXT("\n") + Route + TEXT("\nTotal: ") + Total);
    SetText(TEXT("TXT_SuccessTitle"), bTopUp ? TEXT("Top-Up Complete!") : TEXT("Your Ticket Is Ready!"));
    SetText(TEXT("TXT_SuccessSummary"), Session->ProductName() + TEXT("  |  ") + Total + TEXT("\n") + Route + TEXT("\n") + Session->ValidityText() + TEXT("\nReceipt: ") + Session->ReceiptId
        + (bTopUp ? TEXT("\nNew balance: ") + Session->Money(Session->CardBalanceCents) : TEXT("")));
    SetText(TEXT("TXT_LanguageStatus"), TEXT("Language: ") + Session->Language + (Session->Language == TEXT("English") ? TEXT("") : TEXT(" | English demo interface")));
    SetText(TEXT("TXT_MapPlaceholder"), FString::Join(Session->Config.Stations, TEXT("  --  ")));
    SetText(TEXT("TextBlock_2278"), Session->CanReview() ? TEXT("Demonstration route  |  Fare: ") + Total : TEXT("Choose two different stations to continue."));
    if (auto* Continue = Cast<UButton>(GetWidgetFromName(TEXT("BTN_RouteContinue")))) Continue->SetIsEnabled(Session->CanReview());
    if (Session->Page == EKioskPage::Route)
    {
        if (auto* Origin = Cast<UComboBoxString>(GetWidgetFromName(TEXT("CB_Origin")))) Origin->SetSelectedIndex(Session->Origin);
        if (auto* Destination = Cast<UComboBoxString>(GetWidgetFromName(TEXT("CB_Destination")))) Destination->SetSelectedIndex(Session->Destination);
    }
    bRefreshing = false;
}

void UTransitKioskWidget::HandleAction(FName Action)
{
    if (!Session || bShuttingDown) return;
    if (Action == TEXT("Exit")) { if (auto* PC = Cast<ATransitPlayerController>(GetOwningPlayer())) PC->CloseKiosk(); return; }
    if (Action == TEXT("StaffClose")) { ShowStaff(false); return; }
    if (Action == TEXT("StaffRequest") && bStaffVisible && !bStaffRequested)
    {
        bStaffRequested = true;
        SetText(TEXT("TXT_StaffStatus"), TEXT("Demo request received. A simulated staff member is on the way..."));
        if (auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("BTN_StaffRequest")))) Button->SetIsEnabled(false);
        GetWorld()->GetTimerManager().SetTimer(StaffTimer, this, &UTransitKioskWidget::StaffArrived, 2.f, false);
        return;
    }
    if (bStaffVisible) return;
    if (Action == TEXT("Help")) { ShowStaff(true); return; }
    if (Session->Page == EKioskPage::Processing) return;
    if (Action == TEXT("Start")) Session->Start();
    else if (Action == TEXT("English") || Action == TEXT("Chinese") || Action == TEXT("Malay") || Action == TEXT("Tamil")) Session->SelectLanguage(Action.ToString());
    else if (Action == TEXT("Purchase")) Session->BeginPurchase();
    else if (Action == TEXT("TopUp")) Session->BeginTopUp();
    else if (Action == TEXT("OneWay")) Session->SelectProduct(EKioskProduct::OneWay);
    else if (Action == TEXT("RoundTrip")) Session->SelectProduct(EKioskProduct::RoundTrip);
    else if (Action == TEXT("DayPass")) Session->SelectProduct(EKioskProduct::DayPass);
    else if (Action == TEXT("Review")) Session->ReviewRoute();
    else if (Action == TEXT("Payment")) Session->ProceedToPayment();
    else if (Action == TEXT("Amount0") || Action == TEXT("Amount1") || Action == TEXT("Amount2"))
    {
        const int32 Index = Action.ToString().Right(1).IsNumeric() ? FCString::Atoi(*Action.ToString().Right(1)) : INDEX_NONE;
        if (Session->Config.TopUpAmountsCents.IsValidIndex(Index)) Session->SelectTopUp(Session->Config.TopUpAmountsCents[Index]);
    }
    else if (Action == TEXT("Pay") && Session->StartPayment())
        GetWorld()->GetTimerManager().SetTimer(PaymentTimer, this, &UTransitKioskWidget::FinishPayment, Session->Config.ProcessingSeconds, false);
    else if (Action == TEXT("Back")) Session->Back();
    else if (Action == TEXT("Home") && Session->Page == EKioskPage::Success)
    {
        Session->Reset(); bStaffRequested = false;
        GetWorld()->GetTimerManager().ClearTimer(StaffTimer);
    }
    Refresh();
    SetKeyboardFocus();
}

void UTransitKioskWidget::FinishPayment()
{
    if (bShuttingDown || !Session || !Session->CompletePayment(FDateTime::Now())) return;
    if (auto* PC = Cast<ATransitPlayerController>(GetOwningPlayer())) PC->StoreDemoProgress(Session->CardBalanceCents, Session->bDayPassPurchased);
    Refresh();
}

void UTransitKioskWidget::ShowStaff(bool bShow)
{
    if (!bShow)
    {
        if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(StaffTimer);
        bStaffRequested = false;
    }
    bStaffVisible = bShow;
    if (auto* Overlay = GetWidgetFromName(TEXT("StaffOverlay"))) Overlay->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (auto* Pages = GetWidgetFromName(TEXT("WS_Pages"))) Pages->SetIsEnabled(!bShow);
    if (auto* Help = GetWidgetFromName(TEXT("BTN_Help"))) Help->SetIsEnabled(!bShow);
    if (auto* Request = GetWidgetFromName(TEXT("BTN_StaffRequest"))) Request->SetIsEnabled(!bStaffRequested);
    if (!bStaffRequested) SetText(TEXT("TXT_StaffStatus"), TEXT("Need a hand? Request a simulated staff member.\nYour current selections will be kept."));
    SetKeyboardFocus();
}

void UTransitKioskWidget::StaffArrived()
{
    if (!bShuttingDown && bStaffVisible && bStaffRequested) SetText(TEXT("TXT_StaffStatus"), TEXT("A simulated staff member is ready to assist.\nThis is a demonstration; no real staff were contacted."));
}

FReply UTransitKioskWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
    if (KeyEvent.GetKey() == EKeys::Escape)
    {
        if (bStaffVisible) ShowStaff(false);
        else HandleAction(TEXT("Exit"));
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry, KeyEvent);
}

void UTransitKioskWidget::ShutdownKiosk()
{
    bShuttingDown = true;
    if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(PaymentTimer); GetWorld()->GetTimerManager().ClearTimer(StaffTimer); }
}

void UTransitKioskWidget::NativeDestruct()
{
    ShutdownKiosk(); UnbindButtons();
    Super::NativeDestruct();
}
