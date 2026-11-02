#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TransitKioskSession.h"
#include "TransitKioskWidget.generated.h"

class UButton;
class UTransitKioskWidget;

/** Keeps standard UMG buttons editable while routing their clicks to one C++ handler. */
UCLASS()
class UTransitKioskButtonBinding : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<UButton> Button;
    UPROPERTY() TObjectPtr<UTransitKioskWidget> Owner;
    FName Action;
    UFUNCTION() void Click();
};

/** Native behavior for the preserved /Game/Kiosk/UI/KioskUI Widget Blueprint. */
UCLASS()
class TRANSIT_API UTransitKioskWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void InitializeKiosk(const FTransitKioskConfig& Config, int32 CardBalanceCents, bool bDayPassPurchased = false);
    void ShutdownKiosk();
    void HandleAction(FName Action);
    UTransitKioskSession* GetSession() const { return Session; }
    void Refresh();
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
private:
    void BindButton(FName WidgetName, FName Action);
    void UnbindButtons();
    void SetText(FName Name, const FString& Value);
    void ShowStaff(bool bShow);
    void FinishPayment();
    void StaffArrived();
    UFUNCTION() void RouteChanged(FString Selected, ESelectInfo::Type SelectionType);
    UPROPERTY(Transient) TObjectPtr<UTransitKioskSession> Session;
    UPROPERTY(Transient) TArray<TObjectPtr<UTransitKioskButtonBinding>> ButtonBindings;
    FTimerHandle PaymentTimer;
    FTimerHandle StaffTimer;
    bool bRefreshing = false;
    bool bStaffVisible = false;
    bool bStaffRequested = false;
    bool bShuttingDown = false;
};
