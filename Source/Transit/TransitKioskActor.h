#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TransitKioskSession.h"
#include "TransitKioskActor.generated.h"

UCLASS()
class TRANSIT_API ATransitKioskActor : public AActor
{
	GENERATED_BODY()
	
public:	
	ATransitKioskActor();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Kiosk")
    FTransitKioskConfig DemoConfig;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Kiosk", meta=(ClampMin="50", ClampMax="1000", Units="cm"))
    float InteractionDistance = 250.f;
    bool CanInteract(const APlayerController* Player) const;
};
