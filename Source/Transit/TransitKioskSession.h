#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TransitKioskSession.generated.h"

UENUM(BlueprintType)
enum class EKioskPage : uint8
{
    Welcome, Language, MainMenu, TicketType, Route, Review, TopUp, Payment, Processing, Success
};

UENUM(BlueprintType)
enum class EKioskProduct : uint8 { None, OneWay, RoundTrip, DayPass, TopUp };

/** All demonstration prices are integer cents. No payment services are contacted. */
USTRUCT(BlueprintType)
struct TRANSIT_API FTransitKioskConfig
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Demo")
    TArray<FString> Stations = {TEXT("Central"), TEXT("Museum"), TEXT("Harbour"), TEXT("Airport")};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Demo", meta=(ClampMin="1", ClampMax="100000"))
    int32 OneWayBaseCents = 250;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Demo", meta=(ClampMin="0", ClampMax="100000"))
    int32 AdditionalStopCents = 50;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Demo", meta=(ClampMin="1", ClampMax="100000"))
    int32 DayPassCents = 1000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, EditFixedSize, Category="Demo")
    TArray<int32> TopUpAmountsCents = {500, 1000, 2000};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Demo")
    FString Currency = TEXT("SGD");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Demo", meta=(ClampMin="0"))
    int32 InitialCardBalanceCents = 750;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Demo", meta=(ClampMin="0.1", ClampMax="30"))
    float ProcessingSeconds = 2.0f;
};

/** Transaction state shared by the existing UMG pages and the new pages. */
UCLASS(BlueprintType)
class TRANSIT_API UTransitKioskSession : public UObject
{
    GENERATED_BODY()
public:
    void Configure(const FTransitKioskConfig& InConfig, int32 BalanceCents, bool bInDayPassPurchased = false);
    bool Start();
    bool SelectLanguage(const FString& InLanguage);
    bool BeginPurchase();
    bool SelectProduct(EKioskProduct InProduct);
    bool SelectRoute(int32 InOrigin, int32 InDestination);
    bool ReviewRoute();
    bool BeginTopUp();
    bool SelectTopUp(int32 AmountCents);
    bool ProceedToPayment();
    bool StartPayment();
    bool CompletePayment(const FDateTime& Now);
    bool Back();
    void Reset();
    bool CanReview() const;
    int32 GetTotalCents() const;
    FString Money(int32 Cents) const;
    FString ProductName() const;
    FString ValidityText() const;
    static FDateTime NextMidnight(const FDateTime& Now);

    UPROPERTY(BlueprintReadOnly, Category="Kiosk") EKioskPage Page = EKioskPage::Welcome;
    UPROPERTY(BlueprintReadOnly, Category="Kiosk") EKioskProduct Product = EKioskProduct::None;
    UPROPERTY(BlueprintReadOnly, Category="Kiosk") FString Language = TEXT("English");
    UPROPERTY(BlueprintReadOnly, Category="Kiosk") int32 Origin = 0;
    UPROPERTY(BlueprintReadOnly, Category="Kiosk") int32 Destination = 1;
    UPROPERTY(BlueprintReadOnly, Category="Kiosk") int32 TopUpCents = 0;
    UPROPERTY(BlueprintReadOnly, Category="Kiosk") int32 CardBalanceCents = 0;
    /** Owned for this play session, independent of the current transaction. */
    UPROPERTY(BlueprintReadOnly, Category="Kiosk") bool bDayPassPurchased = false;
    UPROPERTY(BlueprintReadOnly, Category="Kiosk") FString ReceiptId;
    UPROPERTY(BlueprintReadOnly, Category="Kiosk") FDateTime ValidUntil;
    UPROPERTY(BlueprintReadOnly, Category="Kiosk") FTransitKioskConfig Config;
};
