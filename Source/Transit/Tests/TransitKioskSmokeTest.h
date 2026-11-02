#pragma once
#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
class ATransitPlayerController;
/** Opt-in standalone gameplay check, only active with -KioskSmokeTest. */
void StartTransitKioskSmokeTest(ATransitPlayerController* Player);
#endif
