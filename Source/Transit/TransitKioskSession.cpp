#include "TransitKioskSession.h"
#include "TransitKioskText.h"

void UTransitKioskSession::Configure(const FTransitKioskConfig &InConfig, int32 BalanceCents)
{
    Config = InConfig;
    if (Config.Stations.Num() != 8)
        Config.Stations = FTransitKioskConfig().Stations;
    for (auto &Station : Config.Stations)
    {
        Station.OneWayCents = FMath::Clamp(Station.OneWayCents, 1, 100000);
        Station.MapPosition.X = FMath::Clamp(Station.MapPosition.X, 0., 1.);
        Station.MapPosition.Y = FMath::Clamp(Station.MapPosition.Y, 0., 1.);
    }
    Config.DayPassCents = FMath::Clamp(Config.DayPassCents, 1, 100000);
    Config.ProcessingSeconds = FMath::Clamp(Config.ProcessingSeconds, 0.1f, 30.f);
    Config.MaxTicketsPerPurchase = FMath::Clamp(Config.MaxTicketsPerPurchase,1,20);
    Config.TopUpAmountsCents.RemoveAll([](int32 Cents) { return Cents <= 0 || Cents > 100000; });
    Config.TopUpAmountsCents.Sort();
    if (Config.TopUpAmountsCents.Num() != 6)
        Config.TopUpAmountsCents = FTransitKioskConfig().TopUpAmountsCents;
    CardBalanceCents = FMath::Clamp(BalanceCents, 0, MAX_int32 - 100000);
    bTicketAwaitingCollection = false;
    bCardAwaitingCollection = false;
    bCardPresent = false;
    Reset();
}
void UTransitKioskSession::Reset()
{
    if (HasPendingCollection() || bCardPresent) return;
    Page = EKioskPage::Welcome;
    Popup = PopupBeforeAssistance = EKioskPopup::None;
    Product = EKioskProduct::None;
    Language = TEXT("English");
    Destination = INDEX_NONE;
    TopUpCents = 0;
    TicketQuantity = 1;
    bCardPresent = false;
    PaidTotalCents = 0;
    ReceiptId.Reset();
    ValidUntil = FDateTime();
}
bool UTransitKioskSession::Start()
{
    if (Page != EKioskPage::Welcome || Popup != EKioskPopup::None)
        return false;
    Page = EKioskPage::Language;
    return true;
}
bool UTransitKioskSession::SelectLanguage(const FString &Value)
{
    if (Page != EKioskPage::Language || Popup != EKioskPopup::None ||
        !TArray<FString>{TEXT("English"), TEXT("Chinese"), TEXT("Malay"), TEXT("Tamil")}.Contains(Value))
        return false;
    Language = Value;
    Page = EKioskPage::MainMenu;
    return true;
}
bool UTransitKioskSession::BeginPurchase()
{
    if (Page != EKioskPage::MainMenu || Popup != EKioskPopup::None)
        return false;
    Page = EKioskPage::TicketType;
    Product = EKioskProduct::None;
    return true;
}
bool UTransitKioskSession::BeginDestination()
{
    if (Page != EKioskPage::TicketType || Popup != EKioskPopup::None)
        return false;
    Page = EKioskPage::Route;
    Product = EKioskProduct::None;
    return true;
}
bool UTransitKioskSession::SelectDestination(int32 Index)
{
    if (Page != EKioskPage::Route || Popup != EKioskPopup::None || !Config.Stations.IsValidIndex(Index))
        return false;
    Destination = Index;
    Product = EKioskProduct::None;
    return true;
}
bool UTransitKioskSession::CanConfirmDestination() const
{
    return Page == EKioskPage::Route && Config.Stations.IsValidIndex(Destination);
}
bool UTransitKioskSession::ConfirmDestination()
{
    if (Popup != EKioskPopup::None || !CanConfirmDestination())
        return false;
    Popup = EKioskPopup::Trip;
    return true;
}
bool UTransitKioskSession::SelectProduct(EKioskProduct Value)
{
    const bool bDay = Value == EKioskProduct::DayPass;
    if (bDay ? (Page != EKioskPage::TicketType || Popup != EKioskPopup::None)
             : (Page != EKioskPage::Route || Popup != EKioskPopup::Trip || !Config.Stations.IsValidIndex(Destination) ||
                (Value != EKioskProduct::OneWay && Value != EKioskProduct::RoundTrip)))
        return false;
    Product = Value;
    TicketQuantity = 1;
    PaidTotalCents = 0;
    ReceiptId.Reset();
    Popup = EKioskPopup::Quantity;
    return true;
}
bool UTransitKioskSession::SetTicketQuantity(int32 Quantity)
{
    if (Popup!=EKioskPopup::Quantity || HasPendingCollection() || Quantity<1 || Quantity>Config.MaxTicketsPerPurchase)
        return false;
    TicketQuantity=Quantity;
    return true;
}
bool UTransitKioskSession::ConfirmTicketQuantity()
{
    if (Popup!=EKioskPopup::Quantity || !CanPay()) return false;
    Popup=EKioskPopup::Payment;
    return true;
}
bool UTransitKioskSession::BeginTopUp()
{
    if (Page != EKioskPage::MainMenu || Popup != EKioskPopup::None)
        return false;
    Page = EKioskPage::CardPlacement;
    Product = EKioskProduct::TopUp;
    return true;
}
bool UTransitKioskSession::ReadCard()
{
    if (HasPendingCollection() || Page != EKioskPage::CardPlacement || Popup != EKioskPopup::None)
        return false;
    bCardPresent = true;
    Page = EKioskPage::TopUp;
    return true;
}
bool UTransitKioskSession::SelectTopUp(int32 Amount)
{
    if (HasPendingCollection() || Page != EKioskPage::TopUp || Popup != EKioskPopup::None || !bCardPresent ||
        !Config.TopUpAmountsCents.Contains(Amount) || CardBalanceCents > MAX_int32 - Amount)
        return false;
    Product = EKioskProduct::TopUp;
    TopUpCents = Amount;
    PaidTotalCents = 0;
    ReceiptId.Reset();
    Popup = EKioskPopup::Payment;
    return true;
}
int32 UTransitKioskSession::GetOneWayCents() const
{
    return Config.Stations.IsValidIndex(Destination) ? Config.Stations[Destination].OneWayCents : 0;
}
int32 UTransitKioskSession::GetTotalCents() const
{
    if (PaidTotalCents > 0)
        return PaidTotalCents;
    if (Product == EKioskProduct::TopUp)
        return TopUpCents;
    return GetUnitPriceCents()*TicketQuantity;
}
int32 UTransitKioskSession::GetUnitPriceCents() const
{
    if (Product == EKioskProduct::DayPass) return Config.DayPassCents;
    const int32 Fare = GetOneWayCents();
    return Product == EKioskProduct::RoundTrip ? (Fare * 3 + 1) / 2 : Product==EKioskProduct::OneWay ? Fare : 0;
}
bool UTransitKioskSession::CanPay() const
{
    if (HasPendingCollection()) return false;
    if (Product == EKioskProduct::TopUp)
        return bCardPresent && Config.TopUpAmountsCents.Contains(TopUpCents) &&
               CardBalanceCents <= MAX_int32 - TopUpCents;
    if (TicketQuantity<1 || TicketQuantity>Config.MaxTicketsPerPurchase) return false;
    if (Product==EKioskProduct::DayPass) return true;
    return (Product == EKioskProduct::OneWay || Product == EKioskProduct::RoundTrip) &&
           Config.Stations.IsValidIndex(Destination);
}
bool UTransitKioskSession::StartPayment()
{
    if (Popup != EKioskPopup::Payment || !CanPay() || GetTotalCents() <= 0)
        return false;
    Popup = EKioskPopup::Processing;
    return true;
}
bool UTransitKioskSession::CompletePayment(const FDateTime &Now)
{
    const bool bHelp = Popup == EKioskPopup::Assistance;
    if ((bHelp ? PopupBeforeAssistance : Popup) != EKioskPopup::Processing || !CanPay())
        return false;
    PaidTotalCents = GetTotalCents();
    if (Product == EKioskProduct::TopUp)
        CardBalanceCents += TopUpCents;
    ValidUntil = NextMidnight(Now);
    ReceiptId = TEXT("DEMO-") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8).ToUpper();
    bTicketAwaitingCollection = Product != EKioskProduct::TopUp;
    bCardAwaitingCollection = bCardPresent;
    // Collection takes priority even if payment finished while assistance was open.
    Popup = EKioskPopup::Success;
    PopupBeforeAssistance = EKioskPopup::None;
    return true;
}
bool UTransitKioskSession::ShowAssistance()
{
    if (HasPendingCollection() || Popup == EKioskPopup::Assistance || Popup == EKioskPopup::TopUpCancelled)
        return false;
    PopupBeforeAssistance = Popup;
    Popup = EKioskPopup::Assistance;
    return true;
}
bool UTransitKioskSession::DismissAssistance()
{
    if (Popup != EKioskPopup::Assistance)
        return false;
    Popup = PopupBeforeAssistance;
    PopupBeforeAssistance = EKioskPopup::None;
    return true;
}
bool UTransitKioskSession::CompleteAndReset()
{
    if (Popup != EKioskPopup::Success || HasPendingCollection() || bCardPresent)
        return false;
    Reset();
    return true;
}
bool UTransitKioskSession::CollectTicket()
{
    const auto EffectivePopup = Popup == EKioskPopup::Assistance ? PopupBeforeAssistance : Popup;
    if (!bTicketAwaitingCollection || EffectivePopup != EKioskPopup::Success) return false;
    bTicketAwaitingCollection = false;
    return true;
}
bool UTransitKioskSession::RequestCardCollection()
{
    if (!bCardPresent) return false;
    bCardAwaitingCollection = true;
    if (Popup != EKioskPopup::Success) Popup = EKioskPopup::None;
    PopupBeforeAssistance = EKioskPopup::None;
    return true;
}
bool UTransitKioskSession::CanCancelUnpaidTopUp() const
{
    const auto TransactionPopup = Popup == EKioskPopup::Assistance ? PopupBeforeAssistance : Popup;
    return bCardPresent && !HasPendingCollection() && Product == EKioskProduct::TopUp && Page == EKioskPage::TopUp &&
           (TransactionPopup == EKioskPopup::None || TransactionPopup == EKioskPopup::Payment);
}
bool UTransitKioskSession::CollectCard()
{
    if (CanCancelUnpaidTopUp())
    {
        bCardPresent = false;
        TopUpCents = 0;
        Product = EKioskProduct::None;
        Popup = EKioskPopup::TopUpCancelled;
        PopupBeforeAssistance = EKioskPopup::None;
        return true;
    }
    if (!bCardPresent || !bCardAwaitingCollection) return false;
    bCardAwaitingCollection = false;
    bCardPresent = false;
    if (Popup != EKioskPopup::Success)
    {
        Page = EKioskPage::CardPlacement;
        TopUpCents = 0;
    }
    return true;
}
bool UTransitKioskSession::Back()
{
    if (HasPendingCollection()) return false;
    if (Popup == EKioskPopup::Assistance)
        return DismissAssistance();
    if (Popup==EKioskPopup::Quantity)
    {
        Popup=Product==EKioskProduct::DayPass ? EKioskPopup::None : EKioskPopup::Trip;
        return true;
    }
    if (Popup==EKioskPopup::Payment && Product!=EKioskProduct::TopUp)
    {
        Popup=EKioskPopup::Quantity;
        return true;
    }
    if (Popup == EKioskPopup::Trip || Popup == EKioskPopup::Payment)
    {
        Popup = EKioskPopup::None;
        return true;
    }
    if (Popup != EKioskPopup::None)
        return false;
    if (bCardPresent && (Page == EKioskPage::TopUp || Page == EKioskPage::CardPlacement))
        return RequestCardCollection();
    switch (Page)
    {
    case EKioskPage::Language:
        Page = EKioskPage::Welcome;
        break;
    case EKioskPage::MainMenu:
        Page = EKioskPage::Language;
        break;
    case EKioskPage::TicketType:
        Page = EKioskPage::MainMenu;
        break;
    case EKioskPage::Route:
        Page = EKioskPage::TicketType;
        break;
    case EKioskPage::CardPlacement:
        Page = EKioskPage::MainMenu;
        break;
    case EKioskPage::TopUp:
        Page = EKioskPage::CardPlacement;
        break;
    default:
        return false;
    }
    return true;
}
FDateTime UTransitKioskSession::NextMidnight(const FDateTime &Now)
{
    return Now.GetDate() + FTimespan::FromDays(1);
}
FString UTransitKioskSession::Money(int32 Cents) const
{
    return FString::Printf(TEXT("$%d.%02d"), Cents / 100, Cents % 100);
}
FString UTransitKioskSession::ProductName() const
{
    switch (Product)
    {
    case EKioskProduct::OneWay:
        return KioskText(Language, TEXT("SingleTicket"));
    case EKioskProduct::RoundTrip:
        return KioskText(Language, TEXT("RoundTicket"));
    case EKioskProduct::DayPass:
        return KioskText(Language, TEXT("DayDescription"));
    case EKioskProduct::TopUp:
        return KioskText(Language, TEXT("TopUp"));
    default:
        return FString();
    }
}
FString UTransitKioskSession::StationName() const
{
    if (!Config.Stations.IsValidIndex(Destination))
        return FString();
    return Language == TEXT("Chinese")
               ? Config.Stations[Destination].Code + TEXT(" ") + KioskText(Language, TEXT("Station"))
               : KioskText(Language, TEXT("Station")) + TEXT(" ") + Config.Stations[Destination].Code;
}
FString UTransitKioskSession::ValidityText() const
{
    return Product == EKioskProduct::DayPass ? KioskText(Language, TEXT("DayValidity")) + TEXT("\n") +
                                                   (ValidUntil.GetTicks() ? ValidUntil : NextMidnight(FDateTime::Now()))
                                                       .ToString(TEXT("%Y-%m-%d %H:%M"))
                                             : FString();
}
