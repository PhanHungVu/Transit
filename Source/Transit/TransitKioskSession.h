#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TransitKioskSession.generated.h"

UENUM(BlueprintType)
enum class EKioskPage : uint8
{
    Welcome,
    Language,
    MainMenu,
    TicketType,
    Route,
    CardPlacement,
    TopUp
};
UENUM(BlueprintType)
enum class EKioskPopup : uint8
{
    None,
    Trip,
    Payment,
    Processing,
    Success,
    Assistance,
    Quantity
};
UENUM(BlueprintType)
enum class EKioskProduct : uint8
{
    None,
    OneWay,
    RoundTrip,
    DayPass,
    TopUp
};

USTRUCT(BlueprintType)
struct TRANSIT_API FKioskStation
{
    GENERATED_BODY()
    FKioskStation() = default;
    FKioskStation(const FString &InCode, int32 InFare, FVector2D InPosition)
        : Code(InCode), OneWayCents(InFare), MapPosition(InPosition)
    {
    }
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station") FString Code;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station", meta = (ClampMin = "1", ClampMax = "100000"))
    int32 OneWayCents = 200;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Station") FVector2D MapPosition = FVector2D::ZeroVector;
};

/** Local demonstration data, with fares and map coordinates transcribed from System Design.pdf. */
USTRUCT(BlueprintType)
struct TRANSIT_API FTransitKioskConfig
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo")
    TArray<FKioskStation> Stations = {
        {TEXT("A"), 200, {104.0 / 384, 81.5 / 256}},  {TEXT("B"), 200, {190.0 / 384, 60.5 / 256}},
        {TEXT("C"), 200, {219.0 / 384, 94.5 / 256}},  {TEXT("D"), 250, {269.0 / 384, 101.5 / 256}},
        {TEXT("E"), 300, {333.0 / 384, 119.5 / 256}}, {TEXT("F"), 250, {250.0 / 384, 148.5 / 256}},
        {TEXT("G"), 400, {325.0 / 384, 194.5 / 256}}, {TEXT("H"), 200, {120.0 / 384, 167.5 / 256}}};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo", meta = (ClampMin = "1", ClampMax = "100000"))
    int32 DayPassCents = 1000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo")
    TArray<int32> TopUpAmountsCents = {
        500,
        1000,
        2000,
        3000,
        5000,
        10000
    };
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo", meta = (ClampMin = "0"))
    int32 InitialCardBalanceCents = 340;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo", meta = (ClampMin = "0.1", ClampMax = "30"))
    float ProcessingSeconds = 2.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo", meta = (ClampMin = "1", ClampMax = "20"))
    int32 MaxTicketsPerPurchase = 10;
};

/** The single authority for the underlying screen, modal stack and transaction. */
UCLASS(BlueprintType)
class TRANSIT_API UTransitKioskSession : public UObject
{
    GENERATED_BODY()
  public:
    void Configure(const FTransitKioskConfig &InConfig, int32 BalanceCents);
    bool Start();
    bool SelectLanguage(const FString &InLanguage);
    bool BeginPurchase();
    bool BeginDestination();
    bool SelectDestination(int32 Index);
    bool ConfirmDestination();
    bool SelectProduct(EKioskProduct InProduct);
    bool SetTicketQuantity(int32 Quantity);
    bool ConfirmTicketQuantity();
    bool BeginTopUp();
    bool ReadCard();
    bool SelectTopUp(int32 AmountCents);
    bool StartPayment();
    bool CompletePayment(const FDateTime &Now);
    bool Back();
    bool ShowAssistance();
    bool DismissAssistance();
    bool CompleteAndReset();
    bool CollectTicket();
    bool CollectCard();
    bool RequestCardCollection();
    bool HasPendingCollection() const { return bTicketAwaitingCollection || bCardAwaitingCollection; }
    void Reset();
    bool CanConfirmDestination() const;
    bool CanPay() const;
    int32 GetOneWayCents() const;
    int32 GetTotalCents() const;
    int32 GetUnitPriceCents() const;
    FString Money(int32 Cents) const;
    FString ProductName() const;
    FString StationName() const;
    FString ValidityText() const;
    static FDateTime NextMidnight(const FDateTime &Now);

    UPROPERTY(BlueprintReadOnly, Category = "Kiosk") EKioskPage Page = EKioskPage::Welcome;
    UPROPERTY(BlueprintReadOnly, Category = "Kiosk") EKioskPopup Popup = EKioskPopup::None;
    UPROPERTY(BlueprintReadOnly, Category = "Kiosk") EKioskPopup PopupBeforeAssistance = EKioskPopup::None;
    UPROPERTY(BlueprintReadOnly, Category = "Kiosk") EKioskProduct Product = EKioskProduct::None;
    UPROPERTY(BlueprintReadOnly, Category = "Kiosk") FString Language = TEXT("English");
    UPROPERTY(BlueprintReadOnly, Category = "Kiosk") int32 Destination = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly, Category = "Kiosk") int32 TopUpCents = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Kiosk") int32 CardBalanceCents = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Kiosk") bool bCardPresent = false;
    UPROPERTY(BlueprintReadOnly, Category = "Kiosk") int32 TicketQuantity = 1;
    UPROPERTY(BlueprintReadOnly, Category = "Kiosk") bool bTicketAwaitingCollection = false;
    UPROPERTY(BlueprintReadOnly, Category = "Kiosk") bool bCardAwaitingCollection = false;
    UPROPERTY(BlueprintReadOnly, Category = "Kiosk") FString ReceiptId;
    UPROPERTY(BlueprintReadOnly, Category = "Kiosk") FDateTime ValidUntil;
    UPROPERTY(BlueprintReadOnly, Category = "Kiosk") FTransitKioskConfig Config;

  private:
    int32 PaidTotalCents = 0;
};
