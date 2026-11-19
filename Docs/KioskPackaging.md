# Windows Shipping packaging fix — 13 November 2026

> Current project, 19 November: [KioskAudio.md](KioskAudio.md) documents the supplied sound effects and current build validation. [KioskCustomerFeedback.md](KioskCustomerFeedback.md) covers repeat day passes, ticket quantity and animated Help. The launcher location is `Saved/Packaged/POVVerified/Windows/Transit.exe`; keep the complete Windows folder together. The compiler fix below remains in place.

The Shipping build failed in `TransitKioskSmokeTest.cpp`: its `#else` fallback used `ATransitPlayerController`, but the header's forward declaration exists only when `WITH_DEV_AUTOMATION_TESTS` is enabled. Development builds never compiled that fallback, so previous Development checks did not expose the error.

Removed the unused fallback. The test declaration, implementation and caller are now all excluded from Shipping consistently. No gameplay, UI or asset changes were needed.

Validation completed with the installed Unreal 5.8 engine:

- `Transit Win64 Shipping` compiled and linked successfully (`Saved/Logs/KioskShippingBuild.log`).
- Full Windows Shipping BuildCookRun, including cooking, staging, Pak/IoStore packaging, prerequisites and archiving, completed successfully with exit code 0 (`Saved/Logs/KioskShippingPackage.log`). Cooking reported zero errors and warnings.
- The verified package is `Saved/Packaged/ShippingVerified/Windows/Transit.exe`. Keep the entire `Windows` folder together when moving or sharing it; the executable depends on its accompanying cooked data and engine files.
- The existing `Build/Windows` archive was preserved. This separate verified package contains the current saved project and restored separate 3D kiosk parts. The packaged application has not received a manual playthrough.

The same Packaging > Windows operation in Unreal Editor can now be retried without the reported compiler error. No Editor restart, asset recreation or settings change is required for this fix.
