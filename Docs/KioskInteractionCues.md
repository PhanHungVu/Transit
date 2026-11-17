# Interaction lights and required card/ticket pickup — 17 November 2026

> Follow-up: [KioskCustomerFeedback.md](KioskCustomerFeedback.md) adds ticket quantity, removes the day-pass purchase restriction, adds animated Help feedback and moves gold pickup highlights to the machine assemblies. The earlier validation below is retained as history.

This update extends [KioskScalePickup.md](KioskScalePickup.md). Pickup reminders now appear immediately, apply to transit cards as well as printed tickets, and lock every machine control until collection. The 192 cm cabinet, separate physical parts, first-person camera, input mappings and existing touchscreen pages are retained.

## Physical cues

| Situation | Visible cue |
| --- | --- |
| Card payment is ready | Payment reader's small screen lights cyan, with dark readable text; cyan outline around the terminal |
| Transit card needs to be placed | Cyan outline around the physical transit-card reader/holder |
| Ticket is ready to collect | Gold outline follows the ticket's exposed face in the dispenser |
| Transit card must be collected | Gold outline around the actual card on the holder |
| Processing, idle, or an action is unavailable | Its action cue is off; payment screen restores its original material |
| Item has been collected | Item, pickup outline and item collision are removed together |

The outlines use four small emissive edges aligned to each real mesh face, including the tilted card. These edges have no collision and use the scene's normal depth test. They do not change global post-process/stencil settings or interfere with the click ray. A new editable `M_KioskInteractionGlow` material compensates for eye adaptation so the cue remains visible in the existing lighting.

## Pickup behavior

The transit card stays physically on its holder throughout amount selection and payment. It is needed during that stage, so placement does not immediately demand that it be removed. As soon as top-up approval finishes, the card becomes collectible and the card reminder appears automatically. Leaving amount selection or attempting to exit with an unpaid card also starts collection. Cancelling while a top-up is processing cancels its timer and requires the card to be picked up; no late credit is applied.

Printed one-way, round-trip and day-pass tickets immediately show the ticket reminder when dispensing succeeds. The existing click-first warning flag has been removed; the reminder follows the session's pending-collection state directly.

During either reminder, **touchscreen buttons, payment, card placement, Help/F1 and exit/Escape are locked**. The player can still look around and click the outlined item. Payment completion takes priority over an already open Help popup, ensuring that pickup is always shown immediately. This deliberately replaces the previous Help exception in response to the latest request to lock the entire machine during pickup.

An actual camera ray must hit `TransitCardProp` or `CollectionItem` to collect it. A reminder button cannot simulate pickup. Collection clears the corresponding state, hides that item and outline, disables its collision, and restores the ordinary screen/controls. The successful transaction receipt remains available; pickup neither charges again nor changes the approved balance.

Pending items survive forced controller cleanup/reopening while the kiosk actor still exists. An uncollected card is protected from resets even before payment. Restarting Play ends the demo session; this does not add disk persistence or an inventory system.

## Assets and implementation

- `UTransitKioskSession` owns card presence, pending card/ticket pickup and transaction rules.
- `UTransitKioskWidget` automatically displays the existing editable collection layer with the correct card/ticket wording and four-language translations. It notifies its physical kiosk when presentation changes.
- `ATransitKioskActor` owns the small screen material override, collision-free outline edges and physical pickups. Existing card/ticket mesh components are reused.
- `ATransitPlayerController` permits only a valid physical pickup while locked, preserving mouse look and restoring regular input afterward.
- The focused native editor `-UpdateInteraction` command updates only the material references and existing reminder text through supported Unreal Blueprint/package APIs. It preserves the component transforms, materials elsewhere, page layouts and configurable demo values. `Scripts/kiosk.ps1 -Mode Interaction` exposes this operation for future use; running it again is not needed to play.
- `TransitKioskText.cpp`, `Content/Kiosk/DesignSource/Localization.json` and the existing extraction/generation script contain matching pickup translations. No PDF extraction or UI rebuild was needed.

Saved assets: existing `BP_TransitKiosk`, `KioskUI`, `Demo` map, plus new `M_KioskInteractionGlow`. The character and input asset hashes match `Saved/KioskInteractionPreservedFiles.json`. Recovery copies are under `Saved/KioskInteractionBackup/20261117_095735` and the editor tool's timestamped `Saved/KioskInteractionAssetBackup` folder.

## Validation

Development Editor compilation and the focused asset update succeeded. The native transaction/UI suite passed **2,309 checks, zero failures** (`Saved/KioskInteractionUIVerification.txt`, `Saved/Logs/KioskInteractionUI.log`). The 1920 × 1080 real-input run passed **222 checks, zero failures** (`Saved/KioskInteractionGameplayHDVerification.txt`, `Saved/Logs/KioskInteractionGameplay.log`).

These cover illumination state, immediate card/ticket reminders, locked screen/Help/shortcuts/exit, real physical collection, no item respawn, cancellation without late credit, repeated top-ups, preserved balance/day-pass restrictions, forced cleanup/reopening, and all five existing simulated eye-height profiles.

The first payment screenshot caught the new shader compiling; the capture helper now waits for shader completion before recording evidence. The final **1280 × 720** run also passed **222 checks, zero failures** (`Saved/KioskInteractionGameplay720Verification.txt`, `Saved/Logs/KioskInteractionGameplay720.log`). Its payment capture visibly shows the cyan screen with readable TAP CARD text and cyan terminal outline, with no checkerboard fallback. The final card/ticket outlines and multilingual reminders were inspected for readability and clipping.

Representative final captures under `Saved`: `KioskInteraction_PaymentReady.png`, `KioskInteraction_PlaceCard.png`, `KioskInteraction_CardOutline.png`, `KioskInteraction_TicketOutline.png`, and `KioskInteraction_Card_<language>.png` / `KioskInteraction_Ticket_<language>.png`. The earlier full-HD captures are preserved in `Saved/KioskInteractionPreviewHD`; the payment screenshot in that earlier folder predates shader completion and is superseded by the final payment capture.

Windows Shipping compilation, cooking of **605 packages**, Pak/IoStore staging and archiving all completed with **exit code 0**. Log: `Saved/Logs/KioskInteractionPackage.log` (PowerShell UTF-16). Cooking reported zero errors and the same two nonblocking `AsyncIoDelete` warnings about existing cook-cache directories. The new glow material was cooked and included. No manual cache deletion or global configuration change was required.

The updated launcher is **`Saved/Packaged/POVVerified/Windows/Transit.exe`**. Keep the entire `Windows` folder together when copying/sharing it. A hidden/offscreen startup test of the new Shipping game stayed running for 15 seconds, after which only that test process was stopped (`Saved/KioskInteractionPackagedStartup.json`). This verifies packaged startup; the full interaction checks above ran in the installed engine. The older `Build/Windows` and `Saved/Packaged/ShippingVerified` archives were preserved.

## Use

Open `/Game/Kiosk/Maps/Demo`, Play, approach the kiosk and press E. Follow the cyan cue to place a transit card or tap the payment reader. When the pickup reminder appears, look down and left-click the gold-outlined ticket/card. The reminder disappears and controls resume. No manual Blueprint, widget or input setup is required.

The installed-engine playthrough uses automated real camera rays and input events. Manual comfort/readability review and a complete manual packaged playthrough remain separate checks.
