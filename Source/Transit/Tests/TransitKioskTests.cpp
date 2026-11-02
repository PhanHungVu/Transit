#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "TransitKioskSession.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitKioskTransactionTest, "Transit.Kiosk.Transactions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTransitKioskTransactionTest::RunTest(const FString& Parameters)
{
    auto* Session = NewObject<UTransitKioskSession>();
    const FDateTime Now(2026, 11, 2, 23, 59, 58);
    for (auto Product : {EKioskProduct::OneWay, EKioskProduct::RoundTrip, EKioskProduct::DayPass})
    {
        Session->Configure(FTransitKioskConfig(), 750);
        TestFalse(TEXT("Cannot pay from Welcome"), Session->StartPayment());
        TestTrue(TEXT("Start"), Session->Start());
        TestFalse(TEXT("Unknown language rejected"), Session->SelectLanguage(TEXT("Unknown")));
        TestTrue(TEXT("Choose language"), Session->SelectLanguage(TEXT("English")));
        TestTrue(TEXT("Purchase"), Session->BeginPurchase());
        TestTrue(TEXT("Choose ticket"), Session->SelectProduct(Product));
        if (Product != EKioskProduct::DayPass)
        {
            TestTrue(TEXT("Set same station"), Session->SelectRoute(0, 0));
            TestFalse(TEXT("Same station cannot proceed"), Session->ReviewRoute());
            TestFalse(TEXT("Out of range station rejected"), Session->SelectRoute(-1, 5));
            TestTrue(TEXT("Central to Airport"), Session->SelectRoute(0, 3));
            TestTrue(TEXT("Review route"), Session->ReviewRoute());
        }
        const int32 Expected = Product == EKioskProduct::OneWay ? 350 : Product == EKioskProduct::RoundTrip ? 700 : 1000;
        TestEqual(TEXT("Fare in cents"), Session->GetTotalCents(), Expected);
        TestTrue(TEXT("Go to Payment"), Session->ProceedToPayment());
        TestTrue(TEXT("Payment Back"), Session->Back());
        TestEqual(TEXT("Return to Review"), Session->Page, EKioskPage::Review);
        TestTrue(TEXT("Return to Payment"), Session->ProceedToPayment());
        TestTrue(TEXT("Tap once"), Session->StartPayment());
        TestFalse(TEXT("Duplicate tap rejected"), Session->StartPayment());
        TestFalse(TEXT("No Back during processing"), Session->Back());
        TestTrue(TEXT("Complete simulation"), Session->CompletePayment(Now));
        TestFalse(TEXT("Cannot complete twice"), Session->CompletePayment(Now));
        TestFalse(TEXT("Receipt assigned"), Session->ReceiptId.IsEmpty());
        TestEqual(TEXT("Next local midnight"), Session->ValidUntil, FDateTime(2026, 11, 3));
        TestEqual(TEXT("Ticket does not credit card"), Session->CardBalanceCents, 750);
    }
    for (int32 Amount : {500, 1000, 2000})
    {
        Session->Configure(FTransitKioskConfig(), 750);
        Session->Start(); Session->SelectLanguage(TEXT("Malay")); Session->BeginTopUp();
        TestFalse(TEXT("Unlisted top-up rejected"), Session->SelectTopUp(999));
        TestTrue(TEXT("Listed top-up"), Session->SelectTopUp(Amount));
        TestTrue(TEXT("Review Back to Top-Up"), Session->Back());
        TestEqual(TEXT("Top-Up page index"), static_cast<int32>(Session->Page), 6);
        Session->SelectTopUp(Amount); Session->ProceedToPayment(); Session->StartPayment();
        TestEqual(TEXT("Balance unchanged before approval"), Session->CardBalanceCents, 750);
        Session->CompletePayment(Now);
        TestEqual(TEXT("Credit added once"), Session->CardBalanceCents, 750 + Amount);
        Session->CompletePayment(Now);
        TestEqual(TEXT("Duplicate completion cannot add credit"), Session->CardBalanceCents, 750 + Amount);
        Session->Reset();
        TestEqual(TEXT("Welcome reset preserves paid balance"), Session->CardBalanceCents, 750 + Amount);
        TestEqual(TEXT("Reset returns Welcome"), Session->Page, EKioskPage::Welcome);
    }
    TestEqual(TEXT("Year boundary"), UTransitKioskSession::NextMidnight(FDateTime(2026, 12, 31, 23, 59)), FDateTime(2027, 1, 1));

    Session->Configure(FTransitKioskConfig(), 750);
    Session->Start(); Session->SelectLanguage(TEXT("English")); Session->BeginPurchase();
    Session->SelectProduct(EKioskProduct::DayPass); Session->ProceedToPayment(); Session->StartPayment();
    TestFalse(TEXT("Pending payment does not grant day pass"), Session->bDayPassPurchased);
    TestTrue(TEXT("Day pass payment succeeds"), Session->CompletePayment(Now));
    TestTrue(TEXT("Completed purchase grants day pass"), Session->bDayPassPurchased);
    TestEqual(TEXT("Owned pass receipt retains fare"), Session->GetTotalCents(), 1000);
    Session->Reset(); Session->Start(); Session->SelectLanguage(TEXT("English")); Session->BeginPurchase();
    TestFalse(TEXT("Cannot buy second day pass after Welcome reset"), Session->SelectProduct(EKioskProduct::DayPass));
    TestEqual(TEXT("Rejected day pass stays at Ticket Type"), Session->Page, EKioskPage::TicketType);
    TestTrue(TEXT("Other tickets remain purchasable with day pass"), Session->SelectProduct(EKioskProduct::OneWay));
    Session->Configure(FTransitKioskConfig(), 750, true);
    Session->Start(); Session->SelectLanguage(TEXT("English")); Session->BeginPurchase();
    TestFalse(TEXT("Restored ownership blocks day pass in a reopened kiosk"), Session->SelectProduct(EKioskProduct::DayPass));
    Session->Product = EKioskProduct::DayPass; Session->Page = EKioskPage::Review;
    TestFalse(TEXT("Owned pass cannot bypass guard through payment page"), Session->ProceedToPayment());
    Session->Page = EKioskPage::Processing;
    TestFalse(TEXT("Owned pass cannot complete another payment"), Session->CompletePayment(Now));
    Session->Configure(FTransitKioskConfig(), 750);
    Session->Start(); Session->SelectLanguage(TEXT("English")); Session->BeginPurchase();
    TestTrue(TEXT("Fresh play session can buy a day pass"), Session->SelectProduct(EKioskProduct::DayPass));
    return true;
}
#endif
