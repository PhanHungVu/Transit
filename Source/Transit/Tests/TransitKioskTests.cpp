#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "TransitKioskSession.h"
#include "TransitKioskText.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransitKioskTransactionTest, "Transit.Kiosk.Transactions",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTransitKioskTransactionTest::RunTest(const FString &)
{
    auto *S = NewObject<UTransitKioskSession>();
    const FDateTime Now(2026, 12, 31, 23, 59, 58);
    const int32 Fares[] = {200, 200, 200, 250, 300, 250, 400, 200};
    const FString Languages[] = {TEXT("English"), TEXT("Chinese"), TEXT("Malay"), TEXT("Tamil")};
    for (const auto &Language : Languages)
        for (int32 Station = 0; Station < 8; ++Station)
            for (auto Product : {EKioskProduct::OneWay, EKioskProduct::RoundTrip})
            {
                S->Configure(FTransitKioskConfig(), 340);
                TestFalse(TEXT("Pay blocked at Welcome"), S->StartPayment());
                TestTrue(TEXT("Start"), S->Start());
                TestFalse(TEXT("Unknown language rejected"), S->SelectLanguage(TEXT("Other")));
                TestTrue(TEXT("Select language"), S->SelectLanguage(Language));
                TestTrue(TEXT("Purchase"), S->BeginPurchase());
                TestTrue(TEXT("Destination screen"), S->BeginDestination());
                TestFalse(TEXT("Unselected destination cannot confirm"), S->ConfirmDestination());
                TestFalse(TEXT("Invalid destination rejected"), S->SelectDestination(8));
                S->SelectDestination((Station + 1) % 8);
                TestTrue(TEXT("Change destination"), S->SelectDestination(Station));
                TestTrue(TEXT("Confirm station"), S->ConfirmDestination());
                TestTrue(TEXT("Cancel trip"), S->Back());
                TestEqual(TEXT("Cancel retains station"), S->Destination, Station);
                TestEqual(TEXT("Cancel retains language"), S->Language, Language);
                S->ConfirmDestination();
                TestTrue(TEXT("Choose trip"), S->SelectProduct(Product));
                TestEqual(TEXT("Quantity precedes payment"),S->Popup,EKioskPopup::Quantity);
                TestFalse(TEXT("Cannot pay before quantity confirmation"),S->StartPayment());
                TestFalse(TEXT("Quantity cannot be zero"),S->SetTicketQuantity(0));
                TestFalse(TEXT("Quantity cannot exceed limit"),S->SetTicketQuantity(S->Config.MaxTicketsPerPurchase+1));
                TestEqual(TEXT("Correct PDF fare"), S->GetTotalCents(),
                          Product == EKioskProduct::RoundTrip ? Fares[Station] * 3 / 2 : Fares[Station]);
                TestTrue(TEXT("Confirm default quantity"),S->ConfirmTicketQuantity());
                TestTrue(TEXT("Cancel payment returns quantity"), S->Back());
                TestEqual(TEXT("Quantity restored for editing"),S->Popup,EKioskPopup::Quantity);
                S->Back();
                S->Back();
                TestEqual(TEXT("Payment cancel stays at map"), S->Page, EKioskPage::Route);
                TestEqual(TEXT("Selected station retained"), S->Destination, Station);
                S->ConfirmDestination();
                S->SelectProduct(Product);
                TestTrue(TEXT("Choose three people"),S->SetTicketQuantity(3));
                TestEqual(TEXT("Total multiplies unit fare"),S->GetTotalCents(),S->GetUnitPriceCents()*3);
                S->ConfirmTicketQuantity();
                TestTrue(TEXT("Request help during payment"), S->ShowAssistance());
                TestFalse(TEXT("Help cannot double stack"), S->ShowAssistance());
                TestFalse(TEXT("No payment through help"), S->StartPayment());
                TestTrue(TEXT("Dismiss help"), S->DismissAssistance());
                TestEqual(TEXT("Restore payment popup"), S->Popup, EKioskPopup::Payment);
                TestTrue(TEXT("Tap card"), S->StartPayment());
                TestFalse(TEXT("Duplicate tap rejected"), S->StartPayment());
                TestFalse(TEXT("Processing cannot go Back"), S->Back());
                S->ShowAssistance();
                TestTrue(TEXT("Payment completes behind help"), S->CompletePayment(Now));
                TestEqual(TEXT("Immediate collection takes priority over help"), S->Popup, EKioskPopup::Success);
                TestFalse(TEXT("Cannot pay twice"), S->CompletePayment(Now));
                TestFalse(TEXT("Receipt populated"), S->ReceiptId.IsEmpty());
                TestEqual(TEXT("Ticket does not credit card"), S->CardBalanceCents, 340);
                TestEqual(TEXT("Language retained through success"), S->Language, Language);
                TestTrue(TEXT("Paid ticket awaits physical collection"), S->bTicketAwaitingCollection);
                TestFalse(TEXT("Cannot finish before collection"), S->CompleteAndReset());
                const FString PaidReceipt=S->ReceiptId;
                S->Reset();
                TestEqual(TEXT("Reset cannot erase uncollected ticket"),S->ReceiptId,PaidReceipt);
                TestFalse(TEXT("Help locked before collection"),S->ShowAssistance());
                TestFalse(TEXT("Back locked before collection"),S->Back());
                TestTrue(TEXT("Physical ticket collection"),S->CollectTicket());
                TestFalse(TEXT("Cannot collect ticket twice"),S->CollectTicket());
                TestTrue(TEXT("Help restored after collection"),S->ShowAssistance());
                S->DismissAssistance();
                TestTrue(TEXT("Explicit completion after pickup"), S->CompleteAndReset());
                TestEqual(TEXT("Back at Welcome"), S->Page, EKioskPage::Welcome);
                TestEqual(TEXT("Next customer starts with one ticket"),S->TicketQuantity,1);
            }
    for (const auto &Language : Languages)
        for (int32 Amount : {500, 1000, 2000, 3000, 5000, 10000})
        {
            S->Configure(FTransitKioskConfig(), 340);
            S->Start();
            S->SelectLanguage(Language);
            TestTrue(TEXT("Top-up begins with card placement"), S->BeginTopUp());
            TestEqual(TEXT("Card placement page"), S->Page, EKioskPage::CardPlacement);
            TestFalse(TEXT("No amount before card"), S->SelectTopUp(Amount));
            TestTrue(TEXT("Read card"), S->ReadCard());
            TestTrue(TEXT("Back requests card collection"), S->Back());
            TestTrue(TEXT("Unpaid card is retained for pickup"),S->bCardPresent && S->bCardAwaitingCollection);
            TestFalse(TEXT("Cannot reuse reader before collecting"),S->ReadCard());
            TestFalse(TEXT("Cannot choose another amount before collecting"),S->SelectTopUp(Amount));
            TestTrue(TEXT("Unpaid card collection"),S->CollectCard());
            TestTrue(TEXT("Card placement can be repeated after pickup"), S->ReadCard());
            TestFalse(TEXT("Unlisted amount rejected"), S->SelectTopUp(999));
            TestTrue(TEXT("Six supported amounts"), S->SelectTopUp(Amount));
            TestTrue(TEXT("Cancel before charge"), S->Back());
            TestEqual(TEXT("Cancelled balance unchanged"), S->CardBalanceCents, 340);
            TestEqual(TEXT("Amount retained"), S->TopUpCents, Amount);
            S->SelectTopUp(Amount);
            S->StartPayment();
            TestEqual(TEXT("Unconfirmed balance"), S->CardBalanceCents, 340);
            S->CompletePayment(Now);
            TestEqual(TEXT("Credit once"), S->CardBalanceCents, 340 + Amount);
            TestFalse(TEXT("Top-up has no printed ticket to collect"),S->bTicketAwaitingCollection);
            TestTrue(TEXT("Approved top-up retains card for pickup"),S->bCardPresent && S->bCardAwaitingCollection);
            TestFalse(TEXT("Top-up completion locked until card pickup"),S->CompleteAndReset());
            S->Reset();
            TestTrue(TEXT("Reset cannot discard card"),S->bCardPresent);
            TestFalse(TEXT("Help locked until card pickup"),S->ShowAssistance());
            S->CompletePayment(Now);
            TestEqual(TEXT("No duplicate credit"), S->CardBalanceCents, 340 + Amount);
            TestTrue(TEXT("Approved card collection"),S->CollectCard());
            TestFalse(TEXT("Cannot collect card twice"),S->CollectCard());
            S->CompleteAndReset();
            TestEqual(TEXT("Balance retained after completion"), S->CardBalanceCents, 340 + Amount);
        }
    S->Configure(FTransitKioskConfig(), 340);
    S->Start();
    S->SelectLanguage(TEXT("Tamil"));
    S->BeginPurchase();
    TestTrue(TEXT("Day pass opens quantity"), S->SelectProduct(EKioskProduct::DayPass));
    S->SetTicketQuantity(2);
    S->ConfirmTicketQuantity();
    TestEqual(TEXT("Two day passes fare"), S->GetTotalCents(), 2000);
    S->StartPayment();
    S->CompletePayment(Now);
    TestEqual(TEXT("Midnight / year rollover"), S->ValidUntil, FDateTime(2027, 1, 1));
    TestEqual(TEXT("Paid receipt total"), S->GetTotalCents(), 2000);
    TestFalse(TEXT("Day pass cannot finish before pickup"),S->CompleteAndReset());
    TestTrue(TEXT("Printed day pass can be collected"),S->CollectTicket());
    S->CompleteAndReset();
    S->Start();
    S->SelectLanguage(TEXT("English"));
    S->BeginPurchase();
    TestTrue(TEXT("Next customer may purchase another day pass"), S->SelectProduct(EKioskProduct::DayPass));
    S->ConfirmTicketQuantity(); S->StartPayment(); S->CompletePayment(Now); S->CollectTicket(); S->CompleteAndReset();
    S->Configure(FTransitKioskConfig(), 340);
    S->Start();
    S->SelectLanguage(TEXT("English"));
    S->BeginPurchase();
    TestTrue(TEXT("Reopening permits another day pass"), S->SelectProduct(EKioskProduct::DayPass));
    S->Configure(FTransitKioskConfig(), 340);
    S->Start();
    S->SelectLanguage(TEXT("English"));
    S->BeginPurchase();
    TestTrue(TEXT("Fresh session permits pass"), S->SelectProduct(EKioskProduct::DayPass));
    for (const auto &Language : Languages)
        for (FName Key :
             {TEXT("Payment"), TEXT("Success"), TEXT("HelpTitle"), TEXT("PlaceCard"), TEXT("DestinationTitle"), TEXT("CollectCardTitle"), TEXT("CollectCardInstruction"), TEXT("CollectCardHint")})
            TestFalse(TEXT("Localization entry exists"), KioskText(Language, Key) == Key.ToString());
    return true;
}
#endif
