#include "TransitKioskSmokeTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "TransitPlayerController.h"
#include "TransitKioskActor.h"
#include "TransitKioskWidget.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
struct FKioskGameplayCheck
{
    TWeakObjectPtr<ATransitPlayerController> Player;
    int32 Step = 0, Failures = 0;
    float Elapsed = 0;
    FVector BeforeMove;
    FString Report;
    void Check(bool bPass, const FString& Name)
    {
        Report += (bPass ? TEXT("PASS ") : TEXT("FAIL ")) + Name + TEXT("\n");
        if (!bPass) ++Failures;
        UE_LOG(LogTemp, Display, TEXT("KIOSK_GAMEPLAY %s %s"), bPass ? TEXT("PASS") : TEXT("FAIL"), *Name);
    }
    void Key(FKey Key, EInputEvent Event)
    {
        FInputKeyEventArgs Args;
        Args.Key = Key; Args.Event = Event; Args.AmountDepressed = Event == IE_Released ? 0.f : 1.f;
        Player->InputKey(Args);
    }
    void Click(FName Name)
    {
        auto* Widget = Player->GetKioskWidget();
        auto* Button = Widget ? Cast<UButton>(Widget->GetWidgetFromName(Name)) : nullptr;
        Check(Button && Button->OnClicked.IsBound(), TEXT("Click ") + Name.ToString());
        if (Button) Button->OnClicked.Broadcast();
    }
    bool Finish()
    {
        Report += FString::Printf(TEXT("Failures: %d\n"), Failures);
        FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("KioskGameplayVerification.txt")));
        FPlatformMisc::RequestExitWithStatus(false, Failures ? 1 : 0);
        return false;
    }
    bool Tick(float Delta)
    {
        if (!Player.IsValid()) return false;
        Elapsed += Delta;
        const float Delay = Step == 4 || Step == 5 || Step == 10 ? 3.f : 0.65f;
        if (Elapsed < Delay) return true;
        Elapsed = 0;
        auto* PC = Player.Get();
        auto* World = PC->GetWorld();
        switch (Step++)
        {
        case 0:
        {
            PC->OpenKiosk(); Check(!PC->IsKioskOpen(), TEXT("Cannot open from distant spawn"));
            ATransitKioskActor* Kiosk = nullptr;
            for (TActorIterator<ATransitKioskActor> It(World); It; ++It) { Kiosk = *It; break; }
            Check(Kiosk != nullptr && PC->GetPawn() != nullptr, TEXT("Existing map contains native kiosk and first-person pawn"));
            if (!Kiosk || !PC->GetPawn()) return Finish();
            FVector Center, Extent; Kiosk->GetActorBounds(false, Center, Extent);
            const FVector Position(Center.X + 180, Center.Y, 100);
            PC->GetPawn()->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
            PC->SetControlRotation((Center - Position).Rotation());
            break;
        }
        case 1:
            Key(EKeys::E, IE_Pressed); break;
        case 2:
            Key(EKeys::E, IE_Released);
            Check(PC->IsKioskOpen(), TEXT("Existing Enhanced Input E mapping opens kiosk in range"));
            if (!PC->IsKioskOpen()) return Finish();
            Check(PC->IsMoveInputIgnored() && PC->IsLookInputIgnored() && PC->bShowMouseCursor, TEXT("UI captures movement/look and shows cursor"));
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("KioskGameplayWelcome.png"), true, false);
            break;
        case 3:
            Click(TEXT("BTN_Start")); Click(TEXT("BTN_LanguageEnglish")); Click(TEXT("BTN_Purchase")); Click(TEXT("BTN_DayPass"));
            Click(TEXT("BTN_ReviewProceed")); Click(TEXT("BTN_PayCard"));
            Check(PC->GetKioskWidget()->GetSession()->Page == EKioskPage::Processing, TEXT("Clickable ticket flow reaches Processing"));
            break;
        case 4:
            Check(PC->GetKioskWidget()->GetSession()->Page == EKioskPage::Success, TEXT("Real world timer reaches Success"));
            Check(PC->GetKioskWidget()->GetSession()->bDayPassPurchased, TEXT("Paid day pass is owned"));
            Click(TEXT("BTN_Help")); Click(TEXT("BTN_StaffRequest")); break;
        case 5:
            Check(CastChecked<UTextBlock>(PC->GetKioskWidget()->GetWidgetFromName(TEXT("TXT_StaffStatus")))->GetText().ToString().Contains(TEXT("ready to assist")), TEXT("Staff assistance completes in gameplay"));
            Click(TEXT("BTN_StaffClose")); Click(TEXT("BTN_ExitKiosk"));
            Check(!PC->IsKioskOpen() && !PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored() && !PC->bShowMouseCursor, TEXT("Exit restores movement/look/cursor"));
            BeforeMove = PC->GetPawn()->GetActorLocation(); Key(EKeys::S, IE_Pressed); break;
        case 6:
            Key(EKeys::S, IE_Released);
            Check(FVector::DistSquared(PC->GetPawn()->GetActorLocation(), BeforeMove) > 25.f, TEXT("First-person movement works after exit"));
            PC->GetPawn()->SetActorLocation(BeforeMove, false, nullptr, ETeleportType::TeleportPhysics);
            break;
        case 7:
            Key(EKeys::E, IE_Pressed); break;
        case 8:
            Key(EKeys::E, IE_Released); Check(PC->IsKioskOpen(), TEXT("Can reopen kiosk with E"));
            if (!PC->IsKioskOpen()) return Finish();
            Check(PC->GetKioskWidget()->GetSession()->bDayPassPurchased, TEXT("Day pass ownership survives Exit and E reopening"));
            Click(TEXT("BTN_Start")); Click(TEXT("BTN_LanguageEnglish")); Click(TEXT("BTN_Purchase"));
            Check(!CastChecked<UButton>(PC->GetKioskWidget()->GetWidgetFromName(TEXT("BTN_DayPass")))->GetIsEnabled(), TEXT("Reopened kiosk disables a second day pass"));
            PC->GetKioskWidget()->HandleAction(TEXT("DayPass"));
            Check(PC->GetKioskWidget()->GetSession()->Page == EKioskPage::TicketType, TEXT("Duplicate purchase is blocked by C++"));
            Click(TEXT("BTN_MainMenuBack_1")); Click(TEXT("BTN_TopUp")); Click(TEXT("BTN_TopUp1"));
            Click(TEXT("BTN_ReviewProceed")); Click(TEXT("BTN_PayCard"));
            Click(TEXT("BTN_ExitKiosk"));
            Check(!PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored(), TEXT("Exit during Processing restores input"));
            break;
        case 9: break;
        case 10:
            PC->OpenKiosk(); Check(PC->IsKioskOpen(), TEXT("Reopen after cancelling payment"));
            if (PC->IsKioskOpen())
            {
                Check(PC->GetKioskWidget()->GetSession()->CardBalanceCents == 750, TEXT("Cancelled top-up does not credit demo card"));
                Check(PC->GetKioskWidget()->GetSession()->bDayPassPurchased, TEXT("Other transactions do not reset purchased day pass"));
                PC->CloseKiosk();
            }
            return Finish();
        }
        return true;
    }
};
}

void StartTransitKioskSmokeTest(ATransitPlayerController* Player)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("KioskSmokeTest"))) return;
    auto Test = MakeShared<FKioskGameplayCheck>(); Test->Player = Player;
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Test](float Delta) { return Test->Tick(Delta); }));
}
#endif
