# Character scale and physical ticket collection — 16 November 2026

> The 17 November [interaction-cues update](KioskInteractionCues.md) supersedes the pickup behavior below: reminders are immediate, transit cards require physical collection too, and every machine control is locked during pickup. The dimensions in this report remain current.

This update covers the two requested changes: fitting the kiosk to the existing player and requiring collection of printed tickets. It builds on the first-person/world-space implementation in [KioskPOV.md](KioskPOV.md).

## Measured scale

The audit ran the existing Demo map with its actual `BP_FirstPersonCharacter`, rather than using the collision capsule as a literal human height. Its `SKM_Manny_Simple` mesh bounds measure **178.50 cm** in the sampled pose, the collision capsule is **192 cm**, and the first-person camera is **159.78 cm** above the capsule's floor in the comparison fixture. Mesh bounds vary with animation; the capsule includes collision clearance.

The old 144 cm cabinet looked small beside that character. The revised cabinet is **192 cm tall**, slightly taller than the visible adult character. This is a prototype design choice based on the actual actor and side-by-side render, not a claim about an official MRT machine's dimensions.

| Measurement | Before | After |
| --- | ---: | ---: |
| Cabinet height | 144 cm | 192 cm |
| Cabinet width × depth | 112 × 45 cm | 112 × 45 cm |
| Active screen width × height | 48 × 35 cm | 55 × 40.10 cm |
| Screen mesh center above kiosk floor | 105.83 cm | 125.83 cm |
| Reader assembly center | 86 cm | 98 cm |
| Collection opening center | 85 cm | 96 cm |
| Help button center | 93 cm | 107 cm |
| Character visible mesh bounds | 178.50 cm | 178.50 cm |
| Character capsule height | 192 cm | 192 cm |

The cabinet, sign, screen and separate hardware components were resized/repositioned inside the existing `BP_TransitKiosk`. The actor origin, map placement, character scale, input mapping, card dimensions, screen aspect ratio and 12-degree screen tilt are retained. The world-space widget continues to derive alignment from the actual display mesh. No replacement character or kiosk actor was introduced.

Comparison captures: `Saved/KioskScaleBefore.png`, `Saved/KioskScaleAfter.png`. Measurements: `Saved/KioskScaleBefore.txt`, `Saved/KioskScaleAfter.txt`. Character and input asset hashes still match `Saved/KioskScalePreservedCharacter.json`.

## Collection rule

- Successful one-way, round-trip and day-pass payments set `bTicketAwaitingCollection` in the existing session. Top-ups do not print a ticket.
- The physical `CollectionItem` stays visible and clickable while that state is true. Its visibility no longer depends on the currently shown popup.
- Return, Back/navigation, exit/Escape and card/payment shortcuts cannot dismiss or reset an uncollected ticket. An attempted action shows an editable, localized collection warning on the touchscreen.
- The physical Help button/F1 remains available. Staff-close still dismisses Help; returning from Help preserves the pending ticket and warning.
- The player looks down at the dispenser and left-clicks the actual ticket. The camera ray must hit that ticket. Only successful collection clears the state, removes the prop/collision and dismisses the warning. It does not charge again or reset the receipt.
- After pickup, the player can return to Welcome or exit normally. A collected ticket does not reappear while the Success screen remains open.
- Engine teardown may force input/camera cleanup, but forced close/reopen of a living kiosk preserves its pending ticket. Ordinary player exit remains blocked until pickup. Stopping Play or restarting the game ends the demo session; no save-game persistence is added.

`UTransitKioskSession` owns the state; the widget gates actions and presents the warning; the actor presents/collects the prop; the controller routes the physical hit and protects the close path. There is no duplicate inventory or separate ticket-state flag on the actor.

## Saved assets and source

Unreal's native Blueprint compiler and package-saving APIs successfully updated the existing `BP_TransitKiosk`, `KioskUI` and `Demo` map. The UI change adds only `CollectionWarning` and its localized text children to the existing widget tree; it preserves the seven screens, five existing popups and their artwork. Warning translations reuse the existing four-language composite font, including the small aiming hint. `TransitKioskText.cpp`, `Localization.json` and its extraction/generation source contain the same new translation keys.

Source changes are in `TransitKioskActor`, `TransitKioskSession`, `TransitKioskWidget`, `TransitPlayerController`, the native editor `TransitKioskDesign`, and their existing tests. Recovery copies made before this update are under `Saved/KioskScalePickupBackup/20261116_220204`; the editor migration also keeps timestamped package backups.

## Verification

The Development Editor build passed. The final real-input gameplay run at **1920 × 1080** contains **211 passing checks, zero failures**, including Return/Escape/device blocking, physical pickup, no respawn, Help reopening, forced close/reopen preservation, all transaction types and five simulated eye heights (105, 110, 135, 165 and 180 cm). Report: `Saved/KioskScalePickupVerification.txt`; log: `Saved/Logs/KioskScalePickupGameplayHD.log`.

The existing native transaction and saved-widget regression suite passed **2,037 checks, zero failures**, including all station fares, four languages, six top-ups, payment timers, Help, day-pass restrictions and the collection gate. Report: `Saved/KioskScalePickupUIVerification.txt`; log: `Saved/Logs/KioskScalePickupUIRegression.log`.

All four localized collection-warning screenshots were inspected: `Saved/KioskPickup_English.png`, `KioskPickup_Chinese.png`, `KioskPickup_Malay.png`, `KioskPickup_Tamil.png`. A first render exposed missing Tamil glyphs in the small aiming hint; reusing the existing multilingual font fixed them. The final screenshots contain the correct glyphs and no clipped warning text. `ForceRes` prevents Unreal from silently shrinking the requested test viewport to another monitor's work area; the log and screenshot confirm the final 1920 × 1080 size.

Windows Shipping compiled, cooked all 604 discovered packages, staged Pak/IoStore data and archived successfully with **exit code 0**. Log: `Saved/Logs/KioskScalePickupPackage.log` (PowerShell UTF-16). The cook reported zero errors and two nonblocking `AsyncIoDelete` warnings about existing cook-cache directories; the new kiosk/UI packages were written and included successfully. No manual cache deletion or global setting change was required.

The updated launcher is **`Saved/Packaged/POVVerified/Windows/Transit.exe`**. Keep the complete `Windows` folder together when sharing it. Older `Build/Windows` and `Saved/Packaged/ShippingVerified` archives remain separate. The new Shipping game stayed running for a 15-second hidden/offscreen startup check, after which only that test process was stopped. Evidence: `Saved/KioskScalePickupPackagedStartup.json`. This confirms packaged startup; the full interaction playthrough was automated in the installed engine rather than manually performed in Shipping.

## Using the update

Open `/Game/Kiosk/Maps/Demo` and Play. Approach the machine, press E, purchase a ticket, then try Return or Escape to see the warning. Look down and click the ticket in the dispenser; navigation and exit then work. Help remains usable during the warning. No manual UMG construction, actor replacement, character scaling or input assignment is needed.

Automated rays and renders verify the implementation, not physical ergonomic comfort or accessibility certification. The cabinet is intentionally taller for this adult character; shorter/seated profiles are simulated camera tests. A manual comfort/readability review in the intended display setup remains useful.
