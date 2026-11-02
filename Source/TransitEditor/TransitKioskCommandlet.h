#pragma once
#include "Commandlets/Commandlet.h"
#include "TransitKioskCommandlet.generated.h"

/** Inspect, extend and verify the existing kiosk assets through supported editor APIs. */
UCLASS()
class UTransitKioskCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UTransitKioskCommandlet();
    virtual int32 Main(const FString& Params) override;
};
