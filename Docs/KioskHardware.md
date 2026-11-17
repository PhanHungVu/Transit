# Separate kiosk hardware — 13 November 2026

> Updated on 16 November: the existing separate parts now form a lower integrated cabinet with a tilted world-space touchscreen and first-person free look. See [KioskPOV.md](KioskPOV.md) for the current implementation and validation; the dimensions below describe the earlier restoration.

The PDF migration's wide shallow cabinet and flat terminal/reader illustrations have been replaced with separate solid mesh components in the existing `BP_TransitKiosk`. The current touchscreen, translations, fares and transaction flows are preserved.

## Construction

- Restored the original `MachineBase` dimensions (45 × 110 × 190 cm), position and Grey material from the pre-migration backup. Original component names are retained.
- Restored the screen frame/display depth and height. Their widths are 104/96 cm so the current 960:700 touchscreen fills its display without distortion.
- Restored the original side-mounted `PaymentReader` position and dimensions, with a supporting bracket, raised display and twelve individually modeled keypad keys. The keypad is visual detail; clicking the terminal still simulates payment.
- Restored the original Help button position and dimensions, with a raised mount, solid yellow material and a readable HELP label.
- The transit-card reader is a protruding purple housing with a separate dark pad, indicator and text. The ticket/card collection slot has four frame pieces, a set-back interior and a projecting tray.
- Removed the `Design*Face` plane components from the actor. Historical artwork assets remain available in Content but are no longer used as its hardware faces. All current physical parts are ordinary editable Static Mesh components; small labels use Text Render components.
- Reused original material assets without modifying them. New detail materials use ordinary lit solid colors rather than images of complete devices.

The existing projected controls follow the real `ScreenDisplay`, `PaymentReader`, `HelpButton` and `TransitCardReader` bounds. The temporary kiosk camera is centered on the restored cabinet and input/view restoration remains in the existing controller.

## Files

Updated `BP_TransitKiosk`, its saved instance in `Demo`, native hardware automation, the camera framing and the existing rendered smoke test. Added solid detail materials under `Content/Kiosk/Design`. `Scripts/kiosk.ps1 -Mode Hardware` runs the hardware-only update, with timestamped package backups. It preserves the touchscreen and configured demonstration fares/balance defaults. Full `Migrate` still deliberately regenerates the complete reference design.

The pre-change files are backed up under `Saved/KioskHardwareBackup/20261113_093917`, with further package copies under `Saved/KioskMigrationBackup`. `Saved/KioskOriginalHardwareInspection.txt` records the actual pre-migration mesh transforms/materials read through Unreal.

## Validation

- Development Editor / Win64 and Development Game / Win64 builds passed. Logs: `Saved/Logs/KioskHardwareBuild.log` and `KioskHardwareGameBuild.log`.
- Unreal saved the Blueprint/map with zero commandlet errors or warnings. A fresh read-only reload found **31 solid cube mesh components and zero old flat hardware face components**; the main-body transform/material exactly match the pre-migration inspection. Geometry summary: `Saved/KioskSeparateHardwareVerification.json`.
- **61 rendered gameplay checks passed, zero failed**, using a real DirectX 11 1280×720 viewport. These include the projected screen and all three physical control regions, E/proximity, payment, card reading, repeat Help, retained day-pass ownership, camera/input restoration, cancelled payment and balance updates. These are automated checks, not a manual mouse/touch session. Report: `Saved/KioskGameplayVerification.txt`; log: `Saved/Logs/KioskSeparateHardwareGameplay.log`.
- The touchscreen asset's SHA-256 is **identical to the start of this hardware change**. Its layouts, localization and transaction code were not regenerated.
- Front and angled previews were visually reviewed: `Saved/KioskGameplayWelcome.png` and `Saved/KioskHardwarePerspective.png`. The angled view shows the cabinet depth, terminal bracket, protruding Help/card reader and recessed collection tray.

Open the existing Demo map, press Play, approach the machine and press E. No manual component construction, widget wiring or actor replacement is required. A complete Windows Shipping package was subsequently verified while fixing the Shipping-only test-code compile error; see [KioskPackaging.md](KioskPackaging.md) for the executable and build results.
