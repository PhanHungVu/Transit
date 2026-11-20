// Copyright Epic Games, Inc. All Rights Reserved.


#include "TransitCameraManager.h"
#include "TransitPlayerController.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

ATransitCameraManager::ATransitCameraManager()
{
	// set the min/max pitch
	ViewPitchMin = -70.0f;
	ViewPitchMax = 80.0f;
}

void ATransitCameraManager::UpdateViewTargetInternal(FTViewTarget& OutVT, float DeltaTime)
{
    Super::UpdateViewTargetInternal(OutVT, DeltaTime);
    const auto* Player = Cast<ATransitPlayerController>(PCOwner);
    if (!Player || !Player->IsChildViewEnabled() || OutVT.Target != Player->GetPawn()) return;

    if (const auto* Character = Cast<ACharacter>(OutVT.Target))
    {
        const auto* Capsule = Character->GetCapsuleComponent();
        const auto* Movement = Character->GetCharacterMovement();
        const float FloorClearance = Movement->IsMovingOnGround() && Movement->CurrentFloor.IsWalkableFloor()
            ? Movement->CurrentFloor.GetDistanceToFloor() : 0.f;
        const double FloorZ = Capsule->GetComponentLocation().Z - Capsule->GetScaledCapsuleHalfHeight() - FloorClearance;
        OutVT.POV.Location.Z = FloorZ + ATransitPlayerController::ChildEyeHeightCm;
    }
}
