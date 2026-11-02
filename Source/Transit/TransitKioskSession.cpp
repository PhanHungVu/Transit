#include "TransitKioskSession.h"

void UTransitKioskSession::Configure(const FTransitKioskConfig& InConfig, int32 BalanceCents, bool bInDayPassPurchased)
{
    Config = InConfig;
    Config.OneWayBaseCents = FMath::Clamp(Config.OneWayBaseCents, 1, 100000);
    Config.AdditionalStopCents = FMath::Clamp(Config.AdditionalStopCents, 0, 100000);
    Config.DayPassCents = FMath::Clamp(Config.DayPassCents, 1, 100000);
    Config.ProcessingSeconds = FMath::Clamp(Config.ProcessingSeconds, 0.1f, 30.f);
    Config.Stations.SetNum(FMath::Min(Config.Stations.Num(), 100));
    Config.TopUpAmountsCents.RemoveAll([](int32 Cents) { return Cents <= 0 || Cents > 100000; });
    Config.TopUpAmountsCents.Sort();
    Config.TopUpAmountsCents.SetNum(FMath::Min(3, Config.TopUpAmountsCents.Num()));
    if (Config.TopUpAmountsCents.IsEmpty()) Config.TopUpAmountsCents = {500, 1000, 2000};
    CardBalanceCents = FMath::Clamp(BalanceCents, 0, MAX_int32 - 100000);
    bDayPassPurchased = bInDayPassPurchased;
    Reset();
}

void UTransitKioskSession::Reset()
{
    Page = EKioskPage::Welcome; Product = EKioskProduct::None;
    Language = TEXT("English"); Origin = 0; Destination = 1; TopUpCents = 0;
    ReceiptId.Reset(); ValidUntil = FDateTime();
}

bool UTransitKioskSession::Start()
{
    if (Page != EKioskPage::Welcome) return false;
    Page = EKioskPage::Language; return true;
}

bool UTransitKioskSession::SelectLanguage(const FString& InLanguage)
{
    if (Page != EKioskPage::Language || !TArray<FString>{TEXT("English"), TEXT("Chinese"), TEXT("Malay"), TEXT("Tamil")}.Contains(InLanguage)) return false;
    Language = InLanguage; Page = EKioskPage::MainMenu; return true;
}

bool UTransitKioskSession::BeginPurchase()
{
    if (Page != EKioskPage::MainMenu) return false;
    Product = EKioskProduct::None; Page = EKioskPage::TicketType; return true;
}

bool UTransitKioskSession::SelectProduct(EKioskProduct InProduct)
{
    if (Page != EKioskPage::TicketType || InProduct == EKioskProduct::None || InProduct == EKioskProduct::TopUp) return false;
    if (InProduct == EKioskProduct::DayPass && bDayPassPurchased) return false;
    Product = InProduct; ReceiptId.Reset();
    Page = Product == EKioskProduct::DayPass ? EKioskPage::Review : EKioskPage::Route;
    return true;
}

bool UTransitKioskSession::SelectRoute(int32 InOrigin, int32 InDestination)
{
    if (Page != EKioskPage::Route || !Config.Stations.IsValidIndex(InOrigin) || !Config.Stations.IsValidIndex(InDestination)) return false;
    Origin = InOrigin; Destination = InDestination; return true;
}

bool UTransitKioskSession::CanReview() const
{
    // Keep the paid receipt readable, while rejecting any further day-pass payment.
    if (Product == EKioskProduct::DayPass) return !bDayPassPurchased || Page == EKioskPage::Success;
    if (Product == EKioskProduct::TopUp) return Config.TopUpAmountsCents.Contains(TopUpCents) && CardBalanceCents <= MAX_int32 - TopUpCents;
    return (Product == EKioskProduct::OneWay || Product == EKioskProduct::RoundTrip)
        && Origin != Destination && Config.Stations.IsValidIndex(Origin) && Config.Stations.IsValidIndex(Destination);
}

bool UTransitKioskSession::ReviewRoute()
{
    if (Page != EKioskPage::Route || !CanReview()) return false;
    Page = EKioskPage::Review; return true;
}

bool UTransitKioskSession::BeginTopUp()
{
    if (Page != EKioskPage::MainMenu) return false;
    Product = EKioskProduct::TopUp; TopUpCents = 0; ReceiptId.Reset();
    Page = EKioskPage::TopUp; return true;
}

bool UTransitKioskSession::SelectTopUp(int32 AmountCents)
{
    if (Page != EKioskPage::TopUp || !Config.TopUpAmountsCents.Contains(AmountCents) || CardBalanceCents > MAX_int32 - AmountCents) return false;
    TopUpCents = AmountCents;
    if (!CanReview()) return false;
    Page = EKioskPage::Review; return true;
}

int32 UTransitKioskSession::GetTotalCents() const
{
    if (!CanReview()) return 0;
    if (Product == EKioskProduct::DayPass) return Config.DayPassCents;
    if (Product == EKioskProduct::TopUp) return TopUpCents;
    const int32 Stops = FMath::Abs(Origin - Destination);
    return (Config.OneWayBaseCents + FMath::Max(0, Stops - 1) * Config.AdditionalStopCents)
        * (Product == EKioskProduct::RoundTrip ? 2 : 1);
}

bool UTransitKioskSession::ProceedToPayment()
{
    if (Page != EKioskPage::Review || !CanReview() || GetTotalCents() <= 0) return false;
    Page = EKioskPage::Payment; return true;
}

bool UTransitKioskSession::StartPayment()
{
    if (Page != EKioskPage::Payment || !CanReview()) return false;
    Page = EKioskPage::Processing; return true;
}

bool UTransitKioskSession::CompletePayment(const FDateTime& Now)
{
    if (Page != EKioskPage::Processing || !CanReview()) return false;
    if (Product == EKioskProduct::TopUp) CardBalanceCents += TopUpCents;
    if (Product == EKioskProduct::DayPass) bDayPassPurchased = true;
    ValidUntil = NextMidnight(Now);
    ReceiptId = TEXT("DEMO-") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8).ToUpper();
    Page = EKioskPage::Success; return true;
}

bool UTransitKioskSession::Back()
{
    switch (Page)
    {
    case EKioskPage::Language: Page = EKioskPage::Welcome; break;
    case EKioskPage::MainMenu: Page = EKioskPage::Language; break;
    case EKioskPage::TicketType: Page = EKioskPage::MainMenu; break;
    case EKioskPage::Route: Page = EKioskPage::TicketType; break;
    case EKioskPage::Review:
        Page = Product == EKioskProduct::TopUp ? EKioskPage::TopUp : Product == EKioskProduct::DayPass ? EKioskPage::TicketType : EKioskPage::Route;
        break;
    case EKioskPage::TopUp: Page = EKioskPage::MainMenu; break;
    case EKioskPage::Payment: Page = EKioskPage::Review; break;
    default: return false;
    }
    return true;
}

FDateTime UTransitKioskSession::NextMidnight(const FDateTime& Now) { return Now.GetDate() + FTimespan::FromDays(1); }
FString UTransitKioskSession::Money(int32 Cents) const { return FString::Printf(TEXT("%s %d.%02d"), *Config.Currency, Cents / 100, Cents % 100); }
FString UTransitKioskSession::ProductName() const
{
    switch (Product)
    {
    case EKioskProduct::OneWay: return TEXT("One-way ticket");
    case EKioskProduct::RoundTrip: return TEXT("Round-trip ticket");
    case EKioskProduct::DayPass: return TEXT("Day pass");
    case EKioskProduct::TopUp: return TEXT("Transit card top-up");
    default: return TEXT("Select a ticket");
    }
}
FString UTransitKioskSession::ValidityText() const
{
    if (Product == EKioskProduct::DayPass)
        return TEXT("Unlimited travel until midnight, ") + (ValidUntil.GetTicks() ? ValidUntil : NextMidnight(FDateTime::Now())).ToString(TEXT("%Y-%m-%d %H:%M")) + TEXT(" (local time)");
    if (Product == EKioskProduct::RoundTrip) return TEXT("One outward journey and one return journey today");
    if (Product == EKioskProduct::TopUp) return TEXT("Demo card credit is retained for this play session");
    return TEXT("One journey on the selected route today");
}
