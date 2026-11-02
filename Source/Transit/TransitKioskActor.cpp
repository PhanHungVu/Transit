#include "TransitKioskActor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

// Sets default values
ATransitKioskActor::ATransitKioskActor()
{
	PrimaryActorTick.bCanEverTick = false;

}

bool ATransitKioskActor::CanInteract(const APlayerController* Player) const
{
    if (!Player || !Player->GetPawn()) return false;
    FVector Center, Extent;
    GetActorBounds(false, Center, Extent);
    if (FVector::DistSquared(Player->GetPawn()->GetActorLocation(), Center) > FMath::Square(InteractionDistance)) return false;
    FVector Eye; FRotator View;
    Player->GetPlayerViewPoint(Eye, View);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(KioskVisibility), false, Player->GetPawn());
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, Center, ECC_Visibility, Query) || Hit.GetActor() == this;
}

