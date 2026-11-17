#include "TransitKioskSmokeTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "TransitPlayerController.h"
#include "TransitKioskActor.h"
#include "TransitKioskWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "ShaderCompiler.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "TransitCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
void StartTransitKioskPOVTest(ATransitPlayerController* Player);
void StartTransitKioskSmokeTest(ATransitPlayerController *Player)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("KioskSmokeTest")))
        return;
    if (FParse::Param(FCommandLine::Get(),TEXT("KioskScaleAudit")))
    {
        auto Step=MakeShared<int32>(0);
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Player,Step](float)
        {
            auto* Human=Cast<ATransitCharacter>(Player->GetPawn());
            ATransitKioskActor* Machine=nullptr;
            for(TActorIterator<ATransitKioskActor> It(Player->GetWorld());It;++It){Machine=*It;break;}
            if(!Human || !Machine) return true;
            const auto Transform=Machine->GetActorTransform();
            if((*Step)++==0)
            {
                Human->SetActorLocation(Transform.TransformPosition(FVector(0,120,Human->GetCapsuleComponent()->GetScaledCapsuleHalfHeight())),false,nullptr,ETeleportType::TeleportPhysics);
                Player->SetControlRotation(Machine->GetActorRotation());
                Human->SetActorRotation(Machine->GetActorRotation());
                Human->GetMesh()->SetOwnerNoSee(false);
                Human->GetFirstPersonMesh()->SetVisibility(false);
                const auto Position=Transform.TransformPosition(FVector(430,60,105));
                const auto Target=Transform.TransformPosition(FVector(0,60,100));
                auto* Camera=Player->GetWorld()->SpawnActor<ACameraActor>(Position,(Target-Position).Rotation());
                Camera->GetCameraComponent()->SetFieldOfView(50);
                Camera->GetCameraComponent()->SetConstraintAspectRatio(false);
                Player->SetViewTarget(Camera);
                if(GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
            }
            if(*Step==4)
            {
                Human->GetMesh()->UpdateBounds();
                const float Floor=Human->GetActorLocation().Z-Human->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
                auto* Body=Machine->FindPart(TEXT("MachineBase"));
                FString Report=FString::Printf(TEXT("Character=%s\nActorScale=%s\nCapsuleHeightCm=%.2f\nMeshAsset=%s\nMeshBoundsHeightCm=%.2f\nEyeHeightCm=%.2f\nHeadBoneHeightCm=%.2f\nFootBoneHeightCm=%.2f\nKioskBodyHeightCm=%.2f\nScreenCentreHeightCm=%.2f\n"),
                    *Human->GetClass()->GetPathName(),*Human->GetActorScale3D().ToString(),Human->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*2,
                    *GetPathNameSafe(Human->GetMesh()->GetSkeletalMeshAsset()),Human->GetMesh()->Bounds.BoxExtent.Z*2,
                    Human->GetFirstPersonCameraComponent()->GetComponentLocation().Z-Floor,
                    Human->GetMesh()->GetBoneLocation(TEXT("head")).Z-Floor,Human->GetMesh()->GetBoneLocation(TEXT("foot_l")).Z-Floor,
                    Body->Bounds.BoxExtent.Z*2,Machine->FindPart(TEXT("ScreenDisplay"))->GetComponentLocation().Z-Machine->GetActorLocation().Z);
                FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("KioskScaleMeasurements.txt")));
                FString Filename=TEXT("KioskScaleComparison.png");
                FParse::Value(FCommandLine::Get(),TEXT("ScaleScreenshot="),Filename);
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FPaths::GetCleanFilename(Filename),false,false);
            }
            if(*Step>6){FPlatformMisc::RequestExitWithStatus(false,0);return false;}
            return true;
        }),.5f);
        return;
    }
    if (!FParse::Param(FCommandLine::Get(), TEXT("KioskHardwarePreviewOnly")))
    {
        StartTransitKioskPOVTest(Player);
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("KioskHardwarePreviewOnly")))
    {
        auto Step = MakeShared<int32>(0);
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Player, Step](float)
        {
            if ((*Step)++ == 0)
            {
                for (TActorIterator<ATransitKioskActor> It(Player->GetWorld()); It; ++It)
                {
                    const auto Transform = It->GetActorTransform();
                    const auto Position = Transform.TransformPosition(FVector(260, -220, 180));
                    const auto Target = Transform.TransformPosition(FVector(0, 0, 85));
                    auto *Camera = Player->GetWorld()->SpawnActor<ACameraActor>(Position, (Target-Position).Rotation());
                    Camera->GetCameraComponent()->SetFieldOfView(50);
                    Player->SetViewTarget(Camera);
                    Player->GetPawn()->SetActorHiddenInGame(true);
                    Player->GetPawn()->SetActorLocation(Transform.TransformPosition(FVector(1000,0,100)));
                    if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
                    break;
                }
                return true;
            }
            if (*Step == 4)
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("KioskPOVHardware.png"), false, false);
            if (*Step > 6)
            {
                FPlatformMisc::RequestExitWithStatus(false, 0);
                return false;
            }
            return true;
        }), .5f);
        return;
    }
}
#endif
