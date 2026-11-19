#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "TransitPlayerController.h"
#include "TransitKioskActor.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Sound/SoundBase.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

/** Records each configured cue through the real mixer; PCM validation follows the run. */
struct FTransitKioskAudioCheck
{
    TWeakObjectPtr<ATransitPlayerController> Player;
    TWeakObjectPtr<ATransitKioskActor> Kiosk;
    int32 Cue=0, Phase=0, Failures=0;
    double Next=0, WriteDeadline=0;
    float OriginalUnfocusedVolume=1;
    FString Report, Name, Folder;
    void Check(bool Good,const FString& Label)
    {
        Report+=(Good ? TEXT("PASS ") : TEXT("FAIL "))+Label+TEXT("\n");
        if (!Good) ++Failures;
        UE_LOG(LogTemp,Display,TEXT("KIOSK_AUDIO %s %s"),Good ? TEXT("PASS") : TEXT("FAIL"),*Label);
    }
    bool Tick(float)
    {
        if (!Player.IsValid()) return false;
        const double Now=FPlatformTime::Seconds();
        if (Now<Next) return true;
        if (!Kiosk.IsValid())
        {
            for (TActorIterator<ATransitKioskActor> It(Player->GetWorld());It;++It) {Kiosk=*It;break;}
            Check(Kiosk.IsValid(),TEXT("Existing kiosk available for mixer test"));
            if (!Kiosk.IsValid()) Cue=7;
            Folder=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("KioskAudioPreview")/FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
            Report+=TEXT("Folder: ")+Folder+TEXT("\n");
            IFileManager::Get().MakeDirectory(*Folder,true);
            OriginalUnfocusedVolume=FApp::GetUnfocusedVolumeMultiplier();
            FApp::SetUnfocusedVolumeMultiplier(1.f);
        }
        if (Cue>=7)
        {
            FApp::SetUnfocusedVolumeMultiplier(OriginalUnfocusedVolume);
            Report+=FString::Printf(TEXT("Failures: %d\n"),Failures);
            FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("KioskAudioMixerVerification.txt")));
            FPlatformMisc::RequestExitWithStatus(false,Failures ? 1 : 0);
            return false;
        }
        const auto Event=static_cast<EKioskSound>(Cue);
        auto* Sound=Kiosk->GetKioskSound(Event);
        if (Phase==0)
        {
            Name=StaticEnum<EKioskSound>()->GetNameStringByValue(Cue);
            Check(Sound && Sound->GetDuration()>0,TEXT("Configured sound ")+Name);
            if (!Sound) {++Cue;return true;}
            UAudioMixerBlueprintLibrary::StartRecordingOutput(Player.Get(),Sound->GetDuration()+1.f);
            Phase=1; Next=Now+.2;
        }
        else if (Phase==1)
        {
            const int32 Before=Kiosk->GetSoundEventCount(Event);
            Kiosk->PlayKioskSound(Event);
            Check(Kiosk->GetSoundEventCount(Event)==Before+1,TEXT("Single playback request ")+Name);
            Phase=2; Next=Now+FMath::Clamp(Sound->GetDuration(),.05f,30.f)+.4;
        }
        else if (Phase==2)
        {
            UAudioMixerBlueprintLibrary::StopRecordingOutput(Player.Get(),EAudioRecordingExportType::WavFile,Name,Folder);
            Phase=3; WriteDeadline=Now+8; Next=Now+.1;
        }
        else
        {
            const bool Written=IFileManager::Get().FileSize(*(Folder/(Name+TEXT(".wav"))))>44;
            if (!Written && Now<WriteDeadline) {Next=Now+.1;return true;}
            Check(Written,TEXT("Mixer WAV exported ")+Name);
            Phase=0; ++Cue; Next=Now+.15;
        }
        return true;
    }
};
void StartTransitKioskAudioTest(ATransitPlayerController* Player)
{
    auto Test=MakeShared<FTransitKioskAudioCheck>();
    Test->Player=Player;
    Test->Next=FPlatformTime::Seconds()+1;
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Test](float Delta){return Test->Tick(Delta);}));
}
#endif
